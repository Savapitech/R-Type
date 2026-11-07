from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QBuffer, QByteArray, QIODevice, QPoint, QPointF, QRect, QRectF, Qt, QTimer, Signal
from PySide6.QtGui import (QBrush, QColor, QImage, QPainter, QPen, QPixmap, QUndoCommand,
                           QUndoStack)
from PySide6.QtWidgets import (QButtonGroup, QCheckBox, QColorDialog, QComboBox,
                               QGraphicsRectItem, QGraphicsScene, QGroupBox, QHBoxLayout,
                               QInputDialog, QLabel, QListWidget, QPushButton, QScrollArea,
                               QSplitter, QTabWidget, QVBoxLayout, QWidget)

from .. import jsonio, schema
from ..project import Project, animation_rects
from ..widgets.property_form import FieldsForm
from ..widgets.zoom_view import ZoomView
from .base import EditorTab


def load_argb(path: Path) -> QImage:
    image = QImage(str(path))
    if image.isNull():
        image = QImage(32, 32, QImage.Format_ARGB32)
        image.fill(Qt.transparent)
    return image.convertToFormat(QImage.Format_ARGB32)


def image_to_png_bytes(image: QImage) -> bytes:
    data = QByteArray()
    buf = QBuffer(data)
    buf.open(QIODevice.WriteOnly)
    image.save(buf, "PNG")
    return bytes(data)


