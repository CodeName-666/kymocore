# SPDX-License-Identifier: MIT
from __future__ import annotations
import sys
from pathlib import Path
from PySide6.QtWidgets import QApplication
from ..i18n import set_language
from .main_window import MainWindow, initial_language


def run(project_path=None, mode=None, lang=None):
    app = QApplication.instance() or QApplication(sys.argv)
    set_language(initial_language(lang))
    window = MainWindow(Path(project_path) if project_path else None, mode)
    window.show()
    return app.exec()
