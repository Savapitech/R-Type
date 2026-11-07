from __future__ import annotations

import re
from pathlib import Path

from PySide6.QtCore import QObject, QRect, Qt, Signal
from PySide6.QtGui import QImage, QPixmap

from . import jsonio

BLUEPRINT_DIRS = ("assets/descriptionEntity", "assets/powerUp")
SCREEN_W, SCREEN_H = 1280, 720

KIND_TERRAIN = "terrain"
KIND_ENEMY = "enemy"
KIND_POWERUP = "powerup"
KIND_BACKGROUND = "background"
KIND_OTHER = "other"


def blank_stage(name: str = "New stage") -> dict:
    return {"name": name, "nextStage": "", "duration": 0.0, "startX": 0.0,
            "scrollSpeed": 50.0, "randomSpawns": True,
            "segments": [], "spawns": []}


def blueprint_kind(data: dict) -> str:
    comps = {k.lower(): v for k, v in data.get("components", {}).items()}
    layer = comps.get("collider", {}).get("layer")
    if layer in ("Environment", "BossRoom"):
        return KIND_TERRAIN
    if "parallax" in comps:
        return KIND_BACKGROUND
    if layer == "PowerUp" or "powerup" in comps:
        return KIND_POWERUP
    if layer == "Enemy" or "enemy" in comps:
        return KIND_ENEMY
    return KIND_OTHER


def component(data: dict, name: str) -> dict:
    for key, value in data.get("components", {}).items():
        if key.lower() == name.lower() and isinstance(value, dict):
            return value
    return {}


def animation_rects(anim: dict) -> list[QRect]:
    fw, fh = int(anim.get("frameWidth", 0)), int(anim.get("frameHeight", 0))
    total = max(1, int(anim.get("totalFrames", 1)))
    cols = int(anim.get("columns", 0))
    sx, sy = int(anim.get("startX", 0)), int(anim.get("startY", 0))
    px, py = int(anim.get("paddingX", 0)), int(anim.get("paddingY", 0))
    rects = []
    for i in range(total):
        col = i % cols if cols > 0 else i
        row = i // cols if cols > 0 else 0
        rects.append(QRect(sx + col * (fw + px), sy + row * (fh + py), fw, fh))
    return rects


