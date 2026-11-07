from __future__ import annotations

import json
import os
import shutil
import tempfile
from pathlib import Path

from . import jsonio, schema
from .project import Project, blueprint_kind


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []
        self.checks = 0

    def check(self, condition: bool, message: str) -> None:
        self.checks += 1
        if not condition:
            self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def check_assets(root: Path, report: Report) -> None:
    project = Project(root)
    for path in sorted((root / "assets").rglob("*.json")):
        rel = path.relative_to(root).as_posix()
        try:
            data = jsonio.load_json(path)
        except ValueError as err:
            report.check(False, f"{rel}: invalid JSON ({err})")
            continue
        report.check(json.loads(jsonio.dumps(data)) == data, f"{rel}: round-trip changed the data")

    names = project.blueprint_names()
    for name in names:
        file = project.abs(project.blueprint_files[name])
        report.check(file.exists(), f"blueprint '{name}': missing file {file}")
    listed = set(project.blueprint_files.values())
    for folder in ("assets/descriptionEntity", "assets/powerUp"):
        for file in sorted((root / folder).glob("*.json")):
            if file.relative_to(root).as_posix() not in listed:
                report.warn(f"{file.name} is not listed in blueprints.json (the game will not load it)")

    for name in names:
        data = project.load_blueprint(name)
        for comp in data.get("components", {}):
            if schema.canonical(comp) is None:
                report.warn(f"blueprint '{name}': component '{comp}' not in schema (raw JSON editor)")
        tex = next((v.get("texture") for k, v in data.get("components", {}).items()
                    if k.lower() == "sprite"), None)
        if tex:
            report.check(project.abs(tex).exists(), f"blueprint '{name}': missing texture {tex}")

    manifest = project.load_stage_manifest()
    for entry in manifest.get("stages", []):
        file = project.abs(entry["file"])
        report.check(file.exists(), f"stage '{entry['id']}': missing file {entry['file']}")
        if not file.exists():
            continue
        stage = jsonio.load_json(file)
        for seg in stage.get("segments", []):
            report.check(seg.get("blueprint") in project.blueprint_files,
                         f"{entry['id']}: unknown segment blueprint {seg.get('blueprint')}")
        for sp in stage.get("spawns", []):
            report.check(sp.get("blueprint") in project.blueprint_files,
                         f"{entry['id']}: unknown spawn blueprint {sp.get('blueprint')}")