class SheetView(ZoomView):
    selectionChanged = Signal(QRect)

    def __init__(self, scene):
        super().__init__(scene)
        self.show_grid = False
        self._origin: QPointF | None = None
        self.cell = (32, 32)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._origin = self.mapToScene(event.position().toPoint())
            self._emit(self._origin)
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._origin is not None and event.buttons() & Qt.LeftButton:
            self._emit(self.mapToScene(event.position().toPoint()))
            event.accept()
            return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton and self._origin is not None:
            end = self.mapToScene(event.position().toPoint())
            if (end - self._origin).manhattanLength() < 2:
                cw, ch = max(1, self.cell[0]), max(1, self.cell[1])
                x, y = int(self._origin.x() // cw) * cw, int(self._origin.y() // ch) * ch
                self.selectionChanged.emit(QRect(x, y, cw, ch))
            self._origin = None
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def _emit(self, point: QPointF) -> None:
        a = QPoint(int(self._origin.x()), int(self._origin.y()))
        b = QPoint(int(point.x()), int(point.y()))
        rect = QRect(a, b).normalized()
        self.selectionChanged.emit(QRect(rect.x(), rect.y(), max(1, rect.width()), max(1, rect.height())))


SELECTION_FIELDS = [schema.rect("selection")]
DISPLAY_FIELDS = [schema.size2("size")]
ANIMATION_FIELDS = schema.fields_of("animation")
CLIP_KEYS = ("startX", "startY", "totalFrames", "frameDuration", "loop", "reverse")


class SpritesheetTab(QWidget):
    applyRequested = Signal(str, dict)
    newBlueprintRequested = Signal(dict)

    def __init__(self, project: Project, path: Path, parent=None):
        super().__init__(parent)
        self.project, self.path = project, path
        self.image = QImage()
        self.clips: dict[str, dict] = {}
        self.scene = QGraphicsScene(self)
        self.view = SheetView(self.scene)
        self.view.selectionChanged.connect(self.set_selection)
        self._sel_item: QGraphicsRectItem | None = None
        self._frame_items: list[QGraphicsRectItem] = []
        self._frames: list[QPixmap] = []
        self._index = 0
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)

        self.selection = FieldsForm(SELECTION_FIELDS)
        self.selection.set_data({"selection": SELECTION_FIELDS[0]["default"]})
        self.selection.edited.connect(self._update_selection_item)
        self.anim = FieldsForm(ANIMATION_FIELDS)
        self.anim.set_data(schema.default_component("animation"))
        self.anim.edited.connect(self._params_changed)
        self.display = FieldsForm(DISPLAY_FIELDS)
        self.display.set_data({"size": DISPLAY_FIELDS[0]["default"]})
        self.animated = QCheckBox("Animated sprite")

        self.preview = QLabel()
        self.preview.setMinimumSize(128, 128)
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setStyleSheet("background:#222;")
        self.clip_list = QListWidget()
        self.clip_list.setMaximumHeight(90)
        self.clip_list.currentTextChanged.connect(self._load_clip)
        self.clip_name = QComboBox()
        self.clip_name.setEditable(True)
        self.clip_name.addItems(schema.ANIM_STATES)
        self.target = QComboBox()
        self.target.addItems(project.blueprint_names())

        side = QVBoxLayout()
        side.addWidget(self._box("Selection (drag on image, click = grid cell)", self.selection,
                                 self._button("Selection to first frame (+ display size)",
                                              self._selection_to_frame),
                                 self._button("Auto columns / frames from image", self._auto_grid)))
        side.addWidget(self._box("Animation (computed like AnimationSystem)", self.anim, self.display))
        side.addWidget(self.preview)
        side.addWidget(self._box("Animator clips", self.clip_list, self.clip_name,
                                 self._button("Set from animation", self._save_clip),
                                 self._button("Delete", self._delete_clip)))
        side.addWidget(self._box("Export to a blueprint", self.animated, self.target,
                                 self._button("Apply", self._apply),
                                 self._button("New blueprint from this...",
                                              lambda: self.newBlueprintRequested.emit(self.build_patch()))))
        side.addStretch(1)
        panel = QWidget()
        panel.setLayout(side)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(panel)
        scroll.setMinimumWidth(330)
        split = QSplitter()
        split.addWidget(self.view)
        split.addWidget(scroll)
        split.setStretchFactor(0, 1)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(split)
        project.blueprintsChanged.connect(self._refresh_targets)

    @staticmethod
    def _button(text: str, slot) -> QPushButton:
        button = QPushButton(text)
        button.clicked.connect(slot)
        return button

    @staticmethod
    def _box(title: str, *widgets: QWidget) -> QGroupBox:
        box = QGroupBox(title)
        column = QVBoxLayout(box)
        for widget in widgets:
            column.addWidget(widget)
        return box

    def set_image(self, image: QImage) -> None:
        self.image = image
        self.scene.clear()
        self._frame_items = []
        checker = QPixmap(16, 16)
        checker.fill(QColor(70, 70, 76))
        painter = QPainter(checker)
        painter.fillRect(0, 0, 8, 8, QColor(90, 90, 96))
        painter.fillRect(8, 8, 8, 8, QColor(90, 90, 96))
        painter.end()
        background = self.scene.addRect(0, 0, image.width(), image.height(), QPen(Qt.NoPen),
                                        QBrush(checker))
        background.setZValue(-1)
        self.scene.addPixmap(QPixmap.fromImage(image))
        self._sel_item = QGraphicsRectItem()
        self._sel_item.setPen(QPen(QColor(255, 220, 60), 0))
        self._sel_item.setBrush(QBrush(QColor(255, 220, 60, 40)))
        self._sel_item.setZValue(3)
        self.scene.addItem(self._sel_item)
        self._update_selection_item()
        self.scene.setSceneRect(-20, -20, image.width() + 40, image.height() + 40)
        self.view.fit_later(self.scene.sceneRect())
        self._params_changed()

    def _refresh_targets(self) -> None:
        current = self.target.currentText()
        self.target.clear()
        self.target.addItems(self.project.blueprint_names())
        self.target.setCurrentText(current)

    def _selection_rect(self) -> dict:
        return self.selection.data()["selection"]

    def set_selection(self, rect: QRect) -> None:
        self.selection.set_data({"selection": {"x": rect.x(), "y": rect.y(),
                                               "w": rect.width(), "h": rect.height()}})
        self._update_selection_item()

    def _update_selection_item(self, *_args) -> None:
        if self._sel_item is not None:
            r = self._selection_rect()
            self._sel_item.setRect(QRectF(r["x"], r["y"], r["w"], r["h"]))

    def _selection_to_frame(self) -> None:
        r = self._selection_rect()
        self.anim.set_data({**self.anim.data(), "startX": r["x"], "startY": r["y"],
                            "frameWidth": r["w"], "frameHeight": r["h"]})
        self.display.set_data({"size": {"w": float(r["w"]), "h": float(r["h"])}})
        self._params_changed()

    def _auto_grid(self) -> None:
        p = self.anim.data()
        columns = max(1, (self.image.width() - p["startX"] + p["paddingX"])
                      // max(1, p["frameWidth"] + p["paddingX"]))
        rows = max(1, (self.image.height() - p["startY"] + p["paddingY"])
                   // max(1, p["frameHeight"] + p["paddingY"]))
        self.anim.set_data({**p, "columns": columns, "totalFrames": columns * rows})
        self._params_changed()

    def _params_changed(self, *_args) -> None:
        params = self.anim.data()
        for item in self._frame_items:
            self.scene.removeItem(item)
        self._frame_items, self._frames = [], []
        bounds = QRect(0, 0, self.image.width(), self.image.height())
        for i, rect in enumerate(animation_rects(params)):
            item = QGraphicsRectItem(QRectF(rect))
            item.setPen(QPen(QColor(80, 220, 255, 200 if i == 0 else 120), 0))
            item.setZValue(2)
            self.scene.addItem(item)
            self._frame_items.append(item)
            if bounds.contains(rect):
                self._frames.append(QPixmap.fromImage(self.image.copy(rect)))
        self._index = len(self._frames) - 1 if params["reverse"] and self._frames else 0
        self._timer.stop()
        self._show_frame()
        if len(self._frames) > 1:
            self._timer.start(max(10, int(params["frameDuration"] * 1000)))

    def _show_frame(self) -> None:
        if not self._frames:
            self.preview.clear()
            return
        pix = self._frames[self._index]
        factor = max(1, min(8, 128 // max(1, pix.width(), pix.height())))
        self.preview.setPixmap(pix.scaled(pix.width() * factor, pix.height() * factor,
                                          Qt.KeepAspectRatio, Qt.FastTransformation))

    def _tick(self) -> None:
        params = self.anim.data()
        step = self._index + (-1 if params["reverse"] else 1)
        if 0 <= step < len(self._frames):
            self._index = step
        elif params["loop"]:
            self._index = step % len(self._frames)
        self._show_frame()

    def set_clips(self, clips: dict) -> None:
        self.clips = dict(clips)
        self.clip_list.blockSignals(True)
        self.clip_list.clear()
        self.clip_list.addItems(list(self.clips))
        self.clip_list.blockSignals(False)

    def _save_clip(self) -> None:
        name = self.clip_name.currentText().strip()
        if name:
            params = self.anim.data()
            self.clips[name] = {key: params[key] for key in CLIP_KEYS}
            self.set_clips(self.clips)

    def _delete_clip(self) -> None:
        item = self.clip_list.currentItem()
        if item:
            self.clips.pop(item.text(), None)
            self.set_clips(self.clips)

    def _load_clip(self, name: str) -> None:
        if name in self.clips:
            self.clip_name.setCurrentText(name)
            self.anim.set_data({**self.anim.data(), **self.clips[name]})
            self._params_changed()

    def build_patch(self) -> dict:
        params, size = self.anim.data(), self.display.data()["size"]
        if self.animated.isChecked():
            rect = {"x": params["startX"], "y": params["startY"],
                    "w": params["frameWidth"], "h": params["frameHeight"]}
        else:
            rect = self._selection_rect()
        patch: dict = {"sprite": {"texture": self.project.rel(self.path), "size": size,
                                  "textureRect": rect}}
        if self.animated.isChecked():
            patch["animation"] = params
            if self.clips:
                first = "IDLE" if "IDLE" in self.clips else next(iter(self.clips))
                patch["animator"] = {"currentState": first, "clips": dict(self.clips)}
        return patch

    def _apply(self) -> None:
        name = self.target.currentText()
        if name:
            self.applyRequested.emit(name, self.build_patch())


class _ImageCommand(QUndoCommand):
    def __init__(self, canvas: "PixelCanvas", before: QImage, after: QImage, label: str):
        super().__init__(label)
        self.canvas, self.before, self.after = canvas, before, after

    def undo(self) -> None:
        self.canvas.set_image(self.before.copy(), keep_view=True)

    def redo(self) -> None:
        self.canvas.set_image(self.after.copy(), keep_view=True)


TOOLS = ["Pencil", "Eraser", "Fill", "Picker", "Line", "Rectangle"]


def _line_points(x0: int, y0: int, x1: int, y1: int):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        yield x0, y0
        if x0 == x1 and y0 == y1:
            return
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


class PixelCanvas(QWidget):
    colorPicked = Signal(QColor)
    edited = Signal()

    def __init__(self, stack: QUndoStack, parent=None):
        super().__init__(parent)
        self.stack = stack
        self.image = QImage(32, 32, QImage.Format_ARGB32)
        self.image.fill(Qt.transparent)
        self.zoom, self.offset = 12, QPoint(20, 20)
        self.tool, self.color = "Pencil", QColor(255, 255, 255)
        self.mirror_h = self.mirror_v = False
        self.filled_rect = False
        self._stroke_before: QImage | None = None
        self._last: tuple[int, int] | None = None
        self._start: tuple[int, int] | None = None
        self._base: QImage | None = None
        self._panning = False
        self._pan_last = QPoint()
        self.setMouseTracking(True)
        self.setMinimumSize(300, 300)

    def set_image(self, image: QImage, keep_view: bool = False) -> None:
        self.image = image.convertToFormat(QImage.Format_ARGB32)
        if not keep_view:
            self.zoom = max(1, min(24, 700 // max(1, max(image.width(), image.height()))))
            self.offset = QPoint(20, 20)
        self.update()
        self.edited.emit()

    def _to_pixel(self, pos) -> tuple[int, int]:
        p = pos if isinstance(pos, QPoint) else pos.toPoint()
        return ((p.x() - self.offset.x()) // self.zoom, (p.y() - self.offset.y()) // self.zoom)

    def paintEvent(self, event):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(38, 38, 42))
        target = QRect(self.offset, self.image.size() * self.zoom)
        tile = max(4, self.zoom // 2)
        p.save()
        p.setClipRect(target)
        for ty in range(0, target.height(), tile):
            for tx in range(0, target.width(), tile):
                dark = ((tx // tile) + (ty // tile)) % 2
                p.fillRect(target.x() + tx, target.y() + ty, tile, tile,
                           QColor(80, 80, 86) if dark else QColor(105, 105, 112))
        p.restore()
        p.drawImage(target, self.image)
        if self.zoom >= 6:
            p.setPen(QPen(QColor(0, 0, 0, 55), 1))
            for x in range(self.image.width() + 1):
                p.drawLine(target.x() + x * self.zoom, target.y(),
                           target.x() + x * self.zoom, target.bottom() + 1)
            for y in range(self.image.height() + 1):
                p.drawLine(target.x(), target.y() + y * self.zoom,
                           target.right() + 1, target.y() + y * self.zoom)
        p.setPen(QPen(QColor(255, 255, 255, 120), 1))
        p.drawRect(target.adjusted(-1, -1, 0, 0))

    def wheelEvent(self, event):
        old = self.zoom
        self.zoom = max(1, min(64, old + (1 if event.angleDelta().y() > 0 else -1)))
        pos = event.position().toPoint()
        self.offset = pos - (pos - self.offset) * self.zoom / old
        self.update()

    def _plot(self, img: QImage, x: int, y: int, color: QColor) -> None:
        points = {(x, y)}
        w, h = img.width(), img.height()
        if self.mirror_h:
            points |= {(w - 1 - px, py) for px, py in points}
        if self.mirror_v:
            points |= {(px, h - 1 - py) for px, py in points}
        for px, py in points:
            if 0 <= px < w and 0 <= py < h:
                img.setPixelColor(px, py, color)

    def _paint_color(self) -> QColor:
        return QColor(0, 0, 0, 0) if self.tool == "Eraser" else self.color

    def _flood(self, x: int, y: int) -> None:
        w, h = self.image.width(), self.image.height()
        if not (0 <= x < w and 0 <= y < h):
            return
        target = self.image.pixel(x, y)
        new = self.color.rgba()
        if target == new:
            return
        stack = [(x, y)]
        while stack:
            cx, cy = stack.pop()
            if not (0 <= cx < w and 0 <= cy < h) or self.image.pixel(cx, cy) != target:
                continue
            self._plot(self.image, cx, cy, self.color) if (self.mirror_h or self.mirror_v) \
                else self.image.setPixelColor(cx, cy, self.color)
            stack += [(cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)]

    def mousePressEvent(self, event):
        if event.button() == Qt.MiddleButton:
            self._panning, self._pan_last = True, event.position().toPoint()
            return
        if event.button() != Qt.LeftButton:
            return
        x, y = self._to_pixel(event.position())
        self._stroke_before = self.image.copy()
        self._start = (x, y)
        if self.tool == "Picker":
            if 0 <= x < self.image.width() and 0 <= y < self.image.height():
                self.colorPicked.emit(self.image.pixelColor(x, y))
            self._stroke_before = None
            return
        if self.tool == "Fill":
            self._flood(x, y)
            self._commit("Fill")
            return
        if self.tool in ("Pencil", "Eraser"):
            self._plot(self.image, x, y, self._paint_color())
            self._last = (x, y)
        else:
            self._base = self.image.copy()
        self.update()

    def mouseMoveEvent(self, event):
        if self._panning:
            cur = event.position().toPoint()
            self.offset += cur - self._pan_last
            self._pan_last = cur
            self.update()
            return
        if not (event.buttons() & Qt.LeftButton) or self._stroke_before is None:
            return
        x, y = self._to_pixel(event.position())
        if self.tool in ("Pencil", "Eraser") and self._last:
            for px, py in _line_points(*self._last, x, y):
                self._plot(self.image, px, py, self._paint_color())
            self._last = (x, y)
        elif self.tool in ("Line", "Rectangle") and self._base is not None and self._start:
            self.image = self._base.copy()
            self._draw_shape(self._start, (x, y))
        self.update()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MiddleButton:
            self._panning = False
            return
        if event.button() == Qt.LeftButton and self._stroke_before is not None:
            self._commit(self.tool)
        self._last, self._base = None, None

    def _draw_shape(self, a: tuple[int, int], b: tuple[int, int]) -> None:
        if self.tool == "Line":
            for px, py in _line_points(*a, *b):
                self._plot(self.image, px, py, self.color)
            return
        x0, x1 = sorted((a[0], b[0]))
        y0, y1 = sorted((a[1], b[1]))
        for py in range(y0, y1 + 1):
            for px in range(x0, x1 + 1):
                if self.filled_rect or px in (x0, x1) or py in (y0, y1):
                    self._plot(self.image, px, py, self.color)

    def _commit(self, label: str) -> None:
        before, after = self._stroke_before, self.image.copy()
        self._stroke_before = None
        if before is not None and before != after:
            self.stack.push(_ImageCommand(self, before, after, label))
        self.update()

    def apply_operation(self, label: str, new_image: QImage) -> None:
        self.stack.push(_ImageCommand(self, self.image.copy(), new_image, label))

    def flip(self, horizontal: bool) -> None:
        self.apply_operation("Flip", self.image.mirrored(horizontal, not horizontal))

    def shift(self, dx: int, dy: int) -> None:
        out = QImage(self.image.size(), QImage.Format_ARGB32)
        out.fill(Qt.transparent)
        p = QPainter(out)
        p.drawImage(dx, dy, self.image)
        p.end()
        self.apply_operation("Shift", out)

    def resize_canvas(self, w: int, h: int) -> None:
        out = QImage(w, h, QImage.Format_ARGB32)
        out.fill(Qt.transparent)
        p = QPainter(out)
        p.drawImage(0, 0, self.image)
        p.end()
        self.apply_operation("Resize canvas", out)


class PixelArtTab(QWidget):
    def __init__(self, stack: QUndoStack, parent=None):
        super().__init__(parent)
        self.canvas = PixelCanvas(stack)
        self.canvas.colorPicked.connect(self.set_color)
        side = QWidget()
        sv = QVBoxLayout(side)
        group = QButtonGroup(self)
        group.setExclusive(True)
        for name in TOOLS:
            b = QPushButton(name)
            b.setCheckable(True)
            b.setChecked(name == "Pencil")
            b.clicked.connect(lambda _=False, n=name: setattr(self.canvas, "tool", n))
            group.addButton(b)
            sv.addWidget(b)
        self.color_btn = QPushButton("Color...")
        self.color_btn.clicked.connect(self._choose_color)
        sv.addWidget(self.color_btn)
        self.recent = QHBoxLayout()
        sv.addLayout(self.recent)
        self._recent_colors: list[QColor] = []
        mh, mv = QCheckBox("Mirror horizontal"), QCheckBox("Mirror vertical")
        fr = QCheckBox("Filled rectangle")
        mh.toggled.connect(lambda v: setattr(self.canvas, "mirror_h", v))
        mv.toggled.connect(lambda v: setattr(self.canvas, "mirror_v", v))
        fr.toggled.connect(lambda v: setattr(self.canvas, "filled_rect", v))
        for w in (mh, mv, fr):
            sv.addWidget(w)
        for label, fn in (("Flip horizontal", lambda: self.canvas.flip(True)),
                          ("Flip vertical", lambda: self.canvas.flip(False)),
                          ("Shift ←", lambda: self.canvas.shift(-1, 0)),
                          ("Shift →", lambda: self.canvas.shift(1, 0)),
                          ("Shift ↑", lambda: self.canvas.shift(0, -1)),
                          ("Shift ↓", lambda: self.canvas.shift(0, 1)),
                          ("Resize canvas...", self._resize),
                          ("Add frame (extend right)...", self._add_frame),
                          ("Clear", self._clear)):
            b = QPushButton(label)
            b.clicked.connect(fn)
            sv.addWidget(b)
        sv.addStretch()
        split = QSplitter()
        split.addWidget(self.canvas)
        split.addWidget(side)
        split.setStretchFactor(0, 1)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(split)
        self.set_color(QColor(255, 255, 255))

    def set_color(self, color: QColor) -> None:
        self.canvas.color = QColor(color)
        text = "black" if color.lightness() > 128 else "white"
        self.color_btn.setStyleSheet(
            f"background: rgba({color.red()},{color.green()},{color.blue()},{color.alpha()}); color: {text};")
        if all(color != c for c in self._recent_colors):
            self._recent_colors.insert(0, QColor(color))
            self._recent_colors = self._recent_colors[:12]
            while self.recent.count():
                item = self.recent.takeAt(0)
                if item.widget():
                    item.widget().deleteLater()
            for c in self._recent_colors:
                b = QPushButton()
                b.setFixedSize(18, 18)
                b.setStyleSheet(f"background: rgba({c.red()},{c.green()},{c.blue()},{c.alpha()});")
                b.clicked.connect(lambda _=False, col=QColor(c): self.set_color(col))
                self.recent.addWidget(b)

    def _choose_color(self) -> None:
        c = QColorDialog.getColor(self.canvas.color, self, "Color", QColorDialog.ShowAlphaChannel)
        if c.isValid():
            self.set_color(c)

    def _resize(self) -> None:
        w, ok = QInputDialog.getInt(self, "Resize canvas", "Width:", self.canvas.image.width(), 1, 8192)
        if not ok:
            return
        h, ok = QInputDialog.getInt(self, "Resize canvas", "Height:", self.canvas.image.height(), 1, 8192)
        if ok:
            self.canvas.resize_canvas(w, h)

    def _add_frame(self) -> None:
        w, ok = QInputDialog.getInt(self, "Add frame", "Frame width:", self.canvas.image.height(), 1, 8192)
        if ok:
            self.canvas.resize_canvas(self.canvas.image.width() + w, self.canvas.image.height())

    def _clear(self) -> None:
        out = QImage(self.canvas.image.size(), QImage.Format_ARGB32)
        out.fill(Qt.transparent)
        self.canvas.apply_operation("Clear", out)


class SpriteEditor(EditorTab):
    applyRequested = Signal(str, dict)
    newBlueprintRequested = Signal(dict)

    def __init__(self, path: Path, project: Project, parent=None):
        super().__init__(parent)
        self.path, self.project = Path(path), project
        self.undo_stack = QUndoStack(self)
        self.undo_stack.cleanChanged.connect(self._emit_modified)
        self.sheet = SpritesheetTab(project, self.path)
        self.pixels = PixelArtTab(self.undo_stack)
        self.sheet.applyRequested.connect(self.applyRequested)
        self.sheet.newBlueprintRequested.connect(self.newBlueprintRequested)
        self.tabs = QTabWidget()
        self.tabs.addTab(self.sheet, "Spritesheet")
        self.tabs.addTab(self.pixels, "Pixel art")
        self.tabs.currentChanged.connect(self._tab_changed)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(self.tabs)
        self.reload()

    def _tab_changed(self, index: int) -> None:
        if index == 0:
            self.sheet.set_image(self.pixels.canvas.image.copy())

    def title(self) -> str:
        return self.path.name

    def is_modified(self) -> bool:
        return not self.undo_stack.isClean()

    def reload(self) -> None:
        image = load_argb(self.path)
        self.undo_stack.clear()
        self.pixels.canvas.set_image(image)
        self.sheet.set_image(image.copy())
        self._load_blueprint_clips()
        self.modifiedChanged.emit()

    def _load_blueprint_clips(self) -> None:
        rel = self.project.rel(self.path)
        for name in self.project.blueprint_names():
            try:
                data = self.project.load_blueprint(name)
            except (OSError, ValueError, KeyError):
                continue
            sprite = next((v for k, v in data.get("components", {}).items() if k.lower() == "sprite"), {})
            if sprite.get("texture") == rel:
                self.sheet.target.setCurrentText(name)
                break

    def save(self) -> bool:
        jsonio.write_bytes_atomic(self.path, image_to_png_bytes(self.pixels.canvas.image))
        self.project.invalidate_texture()
        self.undo_stack.setClean()
        self.sheet.set_image(self.pixels.canvas.image.copy())
        self.saved.emit(self.path)
        self.modifiedChanged.emit()
        return True
