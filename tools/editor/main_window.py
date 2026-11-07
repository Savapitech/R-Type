from __future__ import annotations

import copy
from pathlib import Path

from PySide6.QtCore import QDir, Qt
from PySide6.QtGui import QAction, QImage, QKeySequence, QUndoGroup
from PySide6.QtWidgets import (QComboBox, QDialog, QFileSystemModel, QDialogButtonBox, QFileDialog, QFormLayout,
                               QInputDialog, QLineEdit, QMainWindow, QMessageBox,
                               QTabWidget, QTreeView, QDockWidget)

from . import jsonio, templates
from .editors.base import EditorTab
from .editors.blueprint_editor import BlueprintEditor
from .editors.level_editor import LevelEditor
from .editors.sprite_editor import SpriteEditor
from .editors.stages_panel import StagesPanel
from .editors.text_editor import TextEditor
from .project import BLUEPRINT_DIRS, Project, valid_identifier

IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp"}
RAW_ONLY = {"stages.json", "blueprints.json"}


class NewBlueprintDialog(QDialog):
    def __init__(self, project: Project, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New blueprint")
        self.name = QLineEdit(project.unique_blueprint_name("new_entity"))
        self.kind = QComboBox()
        for label in templates.TEMPLATES:
            self.kind.addItem(label)
        form = QFormLayout(self)
        form.addRow("Name", self.name)
        form.addRow("Template", self.kind)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def result_values(self) -> tuple[str, str]:
        return self.name.text().strip(), templates.TEMPLATES[self.kind.currentText()]


class MainWindow(QMainWindow):
    def __init__(self, project: Project):
        super().__init__()
        self.project = project
        self.setWindowTitle("R-Type Editor")
        self.resize(1500, 900)
        self.undo_group = QUndoGroup(self)

        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.setMovable(True)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self._current_changed)
        self.setCentralWidget(self.tabs)

        self._build_docks()
        self._build_menus()
        project.blueprintsChanged.connect(self._reindex_blueprints)
        self.statusBar().showMessage(f"Project: {project.root}")

    def _build_docks(self) -> None:
        self.model = QFileSystemModel(self)
        assets = str(self.project.root / "assets")
        self.model.setRootPath(assets)
        self.model.setFilter(QDir.AllDirs | QDir.Files | QDir.NoDotAndDotDot)
        self.model.setNameFilters(["*.json", "*.png", "*.jpg", "*.txt", "*.md", "*.wav", "*.otf"])
        self.model.setNameFilterDisables(False)
        self.tree = QTreeView()
        self.tree.setModel(self.model)
        self.tree.setRootIndex(self.model.index(assets))
        for col in (1, 2, 3):
            self.tree.hideColumn(col)
        self.tree.setHeaderHidden(True)
        self.tree.doubleClicked.connect(lambda idx: self._tree_open(idx))
        self.files_dock = QDockWidget("Files", self)
        self.files_dock.setWidget(self.tree)
        self.addDockWidget(Qt.LeftDockWidgetArea, self.files_dock)

        self.stages_panel = StagesPanel(self.project)
        self.stages_panel.openStage.connect(lambda p: self.open_path(p))
        self.stages_panel.filesRewritten.connect(self._reload_paths)
        self.stages_dock = QDockWidget("Stages", self)
        self.stages_dock.setWidget(self.stages_panel)
        self.addDockWidget(Qt.LeftDockWidgetArea, self.stages_dock)
        self.tabifyDockWidget(self.files_dock, self.stages_dock)
        self.files_dock.raise_()

    def _build_menus(self) -> None:
        bar = self.menuBar()
        file_menu = bar.addMenu("&File")

        def add(menu, text, slot, shortcut=None):
            action = QAction(text, self)
            if shortcut:
                action.setShortcut(shortcut)
            action.triggered.connect(slot)
            menu.addAction(action)
            return action

        add(file_menu, "New stage", self.stages_panel.new_stage)
        add(file_menu, "New blueprint...", self.new_blueprint)
        add(file_menu, "New image...", self.new_image)
        add(file_menu, "New text file...", self.new_text_file)
        file_menu.addSeparator()
        add(file_menu, "Open file...", self.open_dialog, QKeySequence.Open)
        add(file_menu, "Save", self.save_current, QKeySequence.Save)
        add(file_menu, "Save all", self.save_all, "Ctrl+Shift+S")
        add(file_menu, "Close tab", lambda: self.close_tab(self.tabs.currentIndex()), "Ctrl+W")
        file_menu.addSeparator()
        add(file_menu, "Quit", self.close, QKeySequence.Quit)

        edit_menu = bar.addMenu("&Edit")
        undo = self.undo_group.createUndoAction(self, "Undo")
        undo.setShortcut(QKeySequence.Undo)
        redo = self.undo_group.createRedoAction(self, "Redo")
        redo.setShortcut(QKeySequence.Redo)
        edit_menu.addAction(undo)
        edit_menu.addAction(redo)

        view_menu = bar.addMenu("&View")
        view_menu.addAction(self.files_dock.toggleViewAction())
        view_menu.addAction(self.stages_dock.toggleViewAction())

        help_menu = bar.addMenu("&Help")
        add(help_menu, "Shortcuts", lambda: QMessageBox.information(
            self, "Shortcuts",
            "Level editor\n"
            "• Drag a blueprint from the palette onto the canvas (or double-click it)\n"
            "• Terrain blueprints extend the terrain track, others become timed spawns\n"
            "• Wheel: zoom   Middle mouse: pan   Delete: remove   Ctrl+D: duplicate\n"
            "• ▶ Preview scrolls the camera and plays the spawns\n\n"
            "Ctrl+S save • Ctrl+Z / Ctrl+Shift+Z undo / redo"))

    def _tree_open(self, index) -> None:
        path = Path(self.model.filePath(index))
        if path.is_file():
            self.open_path(path)

    def open_dialog(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Open", str(self.project.root / "assets"))
        if path:
            self.open_path(Path(path))

    def _kind_of(self, path: Path, mode: str) -> str:
        if path.suffix.lower() in IMAGE_SUFFIXES:
            return "sprite"
        if mode == "text" or path.suffix.lower() != ".json" or path.name in RAW_ONLY:
            return "text"
        try:
            data = jsonio.load_json(path)
        except (OSError, ValueError):
            return "text"
        if isinstance(data, dict) and ("segments" in data or path.parent.name == "stages"):
            return "level"
        if isinstance(data, dict) and "components" in data:
            return "blueprint"
        return "text"

    def open_path(self, path: Path, mode: str = "auto") -> EditorTab | None:
        path = Path(path).resolve()
        kind = self._kind_of(path, mode)
        for i in range(self.tabs.count()):
            tab = self.tabs.widget(i)
            if tab.path == path and getattr(tab, "kind", None) == kind:
                self.tabs.setCurrentIndex(i)
                return tab
        try:
            if kind == "sprite":
                tab = SpriteEditor(path, self.project)
                tab.applyRequested.connect(self.apply_to_blueprint)
                tab.newBlueprintRequested.connect(self.blueprint_from_patch)
            elif kind == "level":
                tab = LevelEditor(path, self.project)
            elif kind == "blueprint":
                tab = BlueprintEditor(path, self.project)
            else:
                tab = TextEditor(path, self.project)
        except Exception as err:
            QMessageBox.critical(self, "Cannot open", f"{path.name}: {err}")
            return None
        tab.kind = kind
        if tab.undo_stack is not None:
            self.undo_group.addStack(tab.undo_stack)
        tab.modifiedChanged.connect(self._refresh_titles)
        tab.saved.connect(lambda p, t=tab: self._on_saved(p, t))
        tab.openRequested.connect(lambda p, m, t=tab: self._open_requested(t, p, m))
        index = self.tabs.addTab(tab, tab.title())
        self.tabs.setCurrentIndex(index)
        self._refresh_titles()
        return tab

    def _open_requested(self, source: EditorTab, path: Path, mode: str) -> None:
        if mode == "text" and source.is_modified():
            answer = QMessageBox.question(self, "Unsaved changes",
                                          "Save before switching to the raw JSON view?")
            if answer == QMessageBox.Yes:
                source.save()
        self.open_path(path, mode)

    def _current_changed(self, index: int) -> None:
        tab = self.tabs.widget(index) if index >= 0 else None
        stack = getattr(tab, "undo_stack", None)
        self.undo_group.setActiveStack(stack)
        self.setWindowTitle(f"R-Type Editor — {tab.title()}" if tab else "R-Type Editor")

    def _refresh_titles(self) -> None:
        for i in range(self.tabs.count()):
            tab = self.tabs.widget(i)
            self.tabs.setTabText(i, tab.title() + (" *" if tab.is_modified() else ""))
            self.tabs.setTabToolTip(i, str(tab.path))

    def close_tab(self, index: int) -> bool:
        if index < 0:
            return True
        tab = self.tabs.widget(index)
        if tab.is_modified():
            self.tabs.setCurrentIndex(index)
            box = QMessageBox.question(
                self, "Unsaved changes", f"Save changes to {tab.path.name}?",
                QMessageBox.Save | QMessageBox.Discard | QMessageBox.Cancel)
            if box == QMessageBox.Cancel or (box == QMessageBox.Save and not tab.save()):
                return False
        if tab.undo_stack is not None:
            self.undo_group.removeStack(tab.undo_stack)
        self.tabs.removeTab(index)
        tab.deleteLater()
        return True

    def closeEvent(self, event):
        while self.tabs.count():
            if not self.close_tab(self.tabs.count() - 1):
                event.ignore()
                return
        event.accept()

    def save_current(self) -> None:
        tab = self.tabs.currentWidget()
        if tab is not None and tab.save():
            self.statusBar().showMessage(f"Saved {tab.path}", 3000)

    def save_all(self) -> None:
        for i in range(self.tabs.count()):
            tab = self.tabs.widget(i)
            if tab.is_modified():
                tab.save()

    def _on_saved(self, path: Path, source: EditorTab) -> None:
        self._reload_paths([path], skip=source)
        rel = self.project.rel(path)
        in_blueprints = rel == self.project.rel(self.project.manifest_path) or \
            any(rel.startswith(d + "/") for d in BLUEPRINT_DIRS)
        if path.suffix.lower() in IMAGE_SUFFIXES or in_blueprints:
            self.project.reload_blueprint_index()
            self.project.invalidate_texture()
            self.project.blueprintsChanged.emit()
        if path.name == "stages.json":
            self.project.stagesChanged.emit()
        self.statusBar().showMessage(f"Saved {path.name}", 3000)

    def _reload_paths(self, paths: list[Path], skip: EditorTab | None = None) -> None:
        wanted = {Path(p).resolve() for p in paths}
        for i in range(self.tabs.count()):
            tab = self.tabs.widget(i)
            if tab is not skip and tab.path in wanted and not tab.is_modified():
                tab.reload()

    def _reindex_blueprints(self) -> None:
        for i in range(self.tabs.count()):
            tab = self.tabs.widget(i)
            if isinstance(tab, SpriteEditor):
                tab.sheet._refresh_targets()

    def new_blueprint(self) -> None:
        dialog = NewBlueprintDialog(self.project, self)
        if dialog.exec() != QDialog.Accepted:
            return
        name, kind = dialog.result_values()
        if not valid_identifier(name):
            QMessageBox.warning(self, "Name", "Use letters, digits, '_' and '-' only.")
            return
        if name in self.project.blueprint_files:
            QMessageBox.warning(self, "Name", f"'{name}' already exists.")
            return
        folder = "powerUp" if kind == "powerup" else "descriptionEntity"
        rel = f"assets/{folder}/{name}.json"
        self.project.save_blueprint(name, templates.make(kind), rel)
        self.open_path(self.project.abs(rel))

    def blueprint_from_patch(self, patch: dict) -> None:
        name, ok = QInputDialog.getText(self, "New blueprint", "Name:",
                                        text=self.project.unique_blueprint_name("new_entity"))
        if not ok or not valid_identifier(name):
            return
        if name in self.project.blueprint_files:
            QMessageBox.warning(self, "Name", f"'{name}' already exists.")
            return
        data = templates.make("enemy")
        for key, value in patch.items():
            existing = next((k for k in data["components"] if k.lower() == key.lower()), key)
            if isinstance(value, dict) and isinstance(data["components"].get(existing), dict):
                data["components"][existing].update(copy.deepcopy(value))
            else:
                data["components"][existing] = copy.deepcopy(value)
        rel = f"assets/descriptionEntity/{name}.json"
        self.project.save_blueprint(name, data, rel)
        self.open_path(self.project.abs(rel))

    def apply_to_blueprint(self, name: str, patch: dict) -> None:
        rel = self.project.blueprint_files.get(name)
        if not rel:
            return
        tab = self.open_path(self.project.abs(rel))
        if isinstance(tab, BlueprintEditor):
            tab.apply_patch(patch)
            self.statusBar().showMessage(f"Applied to {name} (not saved yet)", 4000)

    def new_image(self) -> None:
        name, ok = QInputDialog.getText(self, "New image", "File name (without .png):", text="sprite")
        if not ok or not valid_identifier(name):
            return
        w, ok = QInputDialog.getInt(self, "New image", "Width:", 32, 1, 8192)
        if not ok:
            return
        h, ok = QInputDialog.getInt(self, "New image", "Height:", 32, 1, 8192)
        if not ok:
            return
        path = self.project.abs(f"assets/entityTexture/{name}.png")
        if path.exists():
            QMessageBox.warning(self, "Image", f"{path.name} already exists.")
            return
        image = QImage(w, h, QImage.Format_ARGB32)
        image.fill(Qt.transparent)
        path.parent.mkdir(parents=True, exist_ok=True)
        image.save(str(path), "PNG")
        tab = self.open_path(path)
        if isinstance(tab, SpriteEditor):
            tab.tabs.setCurrentWidget(tab.pixels)

    def new_text_file(self) -> None:
        path, _ = QFileDialog.getSaveFileName(self, "New text file", str(self.project.root / "assets"))
        if path:
            Path(path).touch()
            self.open_path(Path(path), "text")