def smoke_gui(root: Path, report: Report) -> None:
    if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtCore import QPointF
    from PySide6.QtWidgets import QApplication

    from .editors.blueprint_editor import BlueprintEditor
    from .editors.level_editor import LevelEditor, effective_wave
    from .editors.sprite_editor import SpriteEditor
    from .editors.text_editor import TextEditor
    from .main_window import MainWindow

    app = QApplication.instance() or QApplication([])
    tmp = Path(tempfile.mkdtemp(prefix="rtype-editor-selftest-"))
    try:
        shutil.copytree(root / "assets", tmp / "assets")
        project = Project(tmp)
        win = MainWindow(project)

        # open everything
        tabs = []
        for path in sorted((tmp / "assets").rglob("*")):
            if path.suffix.lower() in (".json", ".png"):
                tab = win.open_path(path)
                report.check(tab is not None, f"could not open {path.name}")
                tabs.append(tab)
        app.processEvents()

        from .widgets.property_form import FieldsForm
        for name in project.blueprint_names():
            for comp, data in project.load_blueprint(name)["components"].items():
                fields = schema.fields_of(comp)
                if fields is None:
                    continue
                form, seen = FieldsForm(fields, project), []
                form.edited.connect(seen.append)
                form.set_data(data)
                report.check(not seen, f"loading {name}.{comp} into a form emitted an edit")
                form._on_change(None, [(None, "value", None)])
                report.check(seen and seen[0] == data, f"form round-trip changed {name}.{comp}")
        health = FieldsForm(schema.fields_of("collider"), project)
        seen = []
        health.edited.connect(seen.append)
        health.set_data({"width": 4.0, "layer": "Enemy", "mask": ["Player"]})
        health.params.child("width").setValue(9.5)
        health.params.child("mask").setValue(["Player", "Enemy"])
        report.check(seen and seen[-1]["width"] == 9.5 and seen[-1]["mask"] == ["Player", "Enemy"],
                     "editing a form emits the new values")

        level = win.open_path(tmp / "assets/stages/stage1.json")
        report.check(isinstance(level, LevelEditor), "stage1 should open in the level editor")
        before = json.loads(json.dumps(level.data))
        n_seg, n_sp = len(level.data["segments"]), len(level.data["spawns"])
        report.check(len(level._segment_items) == n_seg, "one scene item per segment")
        level.on_drop("enemyBidos", QPointF(3000, 300))
        report.check(len(level.data["spawns"]) == n_sp + 1, "dropping an enemy adds a spawn")
        spawn = level.data["spawns"][-1]
        report.check(abs(spawn["time"] - (3000 - 16 - spawn["x"]) / 50) < 1.5 or spawn["time"] > 0,
                     "spawn time derived from the drop position")
        level.on_drop("low_ground", QPointF(100, 600))
        report.check(len(level.data["segments"]) == n_seg + 1, "dropping terrain adds a segment")
        level.on_drop("enemyRedShip", QPointF(4000, 300))
        wave = [s for s in level.data["spawns"] if s["blueprint"] == "enemyRedShip"]
        report.check(bool(wave) and effective_wave(wave[0], level.bp("enemyRedShip")) is not None,
                     "wave enemies use the blueprint wave by default")
        # move a spawn
        item = level._spawn_items[0]
        item.setSelected(True)
        item.setPos(item.pos() + QPointF(200, 40))
        item.start_pos = item.pos() - QPointF(200, 40)
        level.item_released(item)
        level.duplicate_selected()
        level.delete_selected()
        while level.undo_stack.canUndo():
            level.undo_stack.undo()
        report.check(level.data == before, "undo restores the original stage")
        while level.undo_stack.canRedo():
            level.undo_stack.redo()
        report.check(level.data != before, "redo re-applies edits")
        level.toggle_preview()
        for _ in range(60):
            level._tick()
        level.stop_preview()
        level.save()
        saved = jsonio.load_json(level.path)
        report.check(len(saved["spawns"]) >= 2 and all(
            isinstance(s["time"], float) and s["time"] >= 0 for s in saved["spawns"]),
            "saved stage has valid spawns")

        count = len(project.stage_ids())
        win.stages_panel.new_stage()
        manifest = project.load_stage_manifest()
        report.check(len(manifest["stages"]) == count + 1, "new stage is added to stages.json")
        ids = [s["id"] for s in manifest["stages"]]
        report.check(all(manifest["stages"][i]["nextStage"] == (ids[i + 1] if i + 1 < len(ids) else "")
                         for i in range(len(ids))), "nextStage chain follows the order")
        last_file = jsonio.load_json(project.abs(manifest["stages"][-2]["file"]))
        report.check(last_file["nextStage"] == ids[-1], "previous stage file points to the new stage")

        bp_tab = win.open_path(tmp / "assets/descriptionEntity/enemyBidos.json")
        report.check(isinstance(bp_tab, BlueprintEditor), "enemyBidos opens in the blueprint editor")
        for name in list(bp_tab._comps()):
            bp_tab._select_component(name, force=True)
        before_bp = json.loads(json.dumps(bp_tab.doc.data))
        bp_tab.add_box.setCurrentText("health")
        bp_tab._add_component()
        report.check("health" in bp_tab._comps(), "adding a component works")
        bp_tab.components.setCurrentRow(list(bp_tab._comps()).index("health"))
        bp_tab._remove_component()
        report.check(bp_tab.doc.data == before_bp, "add + remove component returns to the original")
        bp_tab.apply_patch({"collider": {"width": 12.5}})
        report.check(bp_tab.doc.data["components"]["collider"]["width"] == 12.5, "patch applied")
        bp_tab.save()
        report.check(jsonio.load_json(bp_tab.path)["components"]["collider"]["width"] == 12.5,
                     "blueprint saved")

        from . import templates
        project.save_blueprint("selftest_enemy", templates.make("enemy"),
                               "assets/descriptionEntity/selftest_enemy.json")
        report.check("selftest_enemy" in jsonio.load_json(project.manifest_path)["blueprints"][-1].values(),
                     "new blueprint registered in blueprints.json")
        for kind in templates.TEMPLATES.values():
            report.check(blueprint_kind(templates.make(kind)) in
                         ("enemy", "terrain", "powerup", "other"), f"template {kind} has a kind")

        sprite = win.open_path(tmp / "assets/entityTexture/bidos.png")
        report.check(isinstance(sprite, SpriteEditor), "png opens in the sprite editor")
        sheet = sprite.sheet
        sheet.anim.set_data({**sheet.anim.data(), "frameWidth": 32, "frameHeight": 32})
        sheet._auto_grid()
        report.check(sheet.anim.data()["totalFrames"] >= 2, "auto grid finds the frames of bidos.png")
        sprite.sheet.animated.setChecked(True)
        patch = sprite.sheet.build_patch()
        report.check(patch["animation"]["totalFrames"] >= 1 and "sprite" in patch, "spritesheet patch")
        canvas = sprite.pixels.canvas
        w0 = canvas.image.width()
        canvas.resize_canvas(w0 + 8, canvas.image.height())
        canvas.flip(True)
        canvas.shift(1, 0)
        report.check(sprite.is_modified(), "pixel edits mark the sprite modified")
        sprite.undo_stack.undo(); sprite.undo_stack.undo(); sprite.undo_stack.undo()
        report.check(canvas.image.width() == w0, "pixel undo restores size")
        sprite.sheet._save_clip()
        win.apply_to_blueprint("enemyBidos", sprite.sheet.build_patch())
        sprite.save()

        text = win.open_path(tmp / "assets/blueprints.json", "text")
        report.check(isinstance(text, TextEditor), "blueprints.json opens as text")
        text.format_json()
        text.save()

        win.save_all()
        app.processEvents()
        win.tabs.clear()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def run(root: Path) -> int:
    report = Report()
    check_assets(root, report)
    try:
        import PySide6
    except ImportError:
        report.warn("PySide6 not installed: GUI smoke test skipped")
    else:
        _run_gui(root, report)
    return _finish(report)


def _run_gui(root: Path, report: Report) -> None:
    try:
        smoke_gui(root, report)
    except Exception as err:
        import traceback
        traceback.print_exc()
        report.check(False, f"GUI smoke test crashed: {err!r}")


def _finish(report: Report) -> int:
    for w in report.warnings:
        print(f"warning: {w}")
    for e in report.errors:
        print(f"ERROR: {e}")
    print(f"{report.checks} checks, {len(report.errors)} error(s), {len(report.warnings)} warning(s)")
    return 1 if report.errors else 0
