from __future__ import annotations

from PySide6.QtCore import QRegularExpression
from PySide6.QtGui import QColor, QFont, QSyntaxHighlighter, QTextCharFormat


def _fmt(color: str, bold: bool = False) -> QTextCharFormat:
    f = QTextCharFormat()
    f.setForeground(QColor(color))
    if bold:
        f.setFontWeight(QFont.Bold)
    return f


class JsonHighlighter(QSyntaxHighlighter):
    def __init__(self, document):
        super().__init__(document)
        self._rules = [
            (QRegularExpression(r"-?\b\d+(\.\d+)?([eE][+-]?\d+)?\b"), _fmt("#d19a66")),
            (QRegularExpression(r"\b(true|false|null)\b"), _fmt("#c678dd", True)),
            (QRegularExpression(r'"(\\.|[^"\\])*"'), _fmt("#98c379")),
            (QRegularExpression(r'"(\\.|[^"\\])*"(?=\s*:)'), _fmt("#61afef")),
            (QRegularExpression(r"[{}\[\],:]"), _fmt("#abb2bf")),
        ]

    def highlightBlock(self, text: str) -> None:
        for regex, fmt in self._rules:
            it = regex.globalMatch(text)
            while it.hasNext():
                m = it.next()
                self.setFormat(m.capturedStart(), m.capturedLength(), fmt)
