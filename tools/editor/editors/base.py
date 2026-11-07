from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Signal
from PySide6.QtGui import QUndoStack
from PySide6.QtWidgets import QWidget

from ..document import Document


class EditorTab(QWidget):
    modifiedChanged = Signal()
    saved = Signal(object)
    openRequested = Signal(object, str)

    path: Path | None = None
    undo_stack: QUndoStack | None = None

    def _emit_modified(self, *_args) -> None:
        self.modifiedChanged.emit()

    def title(self) -> str:
        return self.path.name if self.path else "untitled"

    def is_modified(self) -> bool:
        return False

    def save(self) -> bool:
        return True

    def reload(self) -> None:
        pass


class DocumentTab(EditorTab):
    """Tab editing a JSON file through an undoable Document.

    Subclasses implement load() (parse and normalize the file), write() (store
    doc.data) and refresh() (redraw after any change of the document).
    """

    def __init__(self, path: Path, project, parent=None):
        super().__init__(parent)
        self.path, self.project = Path(path), project
        self.doc = Document(self.load(), self)
        self.undo_stack = self.doc.stack
        self.doc.changed.connect(self._on_doc_changed)
        self.doc.stack.cleanChanged.connect(self._emit_modified)

    def load(self) -> dict:
        raise NotImplementedError

    def write(self) -> None:
        raise NotImplementedError

    def refresh(self) -> None:
        raise NotImplementedError

    def _on_doc_changed(self) -> None:
        self.refresh()
        self.modifiedChanged.emit()

    def is_modified(self) -> bool:
        return self.doc.modified

    def reload(self) -> None:
        self.doc.replace(self.load())
        self.modifiedChanged.emit()

    def save(self) -> bool:
        self.write()
        self.doc.mark_saved()
        self.saved.emit(self.path)
        self.modifiedChanged.emit()
        return True
