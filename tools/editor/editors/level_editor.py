from __future__ import annotations

import copy
import math
from pathlib import Path

from PySide6.QtCore import QMimeData, QPointF, QRectF, Qt, QTimer, Signal
from PySide6.QtGui import QBrush, QColor, QIcon, QPainterPath, QPen, QPixmap
from PySide6.QtWidgets import (QAbstractItemView, QCheckBox, QGraphicsItem, QGraphicsPathItem,
                               QGraphicsPixmapItem, QGraphicsRectItem, QGraphicsScene,
                               QGraphicsSimpleTextItem, QGraphicsView, QGroupBox, QLabel,
                               QScrollArea, QSpinBox, QSplitter, QToolBar, QTreeWidget,
                               QTreeWidgetItem, QVBoxLayout, QWidget)

from .. import jsonio, schema
from ..project import (KIND_ENEMY, KIND_OTHER, KIND_POWERUP, KIND_TERRAIN, SCREEN_H, SCREEN_W,
                       Project, blueprint_kind, component)
from ..widgets.property_form import FieldsForm
from ..widgets.zoom_view import ZoomView
from .base import DocumentTab

MIME = "application/x-rtype-blueprint"
DEFAULT_SPAWN_X = 1300.0
RULER_Y = -70
PALETTE_GROUPS = {KIND_TERRAIN: "Terrain", KIND_ENEMY: "Enemies", KIND_POWERUP: "Power-ups",
                  KIND_OTHER: "Other"}

STAGE_FIELDS = [
    schema.field("name", "str", "Stage"),
    schema.field("nextStage", "enum", "", options=[""]),
    schema.field("duration", "float", 0.0),
    schema.field("startX", "float", 0.0),
    schema.field("scrollSpeed", "float", 50.0),
    schema.field("randomSpawns", "bool", True),
]
SEGMENT_FIELDS = [schema.field("blueprint", "blueprint", ""),
                  schema.field("spacing", "float", 128.0),
                  schema.field("symmetric", "bool", False)]
SPAWN_FIELDS = [schema.field("blueprint", "blueprint", ""),
                schema.field("time", "float", 0.0),
                schema.field("x", "float", DEFAULT_SPAWN_X),
                schema.field("y", "float", 0.0)]
WAVE_FIELD = schema.group("wave", [("amplitude", "float", 120.0), ("frequency", "float", 2.5)])


def normalize_stage(data: dict) -> dict:
    for key, value in (("name", "Stage"), ("nextStage", ""), ("duration", 0.0), ("startX", 0.0),
                       ("scrollSpeed", 50.0), ("randomSpawns", True), ("segments", []),
                       ("spawns", [])):
        data.setdefault(key, value)
    return data


def effective_wave(spawn: dict, bp: dict) -> dict | None:
    """Wave parameters a spawn will use: its own override, else the blueprint's."""
    base = component(bp, "waveMovement")
    if not base:
        return None
    wave = spawn.get("wave") or {}
    return {"amplitude": float(wave.get("amplitude", base.get("amplitude", 120.0))),
            "frequency": float(wave.get("frequency", base.get("frequency", 2.5)))}


class _DragItem(QGraphicsPixmapItem):
    """Selectable, movable scene item that reports when a drag ends."""

    def __init__(self, editor: "LevelEditor", index: int, pixmap: QPixmap, z: int):
        super().__init__(pixmap)
        self.editor, self.index, self.start_pos = editor, index, QPointF()
        self.setFlags(QGraphicsItem.ItemIsSelectable | QGraphicsItem.ItemIsMovable
                      | QGraphicsItem.ItemSendsGeometryChanges)
        self.setZValue(z)

    def moved(self) -> bool:
        return (self.pos() - self.start_pos).manhattanLength() > 0.01

    def mousePressEvent(self, event):
        self.start_pos = self.pos()
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        super().mouseReleaseEvent(event)
        self.editor.item_released(self)


class SegmentItem(_DragItem):
    """Terrain piece, free to move along x only."""

    def __init__(self, editor, index: int, pixmap: QPixmap, base_y: float, symmetric: bool):
        super().__init__(editor, index, pixmap, 0)
        self.base_y = base_y
        if symmetric:
            QGraphicsPixmapItem(pixmap, self).setPos(0, -base_y)
        outline = QGraphicsRectItem(QRectF(pixmap.rect()), self)
        outline.setPen(QPen(QColor(120, 200, 255, 70), 0))

    def itemChange(self, change, value):
        if change == QGraphicsItem.ItemPositionChange:
            return QPointF(value.x(), self.base_y)
        return super().itemChange(change, value)


