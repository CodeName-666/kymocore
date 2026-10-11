# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
from __future__ import annotations
import sys
from pathlib import Path
from PySide6.QtWidgets import QApplication
from .main_window import MainWindow

def run(project_path=None):
    app=QApplication.instance() or QApplication(sys.argv)
    window=MainWindow(Path(project_path) if project_path else None)
    window.show()
    return app.exec()