class Project(QObject):
    blueprintsChanged = Signal()
    stagesChanged = Signal()

    def __init__(self, root: str | Path):
        super().__init__()
        self.root = Path(root).resolve()
        self._textures: dict[str, QImage | None] = {}
        self.blueprint_files: dict[str, str] = {}
        self.reload_blueprint_index()

    @property
    def manifest_path(self) -> Path:
        return self.root / "assets" / "blueprints.json"

    @property
    def stages_path(self) -> Path:
        return self.root / "assets" / "stages" / "stages.json"

    def rel(self, path: str | Path) -> str:
        path = Path(path)
        try:
            return path.resolve().relative_to(self.root).as_posix()
        except ValueError:
            return path.as_posix()

    def abs(self, rel: str) -> Path:
        return self.root / rel

    def reload_blueprint_index(self) -> None:
        files: dict[str, str] = {}
        if self.manifest_path.exists():
            manifest = jsonio.load_json(self.manifest_path)
            for entry in manifest.get("blueprints", []):
                files[entry["name"]] = entry["file"]
        else:
            for folder in BLUEPRINT_DIRS:
                for file in sorted((self.root / folder).glob("*.json")):
                    files.setdefault(file.stem, self.rel(file))
        self.blueprint_files = files

    def blueprint_names(self) -> list[str]:
        return list(self.blueprint_files)

    def load_blueprint(self, name: str) -> dict:
        return jsonio.load_json(self.abs(self.blueprint_files[name]))

    def blueprint_name_for_file(self, rel: str) -> str | None:
        for name, file in self.blueprint_files.items():
            if file == rel:
                return name
        return None

    def write_manifest(self) -> None:
        jsonio.save_json(self.manifest_path, {"blueprints": [
            {"name": n, "file": f} for n, f in self.blueprint_files.items()]})

    def register_blueprint(self, name: str, rel_file: str) -> None:
        self.blueprint_files[name] = rel_file
        self.write_manifest()
        self.blueprintsChanged.emit()

    def save_blueprint(self, name: str, data: dict, rel_file: str | None = None) -> str:
        rel_file = rel_file or self.blueprint_files.get(name) \
            or f"assets/descriptionEntity/{name}.json"
        jsonio.save_json(self.abs(rel_file), data)
        if self.blueprint_files.get(name) != rel_file:
            self.register_blueprint(name, rel_file)
        else:
                self.blueprintsChanged.emit()
        return rel_file

    def unique_blueprint_name(self, base: str) -> str:
        name, i = base, 2
        while name in self.blueprint_files or \
                self.abs(f"assets/descriptionEntity/{name}.json").exists():
            name, i = f"{base}_{i}", i + 1
        return name

    def load_stage_manifest(self) -> dict:
        if self.stages_path.exists():
            return jsonio.load_json(self.stages_path)
        return {"defaultStage": "", "stages": []}

    def save_stage_manifest(self, manifest: dict) -> None:
        jsonio.save_json(self.stages_path, manifest)
        self.stagesChanged.emit()

    def stage_ids(self) -> list[str]:
        return [s["id"] for s in self.load_stage_manifest().get("stages", [])]

    def next_stage_id(self) -> str:
        used = set(self.stage_ids())
        i = 1
        while f"stage{i}" in used or self.abs(f"assets/stages/stage{i}.json").exists():
            i += 1
        return f"stage{i}"

    def texture(self, rel: str) -> QImage | None:
        if rel not in self._textures:
            image = QImage(str(self.abs(rel))) if rel else QImage()
            self._textures[rel] = None if image.isNull() else image
        return self._textures[rel]

    def invalidate_texture(self, rel: str | None = None) -> None:
        if rel is None:
            self._textures.clear()
        else:
            self._textures.pop(rel, None)

    def crop(self, rel: str, rect: QRect) -> QPixmap | None:
        image = self.texture(rel)
        if image is None or rect.width() <= 0 or rect.height() <= 0:
            return None
        return QPixmap.fromImage(image.copy(rect))

    def sprite_frames(self, data: dict) -> tuple[list[QPixmap], float]:
        sprite = component(data, "sprite")
        tr = component(data, "transform")
        scale = tr.get("scale", {"x": 1.0, "y": 1.0})
        size = sprite.get("size", {})
        tex = sprite.get("texture", "")
        anim = component(data, "animation")
        rects = animation_rects(anim) if anim.get("frameWidth") else []
        if not rects:
            r = sprite.get("textureRect")
            if r:
                rects = [QRect(int(r["x"]), int(r["y"]), int(r["w"]), int(r["h"]))]
        out: list[QPixmap] = []
        if rects and tex:
            tw = max(1.0, float(size.get("w", rects[0].width())) * float(scale.get("x", 1.0)))
            th = max(1.0, float(size.get("h", rects[0].height())) * float(scale.get("y", 1.0)))
            for r in rects:
                pix = self.crop(tex, r)
                if pix is not None:
                    out.append(pix.scaled(int(round(tw)), int(round(th)),
                                          Qt.IgnoreAspectRatio, Qt.FastTransformation))
        return out, float(anim.get("frameDuration", 0.1)) if anim else 0.0

    def entity_size(self, data: dict) -> tuple[float, float]:
        sprite = component(data, "sprite")
        scale = component(data, "transform").get("scale", {})
        size = sprite.get("size", {})
        coll = component(data, "collider")
        w = float(size.get("w", coll.get("width", 32.0))) * float(scale.get("x", 1.0))
        h = float(size.get("h", coll.get("height", 32.0))) * float(scale.get("y", 1.0))
        return w, h


_ID_RE = re.compile(r"^[A-Za-z0-9_\-]+$")


def valid_identifier(name: str) -> bool:
    return bool(_ID_RE.match(name))
