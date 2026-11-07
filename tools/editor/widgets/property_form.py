from __future__ import annotations

import copy
import json
import os
from typing import Any

from PySide6.QtCore import QTimer, Signal
from PySide6.QtWidgets import (QInputDialog, QLabel, QPlainTextEdit, QPushButton, QVBoxLayout,
                               QWidget)
from pyqtgraph.parametertree import Parameter, ParameterTree
from pyqtgraph.parametertree.parameterTypes import GroupParameter

from .. import schema

SCALARS = ("float", "int", "bool", "str")


def _as_field(sub: tuple | dict) -> dict:
    return sub if isinstance(sub, dict) else schema.field(*sub)


def _clip_spec(name: str, clip: dict | None = None) -> dict:
    children = [{"name": f["key"], "type": f["type"], "default": f["default"],
                 "value": (clip or {}).get(f["key"], f["default"])}
                for f in schema.CLIP_FIELDS]
    return {"name": name, "type": "group", "removable": True, "children": children}


class ClipsParameter(GroupParameter):
    def __init__(self, name: str):
        super().__init__(name=name, addText="Add clip")

    def addNew(self, *_args) -> None:
        name, ok = QInputDialog.getItem(None, "Clip", "State name:", schema.ANIM_STATES, 0, True)
        if ok and name and name not in self.names:
            self.addChild(_clip_spec(name))


def _make(fld: dict, project) -> Parameter | dict:
    key, kind, default = fld["key"], fld["type"], fld["default"]
    base = {"name": key, "default": default}
    if kind == "float":
        return {**base, "type": kind, "value": default, "decimals": 12}
    if kind in SCALARS:
        return {**base, "type": kind, "value": default}
    if kind == "enum":
        return {**base, "type": "list", "limits": fld["options"], "value": default}
    if kind == "blueprint":
        names = project.blueprint_names() if project else []
        return {**base, "type": "list", "limits": names or [default], "value": default}
    if kind == "flags":
        return {**base, "type": "checklist", "limits": fld["options"], "value": default}
    if kind == "texture":
        directory = str(project.root / "assets") if project else ""
        return {**base, "type": "file", "fileMode": "ExistingFile", "directory": directory,
                "nameFilter": "Images (*.png *.jpg *.jpeg *.bmp)", "value": default}
    if kind == "group" and fld.get("color"):
        return {**base, "type": "color", "value": tuple(default[c] for c in "rgba")}
    if kind == "group":
        return {**base, "type": "group",
                "children": [_make(_as_field(s), project) for s in fld["sub"]]}
    return ClipsParameter(key)


def _read(param: Parameter, fld: dict, project) -> Any:
    kind = fld["type"]
    if kind == "group" and fld.get("color"):
        return dict(zip("rgba", param.value().getRgb()))
    if kind == "group":
        return {s["key"]: _read(param.child(s["key"]), s, project)
                for s in map(_as_field, fld["sub"])}
    if kind == "clips":
        return {clip.name(): {c.name(): c.value() for c in clip.children()}
                for clip in param.children()}
    value = param.value()
    if kind == "texture" and value and os.path.isabs(value) and project:
        return project.rel(value)
    if kind == "float":
        return float(value)
    if kind == "int":
        return int(value)
    return list(value) if kind == "flags" else value


def _write(param: Parameter, fld: dict, value: Any) -> None:
    kind = fld["type"]
    if kind == "group" and fld.get("color"):
        param.setValue(tuple((value or fld["default"]).get(c, 255) for c in "rgba"))
    elif kind == "group":
        for sub in map(_as_field, fld["sub"]):
            _write(param.child(sub["key"]), sub, (value or {}).get(sub["key"], sub["default"]))
    elif kind == "clips":
        param.clearChildren()
        for name, clip in (value or {}).items():
            param.addChild(_clip_spec(name, clip))
    elif kind in ("enum", "blueprint"):
        if value not in param.opts["limits"]:
            param.setLimits([*param.opts["limits"], value])
        param.setValue(value)
    elif kind == "flags":
        param.setValue([v for v in value if v in fld["options"]])
    else:
        param.setValue(value)


class FieldsForm(QWidget):
    edited = Signal(dict)

    def __init__(self, fields: list[dict], project=None, parent=None):
        super().__init__(parent)
        self._fields, self._project, self._data = fields, project, {}
        self._loading = False
        self.params = Parameter.create(name="form", type="group",
                                       children=[_make(f, project) for f in fields])
        self.tree = ParameterTree(showHeader=False)
        self.tree.setParameters(self.params, showTop=False)
        self.tree.setVerticalScrollBarPolicy(self.tree.verticalScrollBarPolicy().ScrollBarAlwaysOff)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.tree)
        self.params.sigTreeStateChanged.connect(self._on_change)
        self.tree.itemExpanded.connect(self._fit)
        self.tree.itemCollapsed.connect(self._fit)

    def _fit(self, *_args) -> None:
        def bottom(item) -> int:
            lowest = self.tree.visualItemRect(item).bottom()
            if item.isExpanded():
                for i in range(item.childCount()):
                    lowest = max(lowest, bottom(item.child(i)))
            return lowest
        rows = [self.tree.topLevelItem(i) for i in range(self.tree.topLevelItemCount())]
        height = max([bottom(r) for r in rows], default=0) + 6
        self.tree.setFixedHeight(height)
        self.setFixedHeight(height)

    def data(self) -> dict:
        return copy.deepcopy(self._data)

    def set_data(self, data: dict) -> None:
        self._data = copy.deepcopy(data)
        self._loading = True
        with self.params.treeChangeBlocker():
            for fld in self._fields:
                _write(self.params.child(fld["key"]), fld, self._data.get(fld["key"], fld["default"]))
        self._loading = False
        QTimer.singleShot(0, self._fit)

    def _on_change(self, _param, changes) -> None:
        if self._loading or not any(kind in ("value", "childAdded", "childRemoved") for _, kind, _ in changes):
            return
        out = copy.deepcopy(self._data)
        for fld in self._fields:
            key = fld["key"]
            value = _read(self.params.child(key), fld, self._project)
            if fld["type"] == "group" and isinstance(out.get(key), dict) and not fld.get("color"):
                value = {**out[key], **value}
            if key in out or value != fld["default"]:
                out[key] = value
        self._data = out
        QTimer.singleShot(0, self._fit)
        self.edited.emit(copy.deepcopy(out))


class RawJsonForm(QWidget):
    edited = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.text = QPlainTextEdit()
        self.status = QLabel()
        apply_button = QPushButton("Apply")
        for widget in (QLabel("Unknown component: edit raw JSON"), self.text, self.status, apply_button):
            layout.addWidget(widget)
        apply_button.clicked.connect(self._apply)

    def set_data(self, data: dict) -> None:
        self.text.setPlainText(json.dumps(data, indent=2))
        self.status.clear()

    def _apply(self) -> None:
        try:
            value = json.loads(self.text.toPlainText())
            if not isinstance(value, dict):
                raise ValueError("a JSON object is required")
        except ValueError as err:
            self.status.setText(f"Invalid JSON: {err}")
            return
        self.status.clear()
        self.edited.emit(value)