class SpawnItem(_DragItem):
    def __init__(self, editor, index: int, frames: list[QPixmap], anchor: QPointF, name: str):
        super().__init__(editor, index, frames[0], 2)
        self.frames, self.anchor = frames, anchor
        label = QGraphicsSimpleTextItem(name, self)
        label.setBrush(QBrush(QColor(255, 220, 120)))
        label.setPos(0, -16)

    def anchor_pos(self) -> QPointF:
        return self.pos() + self.anchor


class LevelView(ZoomView):
    blueprintDropped = Signal(str, QPointF)
    deleteRequested = Signal()
    duplicateRequested = Signal()

    def __init__(self, scene):
        super().__init__(scene)
        self.setAcceptDrops(True)
        self.setDragMode(QGraphicsView.RubberBandDrag)

    def dragEnterEvent(self, event):
        if event.mimeData().hasFormat(MIME):
            event.acceptProposedAction()

    dragMoveEvent = dragEnterEvent

    def dropEvent(self, event):
        if event.mimeData().hasFormat(MIME):
            name = bytes(event.mimeData().data(MIME)).decode("utf-8")
            self.blueprintDropped.emit(name, self.mapToScene(event.position().toPoint()))
            event.acceptProposedAction()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Delete:
            self.deleteRequested.emit()
        elif event.key() == Qt.Key_D and event.modifiers() & Qt.ControlModifier:
            self.duplicateRequested.emit()
        else:
            super().keyPressEvent(event)


class Palette(QTreeWidget):
    def __init__(self):
        super().__init__()
        self.setHeaderHidden(True)
        self.setDragEnabled(True)
        self.setDragDropMode(QAbstractItemView.DragOnly)
        self.setIconSize(self.iconSize() * 2)

    def mimeData(self, items):
        data = QMimeData()
        names = [i.data(0, Qt.UserRole) for i in items if i.data(0, Qt.UserRole)]
        if names:
            data.setData(MIME, names[0].encode("utf-8"))
        return data


