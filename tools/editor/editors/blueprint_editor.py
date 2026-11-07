from __future__ import annotations

import copy
from pathlib import Path

from PySide6.QtCore import QRectF, QTimer
from PySide6.QtGui import QBrush, QColor, QPen
from PySide6.QtWidgets import (QComboBox, QGraphicsPixmapItem, QGraphicsRectItem,
                               QGraphicsScene, QHBoxLayout, QInputDialog, QLabel,
                               QListWidget, QMessageBox, QPushButton, QScrollArea,
                               QSplitter, QVBoxLayout, QWidget)

from .. import jsonio, schema
from ..project import Project, component
from ..widgets.property_form import FieldsForm, RawJsonForm
from ..widgets.zoom_view import ZoomView
from .base import DocumentTab


class BlueprintPreview(QWidget):

    def __init__(self, project: Project, parent=None):
        super().__init__(parent)
        self.project = project
        self.scene = QGraphicsScene(self)
        self.view = ZoomView(self.scene)
        self.view.grid_size = 16
        self.clip_box = QComboBox()
        self.clip_box.currentTextChanged.connect(lambda _=None: self._rebuild())
        self.show_collider = QPushButton("Collider")
        self.show_collider.setCheckable(True)
        self.show_collider.setChecked(True)
        self.show_collider.toggled.connect(lambda _=None: self._rebuild())
        bar = QHBoxLayout()
        bar.addWidget(QLabel("Animation clip"))
        bar.addWidget(self.clip_box)
        bar.addWidget(self.show_collider)
        bar.addStretch()
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addLayout(bar)
        lay.addWidget(self.view)
        self._data: dict = {}
        self._frames: list = []
        self._loop = True
        self._reverse = False
        self._index = 0
        self._item: QGraphicsPixmapItem | None = None
        self._first_fit = True
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._advance)

    def set_blueprint(self, data: dict) -> None:
        self._data = data
        clips = component(data, "animator").get("clips", {})
        current = self.clip_box.currentText()
        self.clip_box.blockSignals(True)
        self.clip_box.clear()
        self.clip_box.addItem("(animation)")
        self.clip_box.addItems(list(clips))
        if current in clips:
            self.clip_box.setCurrentText(current)
        self.clip_box.blockSignals(False)
        self._rebuild()

    def _effective_data(self) -> dict:
        data = copy.deepcopy(self._data)
        clip_name = self.clip_box.currentText()
        clips = component(data, "animator").get("clips", {})
        if clip_name in clips:
            anim = None
            for key, value in data.get("components", {}).items():
                if key.lower() == "animation":
                    anim = value
            if anim is None:
                anim = data.setdefault("components", {}).setdefault("animation", {})
            anim.update({k: v for k, v in clips[clip_name].items()
                         if k in ("startX", "startY", "totalFrames", "frameDuration",
                                  "loop", "reverse")})
        return data

    def _rebuild(self) -> None:
        self.scene.clear()
        self._timer.stop()
        data = self._effective_data()
        frames, duration = self.project.sprite_frames(data)
        anim = component(data, "animation")
        self._frames = frames
        self._loop = bool(anim.get("loop", True))
        self._reverse = bool(anim.get("reverse", False))
        self._index = len(frames) - 1 if self._reverse and frames else 0
        tr = component(data, "transform")
        scale = tr.get("scale", {})
        sx, sy = float(scale.get("x", 1.0)), float(scale.get("y", 1.0))
        origin = component(data, "sprite").get("origin", {})
        ox, oy = float(origin.get("x", 0.0)) * sx, float(origin.get("y", 0.0)) * sy
        axis = QPen(QColor(255, 255, 255, 60), 0)
        self.scene.addLine(-2000, 0, 2000, 0, axis)
        self.scene.addLine(0, -2000, 0, 2000, axis)
        content = QRectF(-16, -16, 32, 32)
        if frames:
            self._item = self.scene.addPixmap(frames[self._index])
            self._item.setPos(-ox, -oy)
            content = content.united(QRectF(-ox, -oy, frames[0].width(), frames[0].height()))
            self._item.setOpacity(int(component(data, "sprite").get("color", {}).get("a", 255)) / 255)
        else:
            self._item = None
            note = self.scene.addSimpleText("No sprite (texture missing or textureRect empty)")
            note.setBrush(QBrush(QColor(220, 120, 120)))
        coll = component(data, "collider")
        if coll and self.show_collider.isChecked():
            x = float(coll.get("offsetX", 0.0)) * sx
            y = float(coll.get("offsetY", 0.0)) * sy
            w = float(coll.get("width", 0.0)) * sx
            h = float(coll.get("height", 0.0)) * sy
            box = QGraphicsRectItem(x, y, w, h)
            box.setPen(QPen(QColor(255, 80, 80), 0))
            box.setBrush(QBrush(QColor(255, 80, 80, 40)))
            box.setZValue(1)
            self.scene.addItem(box)
            content = content.united(QRectF(x, y, w, h))
        rect = content.adjusted(-content.width() * 0.3 - 8, -content.height() * 0.3 - 8,
                                content.width() * 0.3 + 8, content.height() * 0.3 + 8)
        self.scene.setSceneRect(rect.adjusted(-2000, -2000, 2000, 2000))
        if self._first_fit:
            self._first_fit = False
            self.view.fit_later(rect, max_zoom=8)
        if len(frames) > 1 and duration > 0:
            self._timer.start(max(10, int(duration * 1000)))

    def _advance(self) -> None:
        n = len(self._frames)
        if n < 2 or self._item is None:
            return
        step = -1 if self._reverse else 1
        nxt = self._index + step
        if 0 <= nxt < n:
            self._index = nxt
        elif self._loop:
            self._index = (nxt) % n
        self._item.setPixmap(self._frames[self._index])


