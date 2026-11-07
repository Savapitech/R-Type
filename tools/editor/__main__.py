from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _dark_palette(app) -> None:
    from PySide6.QtGui import QColor, QPalette
    from PySide6.QtWidgets import QStyleFactory
    app.setStyle(QStyleFactory.create("Fusion"))
    p = QPalette()
    p.setColor(QPalette.Window, QColor(45, 45, 50))
    p.setColor(QPalette.WindowText, QColor(225, 225, 230))
    p.setColor(QPalette.Base, QColor(30, 30, 34))
    p.setColor(QPalette.AlternateBase, QColor(45, 45, 50))
    p.setColor(QPalette.Text, QColor(225, 225, 230))
    p.setColor(QPalette.Button, QColor(58, 58, 64))
    p.setColor(QPalette.ButtonText, QColor(225, 225, 230))
    p.setColor(QPalette.ToolTipBase, QColor(58, 58, 64))
    p.setColor(QPalette.ToolTipText, QColor(225, 225, 230))
    p.setColor(QPalette.Highlight, QColor(60, 120, 200))
    p.setColor(QPalette.HighlightedText, QColor(255, 255, 255))
    app.setPalette(p)


def main(argv: list[str] | None = None) -> int:
    default_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(prog="tools.editor", description="R-Type level/entity editor")
    parser.add_argument("root", nargs="?", default=str(default_root),
                        help="project root containing assets/ (default: this repository)")
    parser.add_argument("--selftest", action="store_true",
                        help="validate the assets and smoke-test the editors, then exit")
    args = parser.parse_args(argv)
    root = Path(args.root).resolve()
    if not (root / "assets").is_dir():
        print(f"error: {root} has no assets/ directory", file=sys.stderr)
        return 2

    if args.selftest:
        from .selftest import run
        return run(root)

    from PySide6.QtWidgets import QApplication
    from .main_window import MainWindow
    from .project import Project

    app = QApplication(sys.argv[:1])
    _dark_palette(app)
    window = MainWindow(Project(root))
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
