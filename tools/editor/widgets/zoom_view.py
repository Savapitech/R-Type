from __future__ import annotations

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QGraphicsView


class ZoomView(QGraphicsView):
    def __init__(self, scene, parent=None):
        super().__init__(scene, parent)
        self.grid_size = 16
        self.show_grid = True
        self.min_zoom, self.max_zoom = 0.05, 64.0
        self._panning = False
        self._pan_last = QPointF()
        self._pending_fit: tuple[QRectF, float] | None = None
        self.setRenderHint(QPainter.Antialiasing, False)
        self.setRenderHint(QPainter.SmoothPixmapTransform, False)
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self.setResizeAnchor(QGraphicsView.AnchorUnderMouse)
        self.setViewportUpdateMode(QGraphicsView.FullViewportUpdate)
        self.setBackgroundBrush(QColor(38, 38, 42))

    @property
    def zoom(self) -> float:
        return self.transform().m11()

    def set_zoom(self, factor: float) -> None:
        factor = max(self.min_zoom, min(self.max_zoom, factor))
        self.resetTransform()
        self.scale(factor, factor)

    def fit_later(self, rect: QRectF, max_zoom: float = 64.0) -> None:
        self._pending_fit = (QRectF(rect), max_zoom)
        self._apply_pending_fit()

    def _apply_pending_fit(self) -> None:
        if self._pending_fit is None or not self.isVisible() or self.viewport().width() < 50:
            return
        rect, max_zoom = self._pending_fit
        self._pending_fit = None
        self.fitInView(rect, Qt.KeepAspectRatio)
        if self.zoom > max_zoom:
            self.set_zoom(max_zoom)
            self.centerOn(rect.center())

    def showEvent(self, event):
        super().showEvent(event)
        self._apply_pending_fit()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._apply_pending_fit()

    def wheelEvent(self, event):
        step = 1.2 if event.angleDelta().y() > 0 else 1 / 1.2
        target = max(self.min_zoom, min(self.max_zoom, self.zoom * step))
        factor = target / self.zoom
        self.scale(factor, factor)
        event.accept()

    def mousePressEvent(self, event):
        if event.button() == Qt.MiddleButton:
            self._panning = True
            self._pan_last = event.position()
            self.setCursor(Qt.ClosedHandCursor)
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._panning:
            delta = event.position() - self._pan_last
            self._pan_last = event.position()
            self.horizontalScrollBar().setValue(
                self.horizontalScrollBar().value() - int(delta.x()))
            self.verticalScrollBar().setValue(
                self.verticalScrollBar().value() - int(delta.y()))
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        if self._panning and event.button() == Qt.MiddleButton:
            self._panning = False
            self.unsetCursor()
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def drawBackground(self, painter, rect: QRectF):
        super().drawBackground(painter, rect)
        step = self.grid_size
        if not self.show_grid or step <= 0 or step * self.zoom < 4:
            return
        left, top = int(rect.left() // step) * step, int(rect.top() // step) * step
        painter.setPen(QPen(QColor(255, 255, 255, 18), 0))
        x = left
        while x < rect.right():
            painter.drawLine(x, int(rect.top()), x, int(rect.bottom()))
            x += step
        y = top
        while y < rect.bottom():
            painter.drawLine(int(rect.left()), y, int(rect.right()), y)
            y += step