class BlueprintEditor(DocumentTab):
    def __init__(self, path: Path, project: Project, parent=None):
        super().__init__(path, project, parent)
        self.rel = project.rel(self.path)
        self.name = project.blueprint_name_for_file(self.rel) or self.path.stem

        self.components = QListWidget()
        self.components.currentTextChanged.connect(self._select_component)
        self.add_box = QComboBox()
        add_btn = QPushButton("Add")
        add_btn.clicked.connect(self._add_component)
        rem_btn = QPushButton("Remove")
        rem_btn.clicked.connect(self._remove_component)
        dup_btn = QPushButton("Save as new blueprint...")
        dup_btn.clicked.connect(self._save_as)
        raw_btn = QPushButton("Raw JSON")
        raw_btn.clicked.connect(lambda: self.openRequested.emit(self.path, "text"))

        left = QWidget()
        lv = QVBoxLayout(left)
        self.title_label = QLabel()
        lv.addWidget(self.title_label)
        lv.addWidget(QLabel("Components"))
        lv.addWidget(self.components)
        row = QHBoxLayout()
        row.addWidget(self.add_box, 1)
        row.addWidget(add_btn)
        lv.addLayout(row)
        lv.addWidget(rem_btn)
        lv.addWidget(dup_btn)
        lv.addWidget(raw_btn)

        self.preview = BlueprintPreview(project)
        self.form_holder = QWidget()
        self.form_layout = QVBoxLayout(self.form_holder)
        self.form_layout.setContentsMargins(6, 6, 6, 6)
        self.form_layout.addStretch(1)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(self.form_holder)
        scroll.setMinimumWidth(320)

        split = QSplitter()
        split.addWidget(left)
        split.addWidget(self.preview)
        split.addWidget(scroll)
        split.setStretchFactor(1, 1)
        split.setSizes([220, 560, 380])
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(split)

        self._form: QWidget | None = None
        self._form_component: str | None = None
        project.blueprintsChanged.connect(self._refresh_title)
        self.refresh()

    def load(self) -> dict:
        data = jsonio.load_json(self.path)
        data.setdefault("components", {})
        return data

    def write(self) -> None:
        self.project.save_blueprint(self.name, self.doc.data, self.rel)

    def apply_patch(self, patch: dict) -> None:
        def apply(d):
            comps = d["components"]
            for key, value in patch.items():
                existing = next((k for k in comps if k.lower() == key.lower()), key)
                if isinstance(value, dict) and isinstance(comps.get(existing), dict):
                    comps[existing].update(copy.deepcopy(value))
                else:
                    comps[existing] = copy.deepcopy(value)
        self.doc.edit("Apply sprite", apply)

    def _refresh_title(self) -> None:
        registered = self.name in self.project.blueprint_files
        suffix = "" if registered else "  (not yet in blueprints.json: saved on Ctrl+S)"
        self.title_label.setText(f"<b>{self.name}</b><br><small>{self.rel}{suffix}</small>")

    def _comps(self) -> dict:
        return self.doc.data["components"]

    def refresh(self) -> None:
        self._refresh_title()
        current = self.components.currentItem().text() if self.components.currentItem() else None
        names = list(self._comps())
        self.components.blockSignals(True)
        self.components.clear()
        self.components.addItems(names)
        self.components.blockSignals(False)
        present = {n.lower() for n in names}
        self.add_box.clear()
        self.add_box.addItems([c for c in schema.COMPONENTS if c.lower() not in present])
        target = current if current in names else (names[0] if names else None)
        if target:
            self.components.setCurrentRow(names.index(target))
        self._select_component(target or "", force=True)
        self.preview.set_blueprint(self.doc.data)

    def _clear_form(self) -> None:
        if self._form is not None:
            self._form.setParent(None)
            self._form.deleteLater()
            self._form = None

    def _select_component(self, name: str, force: bool = False) -> None:
        comps = self._comps()
        if not name or name not in comps:
            self._clear_form()
            self._form_component = None
            return
        if name == self._form_component and self._form is not None and not force:
            return
        if name == self._form_component and self._form is not None:
            self._form.set_data(comps[name])
            return
        self._clear_form()
        fields = schema.fields_of(name)
        if fields is None:
            form: QWidget = RawJsonForm()
        else:
            form = FieldsForm(fields, self.project)
        form.set_data(comps[name])
        form.edited.connect(lambda data, n=name: self._component_edited(n, data))
        self.form_layout.insertWidget(0, form)
        self._form, self._form_component = form, name

    def _component_edited(self, name: str, data: dict) -> None:
        def apply(d):
            d["components"][name] = data
        self.doc.edit(f"Edit {name}", apply, merge_key=f"comp-{name}")

    def _add_component(self) -> None:
        name = self.add_box.currentText()
        if not name:
            return

        def apply(d):
            d["components"][name] = schema.default_component(name)
        self.doc.edit(f"Add {name}", apply)
        names = list(self._comps())
        if name in names:
            self.components.setCurrentRow(names.index(name))

    def _remove_component(self) -> None:
        item = self.components.currentItem()
        if not item:
            return
        name = item.text()

        def apply(d):
            d["components"].pop(name, None)
        self._form_component = None
        self.doc.edit(f"Remove {name}", apply)

    def _save_as(self) -> None:
        name, ok = QInputDialog.getText(self, "New blueprint", "Name of the copy:",
                                        text=self.project.unique_blueprint_name(self.name))
        if not ok or not name:
            return
        if name in self.project.blueprint_files:
            QMessageBox.warning(self, "Blueprint", f"'{name}' already exists.")
            return
        rel = f"assets/descriptionEntity/{name}.json"
        self.project.save_blueprint(name, copy.deepcopy(self.doc.data), rel)
        self.openRequested.emit(self.project.abs(rel), "auto")
