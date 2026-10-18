# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Main window: start page, guided mode and professional mode on one session."""
from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QKeySequence
from PySide6.QtWidgets import (QComboBox, QFileDialog, QFrame, QHBoxLayout, QLabel, QMainWindow,
                               QMessageBox, QPushButton, QStackedWidget, QToolBar, QVBoxLayout,
                               QWidget)

from ..model import CORE_CONFIG_API_VERSION
from .guided import GuidedWizard
from .professional import ProfessionalEditor
from .session import ProjectSession
from .style import STYLE

GUIDED = "guided"
PROFESSIONAL = "professional"


class StartPage(QWidget):
    """Choose how to work; shown when no mode is given on the command line."""

    def __init__(self, window: "MainWindow"):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(48, 36, 48, 36)
        title = QLabel("KymoCore Configurator")
        title.setObjectName("startTitle")
        layout.addWidget(title)
        intro = QLabel(f"Creates the build configuration for KymoCore 7 (configuration API "
                       f"{CORE_CONFIG_API_VERSION}): which features are compiled in, which channels are "
                       "sent and which of your functions connect KymoCore to the hardware.")
        intro.setWordWrap(True)
        intro.setObjectName("sectionIntro")
        layout.addWidget(intro)
        cards = QHBoxLayout()
        for mode, heading, text, button_text in (
            (GUIDED, "Guided setup",
             "Step by step: KymoCore asks what you want to achieve and only shows what matters for it. "
             "Advanced options keep safe defaults. Recommended for the first project.",
             "Start guided setup"),
            (PROFESSIONAL, "Professional mode",
             "All settings at once, grouped by topic, with live status, diagnostics and a preview "
             "of the generated files. For users who know KymoCore.",
             "Open professional mode"),
        ):
            card = QFrame()
            card.setObjectName("modeCard")
            card_layout = QVBoxLayout(card)
            label = QLabel(heading)
            label.setObjectName("modeTitle")
            body = QLabel(text)
            body.setWordWrap(True)
            body.setObjectName("fieldHelp")
            button = QPushButton(button_text)
            button.setObjectName("primaryButton")
            button.clicked.connect(lambda _=False, m=mode: window.set_mode(m))
            card_layout.addWidget(label)
            card_layout.addWidget(body, 1)
            card_layout.addWidget(button)
            cards.addWidget(card)
        layout.addLayout(cards)
        files = QHBoxLayout()
        open_button = QPushButton("Open existing project…")
        open_button.clicked.connect(window.open_project)
        files.addWidget(open_button)
        files.addStretch(1)
        layout.addLayout(files)
        layout.addStretch(1)


