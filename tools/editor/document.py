from __future__ import annotations

import copy
from typing import Any, Callable

from PySide6.QtCore import QObject, Signal
from PySide6.QtGui import QUndoCommand, QUndoStack


class _SnapshotCommand(QUndoCommand):
    def __init__(self, doc: "Document", before: Any, after: Any, label: str,
                 merge_key: str | None):
        super().__init__(label)
        self._doc = doc
        self._before = before
        self._after = after
        self._merge_key = merge_key

    def id(self) -> int:
        return hash(self._merge_key) & 0x7FFFFFFF if self._merge_key else -1

    def mergeWith(self, other: QUndoCommand) -> bool:
        if not isinstance(other, _SnapshotCommand):
            return False
        if self._merge_key is None or other._merge_key != self._merge_key:
            return False
        self._after = other._after
        return True

    def undo(self) -> None:
        self._doc._set(copy.deepcopy(self._before))

    def redo(self) -> None:
        self._doc._set(copy.deepcopy(self._after))


class Document(QObject):

    changed = Signal()

    def __init__(self, data: Any, parent: QObject | None = None):
        super().__init__(parent)
        self.data = data
        self.stack = QUndoStack(self)

    def _set(self, data: Any) -> None:
        self.data = data
        self.changed.emit()

    def edit(self, label: str, fn: Callable[[Any], None],
             merge_key: str | None = None) -> bool:
        new = copy.deepcopy(self.data)
        fn(new)
        if new == self.data:
            return False
        self.stack.push(_SnapshotCommand(self, copy.deepcopy(self.data), new,
                                         label, merge_key))
        return True

    def replace(self, data: Any) -> None:
        self.stack.clear()
        self._set(data)

    @property
    def modified(self) -> bool:
        return not self.stack.isClean()

    def mark_saved(self) -> None:
        self.stack.setClean()
