from __future__ import annotations

import json
from pathlib import Path

from PySide6.QtCore import QRect, QSize, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QTextCharFormat, QTextCursor
from PySide6.QtWidgets import (QHBoxLayout, QLabel, QLineEdit, QMessageBox,
                               QPlainTextEdit, QPushButton, QTextEdit, QVBoxLayout,
                               QWidget)

from .. import jsonio
from ..widgets.json_highlighter import JsonHighlighter
from .base import EditorTab


class _LineNumbers(QWidget):
    def __init__(self, editor: "CodeEditor"):
        super().__init__(editor)
        self._editor = editor

    def sizeHint(self) -> QSize:
        return QSize(self._editor.line_number_width(), 0)

    def paintEvent(self, event):
        self._editor.paint_line_numbers(event)


class CodeEditor(QPlainTextEdit):
    INDENT = "  "

    def __init__(self, parent=None):
        super().__init__(parent)
        font = QFont("monospace")
        font.setStyleHint(QFont.Monospace)
        font.setPointSize(10)
        self.setFont(font)
        self.setLineWrapMode(QPlainTextEdit.NoWrap)
        self.setTabStopDistance(self.fontMetrics().horizontalAdvance(" ") * 2)
        self._numbers = _LineNumbers(self)
        self._error_line = -1
        self.blockCountChanged.connect(self._update_margin)
        self.updateRequest.connect(self._update_numbers)
        self.cursorPositionChanged.connect(self._highlight_lines)
        self._update_margin()
        self._highlight_lines()

    def line_number_width(self) -> int:
        digits = len(str(max(1, self.blockCount())))
        return 14 + self.fontMetrics().horizontalAdvance("9") * digits

    def _update_margin(self) -> None:
        self.setViewportMargins(self.line_number_width(), 0, 0, 0)

    def _update_numbers(self, rect, dy) -> None:
        if dy:
            self._numbers.scroll(0, dy)
        else:
            self._numbers.update(0, rect.y(), self._numbers.width(), rect.height())
        if rect.contains(self.viewport().rect()):
            self._update_margin()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        cr = self.contentsRect()
        self._numbers.setGeometry(QRect(cr.left(), cr.top(), self.line_number_width(), cr.height()))

    def paint_line_numbers(self, event) -> None:
        painter = QPainter(self._numbers)
        painter.fillRect(event.rect(), QColor(45, 45, 50))
        block = self.firstVisibleBlock()
        number = block.blockNumber()
        top = round(self.blockBoundingGeometry(block).translated(self.contentOffset()).top())
        bottom = top + round(self.blockBoundingRect(block).height())
        while block.isValid() and top <= event.rect().bottom():
            if block.isVisible() and bottom >= event.rect().top():
                painter.setPen(QColor(230, 90, 90) if number == self._error_line
                               else QColor(130, 130, 140))
                painter.drawText(0, top, self._numbers.width() - 6,
                                 self.fontMetrics().height(), Qt.AlignRight, str(number + 1))
            block = block.next()
            top = bottom
            bottom = top + round(self.blockBoundingRect(block).height())
            number += 1

    def set_error_line(self, line: int) -> None:
        self._error_line = line
        self._highlight_lines()
        self._numbers.update()

    def _highlight_lines(self) -> None:
        selections = []
        current = QTextEdit.ExtraSelection()
        current.format.setBackground(QColor(255, 255, 255, 14))
        current.format.setProperty(QTextCharFormat.FullWidthSelection, True)
        current.cursor = self.textCursor()
        current.cursor.clearSelection()
        selections.append(current)
        if self._error_line >= 0:
            block = self.document().findBlockByNumber(self._error_line)
            if block.isValid():
                err = QTextEdit.ExtraSelection()
                err.format.setBackground(QColor(200, 60, 60, 90))
                err.format.setProperty(QTextCharFormat.FullWidthSelection, True)
                err.cursor = QTextCursor(block)
                selections.append(err)
        self.setExtraSelections(selections)

    def keyPressEvent(self, event):
        if event.key() in (Qt.Key_Return, Qt.Key_Enter) and not event.modifiers():
            cursor = self.textCursor()
            line = cursor.block().text()
            indent = line[:len(line) - len(line.lstrip())]
            before = line[:cursor.positionInBlock()].rstrip()
            if before.endswith(("{", "[")):
                indent += self.INDENT
            cursor.insertText("\n" + indent)
            self.setTextCursor(cursor)
            return
        if event.key() == Qt.Key_Tab and not event.modifiers():
            self.textCursor().insertText(self.INDENT)
            return
        super().keyPressEvent(event)


