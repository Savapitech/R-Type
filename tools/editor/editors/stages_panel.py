from __future__ import annotations

import copy
from pathlib import Path

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QAbstractItemView, QHBoxLayout, QLabel, QListWidget,
                               QListWidgetItem, QMessageBox, QPushButton, QVBoxLayout,
                               QWidget)

from .. import jsonio
from ..project import Project, blank_stage


class StagesPanel(QWidget):
    openStage = Signal(object)
    filesRewritten = Signal(object)

    def __init__(self, project: Project, parent=None):
        super().__init__(parent)
        self.project = project
        self.list = QListWidget()
        self.list.setDragDropMode(QAbstractItemView.InternalMove)
        self.list.model().rowsMoved.connect(lambda *_: self._order_changed())
        self.list.itemDoubleClicked.connect(self._open)
        self.info = QLabel("Drag to reorder: nextStage links follow the order.")
        self.info.setWordWrap(True)
        lay = QVBoxLayout(self)
        lay.addWidget(self.info)
        lay.addWidget(self.list)
        row = QHBoxLayout()
        for label, fn in (("New", self.new_stage), ("Duplicate", self.duplicate_stage),
                          ("Remove", self.remove_stage), ("Default", self.set_default)):
            b = QPushButton(label)
            b.clicked.connect(fn)
            row.addWidget(b)
        lay.addLayout(row)
        project.stagesChanged.connect(self.refresh)
        self.refresh()

    def refresh(self) -> None:
        manifest = self.project.load_stage_manifest()
        default = manifest.get("defaultStage", "")
        self.list.blockSignals(True)
        self.list.clear()
        for entry in manifest.get("stages", []):
            star = "★ " if entry["id"] == default else ""
            item = QListWidgetItem(f"{star}{entry['id']} — {entry.get('name', '')}")
            item.setData(Qt.UserRole, entry["id"])
            self.list.addItem(item)
        self.list.blockSignals(False)

    def _ids(self) -> list[str]:
        return [self.list.item(i).data(Qt.UserRole) for i in range(self.list.count())]

    def _current_id(self) -> str | None:
        item = self.list.currentItem()
        return item.data(Qt.UserRole) if item else None

    def _open(self, item) -> None:
        manifest = self.project.load_stage_manifest()
        for entry in manifest.get("stages", []):
            if entry["id"] == item.data(Qt.UserRole):
                self.openStage.emit(self.project.abs(entry["file"]))

    def _write(self, manifest: dict) -> None:
        stages = manifest.get("stages", [])
        rewritten: list[Path] = []
        for i, entry in enumerate(stages):
            nxt = stages[i + 1]["id"] if i + 1 < len(stages) else ""
            entry["nextStage"] = nxt
            path = self.project.abs(entry["file"])
            if path.exists():
                data = jsonio.load_json(path)
                if data.get("nextStage", "") != nxt:
                    data["nextStage"] = nxt
                    jsonio.save_json(path, data)
                    rewritten.append(path)
        if stages and manifest.get("defaultStage") not in {s["id"] for s in stages}:
            manifest["defaultStage"] = stages[0]["id"]
        self.project.save_stage_manifest(manifest)
        if rewritten:
            self.filesRewritten.emit(rewritten)

    def _order_changed(self) -> None:
        manifest = self.project.load_stage_manifest()
        by_id = {s["id"]: s for s in manifest.get("stages", [])}
        manifest["stages"] = [by_id[i] for i in self._ids() if i in by_id]
        self._write(manifest)

    def new_stage(self) -> None:
        stage_id = self.project.next_stage_id()
        rel = f"assets/stages/{stage_id}.json"
        jsonio.save_json(self.project.abs(rel), blank_stage(stage_id.replace("stage", "Stage ")))
        manifest = self.project.load_stage_manifest()
        manifest.setdefault("stages", []).append(
            {"id": stage_id, "name": stage_id.replace("stage", "Stage "), "file": rel, "nextStage": ""})
        self._write(manifest)
        self.openStage.emit(self.project.abs(rel))

    def duplicate_stage(self) -> None:
        source_id = self._current_id()
        if not source_id:
            return
        manifest = self.project.load_stage_manifest()
        source = next(s for s in manifest["stages"] if s["id"] == source_id)
        stage_id = self.project.next_stage_id()
        rel = f"assets/stages/{stage_id}.json"
        data = copy.deepcopy(jsonio.load_json(self.project.abs(source["file"])))
        data["name"] = f"{data.get('name', source_id)} (copy)"
        jsonio.save_json(self.project.abs(rel), data)
        manifest["stages"].append({"id": stage_id, "name": data["name"], "file": rel, "nextStage": ""})
        self._write(manifest)
        self.openStage.emit(self.project.abs(rel))

    def remove_stage(self) -> None:
        stage_id = self._current_id()
        if not stage_id:
            return
        answer = QMessageBox.question(
            self, "Remove stage",
            f"Remove '{stage_id}' from stages.json?\n(The stage file stays on disk.)")
        if answer != QMessageBox.Yes:
            return
        manifest = self.project.load_stage_manifest()
        manifest["stages"] = [s for s in manifest["stages"] if s["id"] != stage_id]
        self._write(manifest)

    def set_default(self) -> None:
        stage_id = self._current_id()
        if stage_id:
            manifest = self.project.load_stage_manifest()
            manifest["defaultStage"] = stage_id
            self.project.save_stage_manifest(manifest)