class LevelEditor(DocumentTab):
    def __init__(self, path: Path, project: Project, parent=None):
        self.previewing = False
        self._bp_cache: dict[str, dict | None] = {}
        self._selection: tuple[list[int], list[int]] = ([], [])
        self._rebuilding = False
        self._spawn_items: list[SpawnItem] = []
        self._segment_items: list[SegmentItem] = []
        self._cam_item: QGraphicsRectItem | None = None
        self._cam_t = 0.0
        super().__init__(path, project, parent)

        self.scene = QGraphicsScene(self)
        self.scene.selectionChanged.connect(self._on_selection_changed)
        self.view = LevelView(self.scene)
        self.view.blueprintDropped.connect(self.on_drop)
        self.view.deleteRequested.connect(self.delete_selected)
        self.view.duplicateRequested.connect(self.duplicate_selected)

        self.palette = Palette()
        self.palette.itemDoubleClicked.connect(self._palette_double_click)

        centre = QWidget()
        column = QVBoxLayout(centre)
        column.setContentsMargins(0, 0, 0, 0)
        column.addWidget(self._build_toolbar())
        column.addWidget(self.view)
        split = QSplitter()
        split.addWidget(self.palette)
        split.addWidget(centre)
        split.addWidget(self._build_inspector())
        split.setStretchFactor(1, 1)
        split.setSizes([200, 900, 300])
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(split)

        self._timer = QTimer(self)
        self._timer.setInterval(16)
        self._timer.timeout.connect(self._tick)

        project.blueprintsChanged.connect(self._on_blueprints_changed)
        self._fill_palette()
        self.refresh()
        self.view.fit_later(QRectF(-40, RULER_Y - 20, SCREEN_W * 2.2, SCREEN_H + 120))

    def load(self) -> dict:
        return normalize_stage(jsonio.load_json(self.path))

    def write(self) -> None:
        jsonio.save_json(self.path, self.doc.data)
        manifest = self.project.load_stage_manifest()
        rel = self.project.rel(self.path)
        for entry in manifest.get("stages", []):
            if entry.get("file") == rel and entry.get("name") != self.data["name"]:
                entry["name"] = self.data["name"]
                self.project.save_stage_manifest(manifest)

    @property
    def data(self) -> dict:
        return self.doc.data

    def bp(self, name: str) -> dict | None:
        if name not in self._bp_cache:
            try:
                self._bp_cache[name] = self.project.load_blueprint(name)
            except (KeyError, OSError, ValueError):
                self._bp_cache[name] = None
        return self._bp_cache[name]

    def scroll_speed(self) -> float:
        return max(float(self.data.get("scrollSpeed", 50.0)), 1.0)

    def _on_blueprints_changed(self) -> None:
        self._bp_cache.clear()
        self._fill_palette()
        self.refresh()

    def _fill_palette(self) -> None:
        self.palette.clear()
        groups = {kind: QTreeWidgetItem([label]) for kind, label in PALETTE_GROUPS.items()}
        for name in self.project.blueprint_names():
            data = self.bp(name)
            kind = blueprint_kind(data) if data else None
            if kind not in groups:
                continue
            item = QTreeWidgetItem([name])
            item.setData(0, Qt.UserRole, name)
            item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable | Qt.ItemIsDragEnabled)
            frames, _ = self.project.sprite_frames(data)
            if frames:
                item.setIcon(0, QIcon(frames[0].scaled(48, 32, Qt.KeepAspectRatio)))
            groups[kind].addChild(item)
        for group in groups.values():
            if group.childCount():
                self.palette.addTopLevelItem(group)
                group.setExpanded(True)

    def _palette_double_click(self, item, _column) -> None:
        name = item.data(0, Qt.UserRole)
        data = self.bp(name) if name else None
        if data is None:
            return
        if blueprint_kind(data) == KIND_TERRAIN:
            self.on_drop(name, QPointF(self.length_x + 1, 0))
        else:
            self.on_drop(name, self.view.mapToScene(self.view.viewport().rect().center()))

    def _build_toolbar(self) -> QToolBar:
        bar = QToolBar()
        self.play_action = bar.addAction("Preview", self.toggle_preview)
        bar.addSeparator()
        self.snap_check = QCheckBox("Snap")
        self.snap_check.setChecked(True)
        self.snap_size = QSpinBox()
        self.snap_size.setRange(1, 512)
        self.snap_size.setValue(16)
        self.snap_size.valueChanged.connect(self._snap_changed)
        bar.addWidget(self.snap_check)
        bar.addWidget(self.snap_size)
        bar.addSeparator()
        bar.addAction("Fit", self.fit_all)
        bar.addAction("Duplicate", self.duplicate_selected)
        bar.addAction("Delete", self.delete_selected)
        bar.addSeparator()
        bar.addAction("Raw JSON", lambda: self.openRequested.emit(self.path, "text"))
        return bar

    def _snap_changed(self, size: int) -> None:
        self.view.grid_size = size
        self.view.viewport().update()

    def _build_inspector(self) -> QScrollArea:
        stage_fields = copy.deepcopy(STAGE_FIELDS)
        stage_fields[1]["options"] = [""] + self.project.stage_ids()
        self.stage_form = FieldsForm(stage_fields, self.project)
        self.stage_form.edited.connect(self._stage_edited)
        self.segment_form = FieldsForm(SEGMENT_FIELDS, self.project)
        self.segment_form.edited.connect(self._segment_edited)
        self.spawn_form = FieldsForm(SPAWN_FIELDS, self.project)
        self.wave_spawn_form = FieldsForm(SPAWN_FIELDS + [WAVE_FIELD], self.project)
        for form in (self.spawn_form, self.wave_spawn_form):
            form.edited.connect(self._spawn_edited)
        self.sel_label = QLabel()
        self.sel_label.setWordWrap(True)

        stage_box = QGroupBox("Stage")
        QVBoxLayout(stage_box).addWidget(self.stage_form)
        selection_box = QGroupBox("Selection")
        box = QVBoxLayout(selection_box)
        for widget in (self.sel_label, self.segment_form, self.spawn_form, self.wave_spawn_form):
            box.addWidget(widget)

        panel = QWidget()
        column = QVBoxLayout(panel)
        column.addWidget(stage_box)
        column.addWidget(selection_box)
        column.addStretch(1)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(panel)
        scroll.setMinimumWidth(260)
        return scroll

    def _frames_for(self, name: str) -> tuple[list[QPixmap], float, float]:
        data = self.bp(name)
        frames = self.project.sprite_frames(data)[0] if data else []
        w, h = self.project.entity_size(data) if data else (64.0, 64.0)
        if not frames:
            placeholder = QPixmap(max(8, int(w)), max(8, int(h)))
            placeholder.fill(QColor(150, 40, 40, 160))
            frames = [placeholder]
        return frames, w, h

    @staticmethod
    def _anchor(bp: dict) -> QPointF:
        scale = component(bp, "transform").get("scale", {})
        origin = component(bp, "sprite").get("origin", {})
        return QPointF(float(origin.get("x", 0.0)) * float(scale.get("x", 1.0)),
                       float(origin.get("y", 0.0)) * float(scale.get("y", 1.0)))

    def refresh(self) -> None:
        self._rebuilding = True
        keep = self._selection
        self.scene.clear()
        self._segment_items, self._spawn_items = [], []
        speed = self.scroll_speed()
        x = float(self.data["startX"])
        for i, seg in enumerate(self.data["segments"]):
            frames, _, _ = self._frames_for(seg.get("blueprint", ""))
            bp = self.bp(seg.get("blueprint", "")) or {}
            base_y = float(component(bp, "transform").get("position", {}).get("y", 0.0))
            item = SegmentItem(self, i, frames[0], base_y, bool(seg.get("symmetric")))
            item.setPos(x, base_y)
            self.scene.addItem(item)
            self._segment_items.append(item)
            x += float(seg.get("spacing", 0.0))

        spawn_end = 0.0
        for i, spawn in enumerate(self.data["spawns"]):
            name = spawn.get("blueprint", "")
            frames, w, _ = self._frames_for(name)
            bp = self.bp(name) or {}
            anchor = self._anchor(bp)
            ax = float(spawn.get("time", 0.0)) * speed + float(spawn.get("x", DEFAULT_SPAWN_X))
            ay = float(spawn.get("y", 0.0))
            item = SpawnItem(self, i, frames, anchor, name)
            item.setPos(ax - anchor.x(), ay - anchor.y())
            self.scene.addItem(item)
            self._spawn_items.append(item)
            spawn_end = max(spawn_end, ax + w)
            self._add_wave_path(spawn, bp, ax, ay, speed)

        self.length_x = max(x, spawn_end, SCREEN_W)
        self._draw_frame_and_ruler(speed)
        self.scene.setSceneRect(-200, RULER_Y - 60, self.length_x + SCREEN_W + 400,
                                SCREEN_H + abs(RULER_Y) + 200)
        for items, indices in zip((self._segment_items, self._spawn_items), keep):
            for i in indices:
                if i < len(items):
                    items[i].setSelected(True)
        self._rebuilding = False
        self._refresh_inspector()

    def _add_wave_path(self, spawn: dict, bp: dict, ax: float, ay: float, speed: float) -> None:
        wave = effective_wave(spawn, bp)
        if wave is None:
            return
        world_vx = float(component(bp, "velocity").get("x", 0.0)) + speed
        path = QPainterPath(QPointF(ax, ay))
        for step in range(1, 121):
            tau = step * 8.0 / 120
            path.lineTo(ax + world_vx * tau, ay + wave["amplitude"] * math.sin(wave["frequency"] * tau))
        item = QGraphicsPathItem(path)
        item.setPen(QPen(QColor(255, 220, 120, 110), 0, Qt.DashLine))
        item.setZValue(1)
        self.scene.addItem(item)

    def _draw_frame_and_ruler(self, speed: float) -> None:
        pen = QPen(QColor(90, 160, 255, 160), 0)
        for y in (0, SCREEN_H):
            self.scene.addLine(-200, y, self.length_x + SCREEN_W + 200, y, pen).setZValue(-1)
        ruler_pen = QPen(QColor(200, 200, 210, 120), 0)
        for second in range(int((self.length_x + SCREEN_W) / speed) + 2):
            x, major = second * speed, second % 5 == 0
            self.scene.addLine(x, RULER_Y, x, RULER_Y + (22 if major else 10), ruler_pen)
            if major:
                label = self.scene.addSimpleText(f"{second}s")
                label.setBrush(QBrush(QColor(200, 200, 210)))
                label.setPos(x + 3, RULER_Y - 4)
        self._cam_item = QGraphicsRectItem(0, 0, SCREEN_W, SCREEN_H)
        self._cam_item.setPen(QPen(QColor(255, 210, 60), 0, Qt.DashLine))
        self._cam_item.setZValue(5)
        self._cam_item.setPos(self._cam_t * speed if self.previewing else 0, 0)
        self.scene.addItem(self._cam_item)

    def fit_all(self) -> None:
        self.view.fit_later(self.scene.itemsBoundingRect().adjusted(-40, -40, 40, 40))

    def _on_selection_changed(self) -> None:
        if self._rebuilding:
            return
        selected = self.scene.selectedItems()
        self._selection = (sorted(i.index for i in selected if isinstance(i, SegmentItem)),
                           sorted(i.index for i in selected if isinstance(i, SpawnItem)))
        self._refresh_inspector()

    def _refresh_inspector(self) -> None:
        self.stage_form.set_data(self.data)
        segments, spawns = self._selection
        single_segment = len(segments) == 1 and not spawns
        single_spawn = len(spawns) == 1 and not segments
        wave = None
        if single_spawn and spawns[0] < len(self.data["spawns"]):
            spawn = self.data["spawns"][spawns[0]]
            wave = effective_wave(spawn, self.bp(spawn.get("blueprint", "")) or {})
        self.segment_form.setVisible(single_segment)
        self.spawn_form.setVisible(single_spawn and wave is None)
        self.wave_spawn_form.setVisible(single_spawn and wave is not None)
        if single_segment:
            self.sel_label.setText(f"Terrain segment #{segments[0]}")
            self.segment_form.set_data(self.data["segments"][segments[0]])
        elif single_spawn:
            self.sel_label.setText(f"Spawn #{spawns[0]}")
            self.spawn_form.set_data(spawn)
            self.wave_spawn_form.set_data({**spawn, "wave": wave})
        elif segments or spawns:
            self.sel_label.setText(f"{len(segments) + len(spawns)} items selected (Delete to remove).")
        else:
            self.sel_label.setText("Nothing selected.\nDrag blueprints from the palette onto the canvas.")

    def _sort_spawns(self, d: dict, keep: list[dict]) -> None:
        """Order spawns by time and select ``keep`` (matched by identity) afterwards."""
        d["spawns"].sort(key=lambda s: s.get("time", 0.0))
        ids = {id(k) for k in keep}
        self._selection = ([], [i for i, sp in enumerate(d["spawns"]) if id(sp) in ids])

    def _stage_edited(self, out: dict) -> None:
        keys = [f["key"] for f in STAGE_FIELDS]
        self.doc.edit("Edit stage", lambda d: d.update({k: out[k] for k in keys if k in out}),
                      merge_key="stage")

    def _segment_edited(self, out: dict) -> None:
        index = self._selection[0][0]
        self.doc.edit("Edit segment", lambda d: d["segments"][index].update(out),
                      merge_key=f"segment{index}")

    def _spawn_edited(self, out: dict) -> None:
        index = self._selection[1][0]

        def apply(d):
            d["spawns"][index].update(out)
            self._sort_spawns(d, [d["spawns"][index]])
        self.doc.edit("Edit spawn", apply, merge_key="spawn")

    def _snap(self, value: float) -> float:
        if not self.snap_check.isChecked():
            return value
        step = self.snap_size.value()
        return round(value / step) * step

    def _spawn_from_anchor(self, spawn: dict, ax: float, ay: float) -> None:
        sx = float(spawn.get("x", DEFAULT_SPAWN_X))
        time = (ax - sx) / self.scroll_speed()
        if time < 0:
            time, sx = 0.0, ax
        spawn.update(time=round(float(time), 3), x=round(float(sx), 2), y=round(float(ay), 2))

    def on_drop(self, name: str, pos: QPointF) -> None:
        data = self.bp(name)
        kind = blueprint_kind(data) if data else None
        if kind not in PALETTE_GROUPS:
            return
        if kind == KIND_TERRAIN:
            index = sum(1 for i in self._segment_items
                        if i.pos().x() + i.pixmap().width() / 2 < pos.x())
            segment = {"blueprint": name, "symmetric": False,
                       "spacing": float(self.project.entity_size(data)[0])}
            self._selection = ([index], [])
            self.doc.edit("Add segment", lambda d: d["segments"].insert(index, segment))
            return
        w, h = self.project.entity_size(data)
        anchor = self._anchor(data)
        spawn = {"blueprint": name, "time": 0.0, "x": DEFAULT_SPAWN_X, "y": 0.0}
        self._spawn_from_anchor(spawn, self._snap(pos.x() - w / 2) + anchor.x(),
                                self._snap(pos.y() - h / 2) + anchor.y())

        def apply(d):
            d["spawns"].append(spawn)
            self._sort_spawns(d, [spawn])
        self.doc.edit("Add spawn", apply)

    def item_released(self, item) -> None:
        if self.previewing:
            return
        moved_spawns = [i for i in self._spawn_items if i.moved()]
        segment_moved = isinstance(item, SegmentItem) and item.moved()
        if not moved_spawns and not segment_moved:
            return
        new_positions = {i.index: (self._snap(i.anchor_pos().x()), self._snap(i.anchor_pos().y()))
                         for i in moved_spawns}
        order = None
        if segment_moved:
            centre = lambda s: s.pos().x() + s.pixmap().width() / 2  # noqa: E731
            others = sorted((s for s in self._segment_items if s is not item), key=centre)
            position = sum(1 for s in others if centre(s) < centre(item))
            order = [s.index for s in others]
            order.insert(position, item.index)
            self._selection = ([position], [])

        def apply(d):
            if order is not None:
                d["segments"] = [d["segments"][i] for i in order]
            moved = [d["spawns"][i] for i in new_positions]
            for spawn, (ax, ay) in zip(moved, new_positions.values()):
                self._spawn_from_anchor(spawn, ax, ay)
            if moved:
                self._sort_spawns(d, moved)
        if not self.doc.edit("Move", apply):
            self.refresh()

    def _selected_indices(self) -> tuple[list[int], list[int]]:
        return (sorted(i.index for i in self._segment_items if i.isSelected()),
                sorted(i.index for i in self._spawn_items if i.isSelected()))

    def delete_selected(self) -> None:
        segments, spawns = self._selected_indices()
        if not segments and not spawns:
            return

        def apply(d):
            d["segments"] = [s for i, s in enumerate(d["segments"]) if i not in segments]
            d["spawns"] = [s for i, s in enumerate(d["spawns"]) if i not in spawns]
        self._selection = ([], [])
        self.doc.edit("Delete", apply)

    def duplicate_selected(self) -> None:
        segments, spawns = self._selected_indices()
        if not segments and not spawns:
            return
        delay = self.snap_size.value() / self.scroll_speed() * 2

        def apply(d):
            if segments:
                end = segments[-1] + 1
                d["segments"][end:end] = [copy.deepcopy(d["segments"][i]) for i in segments]
            clones = [copy.deepcopy(d["spawns"][i]) for i in spawns]
            for clone in clones:
                clone["time"] = round(clone.get("time", 0.0) + delay, 3)
            d["spawns"].extend(clones)
            if clones:
                self._sort_spawns(d, clones)
        self.doc.edit("Duplicate", apply)

    def toggle_preview(self) -> None:
        if self.previewing:
            self.stop_preview()
            return
        self.previewing, self._cam_t = True, 0.0
        self.play_action.setText("Stop")
        self.scene.clearSelection()
        self._timer.start()

    def stop_preview(self) -> None:
        self._timer.stop()
        self.previewing = False
        self.play_action.setText("Preview")
        self.refresh()

    def _tick(self) -> None:
        self._cam_t += self._timer.interval() / 1000.0
        cam_x = self._cam_t * self.scroll_speed()
        if cam_x > self.length_x:
            self.stop_preview()
            return
        self._cam_item.setPos(cam_x, 0)
        self.view.centerOn(cam_x + SCREEN_W / 2, SCREEN_H / 2)
        for item in self._spawn_items:
            spawn = self.data["spawns"][item.index]
            age = self._cam_t - float(spawn.get("time", 0.0))
            item.setVisible(age >= 0)
            if age < 0:
                continue
            bp = self.bp(spawn.get("blueprint", "")) or {}
            velocity = component(bp, "velocity")
            wave = effective_wave(spawn, bp)
            x = float(spawn.get("x", DEFAULT_SPAWN_X)) + float(velocity.get("x", 0.0)) * age
            y = float(spawn.get("y", 0.0))
            y += wave["amplitude"] * math.sin(wave["frequency"] * age) if wave \
                else float(velocity.get("y", 0.0)) * age
            item.setPos(cam_x + x - item.anchor.x(), y - item.anchor.y())
            animation = component(bp, "animation")
            duration = float(animation.get("frameDuration", 0.0))
            if duration > 0 and len(item.frames) > 1:
                frame = int(age / duration)
                last = len(item.frames) - 1
                item.setPixmap(item.frames[frame % len(item.frames) if animation.get("loop", True)
                                           else min(frame, last)])