class TextEditor(EditorTab):
    def __init__(self, path: Path, project=None, parent=None):
        super().__init__(parent)
        self.path = Path(path)
        self.project = project
        self.is_json = self.path.suffix.lower() == ".json"
        self.editor = CodeEditor()
        if self.is_json:
            JsonHighlighter(self.editor.document())

        bar = QHBoxLayout()
        self.find = QLineEdit()
        self.find.setPlaceholderText("Find")
        self.replace = QLineEdit()
        self.replace.setPlaceholderText("Replace")
        bar.addWidget(self.find)
        next_btn = QPushButton("Next")
        next_btn.clicked.connect(self._find_next)
        bar.addWidget(next_btn)
        bar.addWidget(self.replace)
        rep = QPushButton("Replace")
        rep.clicked.connect(self._replace_one)
        bar.addWidget(rep)
        rep_all = QPushButton("Replace all")
        rep_all.clicked.connect(self._replace_all)
        bar.addWidget(rep_all)
        if self.is_json:
            fmt = QPushButton("Format")
            fmt.clicked.connect(self.format_json)
            bar.addWidget(fmt)
        self.find.returnPressed.connect(self._find_next)

        self.status = QLabel()
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addLayout(bar)
        lay.addWidget(self.editor)
        lay.addWidget(self.status)

        self.reload()
        self.editor.document().modificationChanged.connect(self._emit_modified)
        self.editor.cursorPositionChanged.connect(self._show_position)

    def is_modified(self) -> bool:
        return self.editor.document().isModified()

    def reload(self) -> None:
        text = self.path.read_text(encoding="utf-8") if self.path.exists() else ""
        self.editor.setPlainText(text)
        self.editor.document().setModified(False)
        self.editor.set_error_line(-1)
        self.status.clear()

    def save(self) -> bool:
        text = self.editor.toPlainText()
        if self.is_json:
            try:
                json.loads(text)
            except json.JSONDecodeError as err:
                self.editor.set_error_line(err.lineno - 1)
                cursor = self.editor.textCursor()
                cursor.setPosition(self.editor.document().findBlockByNumber(
                    err.lineno - 1).position())
                self.editor.setTextCursor(cursor)
                self.status.setText(f"Invalid JSON, line {err.lineno}: {err.msg}")
                answer = QMessageBox.question(
                    self, "Invalid JSON",
                    f"Line {err.lineno}: {err.msg}\n\nSave anyway?")
                if answer != QMessageBox.Yes:
                    return False
            else:
                self.editor.set_error_line(-1)
        if text and not text.endswith("\n"):
            text += "\n"
        jsonio.write_text_atomic(self.path, text)
        self.editor.document().setModified(False)
        self.status.setText(f"Saved {self.path.name}")
        self.saved.emit(self.path)
        return True

    def format_json(self) -> None:
        try:
            value = json.loads(self.editor.toPlainText())
        except json.JSONDecodeError as err:
            self.editor.set_error_line(err.lineno - 1)
            self.status.setText(f"Invalid JSON, line {err.lineno}: {err.msg}")
            return
        cursor = self.editor.textCursor()
        cursor.select(QTextCursor.Document)
        cursor.insertText(jsonio.dumps(value))
        self.editor.set_error_line(-1)

    def _show_position(self) -> None:
        c = self.editor.textCursor()
        self.status.setText(f"Line {c.blockNumber() + 1}, column {c.positionInBlock() + 1}")

    def _find_next(self) -> None:
        text = self.find.text()
        if not text:
            return
        if not self.editor.find(text):
            self.editor.moveCursor(QTextCursor.Start)
            self.editor.find(text)

    def _replace_one(self) -> None:
        cursor = self.editor.textCursor()
        if cursor.hasSelection() and cursor.selectedText() == self.find.text():
            cursor.insertText(self.replace.text())
        self._find_next()

    def _replace_all(self) -> None:
        text = self.find.text()
        if not text:
            return
        cursor = self.editor.textCursor()
        cursor.beginEditBlock()
        cursor.movePosition(QTextCursor.Start)
        self.editor.setTextCursor(cursor)
        count = 0
        while self.editor.find(text):
            self.editor.textCursor().insertText(self.replace.text())
            count += 1
        cursor.endEditBlock()
        self.status.setText(f"{count} replacement(s)")