class MainWindow(QMainWindow):
    def __init__(self, path: Path | None = None, mode: str | None = None):
        super().__init__()
        self.setWindowTitle("KymoCore Configurator")
        self.resize(1180, 800)
        self.setStyleSheet(STYLE)
        self.session = ProjectSession()
        if path is not None and Path(path).exists():
            self.session.load(path)
        elif path is not None:
            self.session.path = Path(path)
        self._build_toolbar()
        central = QWidget()
        outer = QVBoxLayout(central)
        outer.setContentsMargins(0, 0, 0, 0)
        self.notice = QLabel("")
        self.notice.setObjectName("notice")
        self.notice.setWordWrap(True)
        outer.addWidget(self.notice)
        self.stack = QStackedWidget()
        outer.addWidget(self.stack, 1)
        self.setCentralWidget(central)
        self.start_page = StartPage(self)
        self.guided = GuidedWizard(self.session)
        self.professional = ProfessionalEditor(self.session)
        self.guided.switch_to_professional.connect(self._open_in_professional)
        for page in (self.start_page, self.guided, self.professional):
            self.stack.addWidget(page)
        self.session.changed.connect(self._update_chrome)
        self.session.replaced.connect(self._load_profiles)
        self._load_profiles()
        self.set_mode(mode) if mode in (GUIDED, PROFESSIONAL) else self.stack.setCurrentWidget(self.start_page)
        self._update_chrome()

    # --- chrome -------------------------------------------------------------
    def _build_toolbar(self) -> None:
        bar = QToolBar("Project")
        bar.setMovable(False)
        self.addToolBar(bar)
        for text, shortcut, slot in (("New", QKeySequence.StandardKey.New, self.new_project),
                                     ("Open…", QKeySequence.StandardKey.Open, self.open_project),
                                     ("Save", QKeySequence.StandardKey.Save, self.save),
                                     ("Save as…", QKeySequence.StandardKey.SaveAs, self.save_as)):
            action = QAction(text, self)
            action.setShortcut(shortcut)
            action.triggered.connect(slot)
            bar.addAction(action)
        bar.addSeparator()
        bar.addWidget(QLabel(" Profile "))
        self.profile = QComboBox()
        self.profile.setToolTip("A profile is one build variant, e.g. development or production.")
        self.profile.currentTextChanged.connect(self.session.set_profile)
        bar.addWidget(self.profile)
        duplicate = QAction("Duplicate profile", self)
        duplicate.triggered.connect(self.session.duplicate_profile)
        bar.addAction(duplicate)
        spacer = QWidget()
        spacer.setSizePolicy(spacer.sizePolicy().horizontalPolicy().Expanding,
                             spacer.sizePolicy().verticalPolicy().Preferred)
        bar.addWidget(spacer)
        bar.addWidget(QLabel("Mode "))
        self.guided_action = QAction("Guided", self, checkable=True)
        self.professional_action = QAction("Professional", self, checkable=True)
        self.guided_action.triggered.connect(lambda: self.set_mode(GUIDED))
        self.professional_action.triggered.connect(lambda: self.set_mode(PROFESSIONAL))
        bar.addAction(self.guided_action)
        bar.addAction(self.professional_action)

    def _load_profiles(self) -> None:
        self.profile.blockSignals(True)
        self.profile.clear()
        self.profile.addItems(self.session.profiles)
        self.profile.setCurrentText(self.session.profile)
        self.profile.blockSignals(False)

    def _update_chrome(self) -> None:
        name = self.session.path.name if self.session.path else "unsaved project"
        self.setWindowTitle(f"KymoCore Configurator — {name}{' *' if self.session.dirty else ''}")
        errors = len(self.session.errors())
        warnings = len(self.session.diagnostics()) - errors
        state = "ready to generate" if not errors else f"{errors} problem(s) to fix"
        if warnings:
            state += f", {warnings} warning(s)"
        self.statusBar().showMessage(f"Profile '{self.session.profile}': {state}")
        self.notice.setText("\n".join(self.session.notes))
        self.notice.setVisible(bool(self.session.notes))

    def mode(self) -> str:
        current = self.stack.currentWidget()
        return GUIDED if current is self.guided else PROFESSIONAL if current is self.professional else ""

    def set_mode(self, mode: str) -> None:
        self.stack.setCurrentWidget(self.guided if mode == GUIDED else self.professional)
        self.guided_action.setChecked(mode == GUIDED)
        self.professional_action.setChecked(mode == PROFESSIONAL)

    def _open_in_professional(self, key: str) -> None:
        self.set_mode(PROFESSIONAL)
        self.professional.open_section(key)

    # --- files --------------------------------------------------------------
    def _ask_save_dirty(self) -> bool:
        if not self.session.dirty:
            return True
        answer = QMessageBox.question(
            self, "Unsaved changes", "Save changes before continuing?",
            QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard |
            QMessageBox.StandardButton.Cancel)
        if answer == QMessageBox.StandardButton.Cancel:
            return False
        return self.save() if answer == QMessageBox.StandardButton.Save else True

    def new_project(self) -> None:
        if self._ask_save_dirty():
            self.session.new()

    def open_project(self) -> None:
        if not self._ask_save_dirty():
            return
        name, _ = QFileDialog.getOpenFileName(self, "Open Kymo project", "", "Kymo project (*.json)")
        if name:
            try:
                self.session.load(name)
            except ValueError as exc:
                QMessageBox.critical(self, "Open failed", str(exc))
                return
            if not self.mode():
                self.set_mode(PROFESSIONAL)

    def save(self) -> bool:
        if self.session.path is None:
            return self.save_as()
        try:
            self.session.save()
        except OSError as exc:
            QMessageBox.critical(self, "Save failed", str(exc))
            return False
        return True

    def save_as(self) -> bool:
        name, _ = QFileDialog.getSaveFileName(self, "Save Kymo project", "kymo.project.json",
                                              "Kymo project (*.json)")
        if not name:
            return False
        try:
            self.session.save(name)
        except OSError as exc:
            QMessageBox.critical(self, "Save failed", str(exc))
            return False
        return True

    def closeEvent(self, event) -> None:  # noqa: N802 - Qt override
        if self._ask_save_dirty():
            event.accept()
        else:
            event.ignore()
