# SPDX-License-Identifier: MIT
"""Main window: start page, guided mode and professional mode on one session."""
from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QLocale, QSettings, Qt, QTimer
from PySide6.QtGui import QAction, QActionGroup, QKeySequence
from PySide6.QtWidgets import (QComboBox, QFileDialog, QFrame, QHBoxLayout, QInputDialog, QLabel,
                               QMainWindow, QMenu, QMessageBox, QPushButton, QStackedWidget,
                               QToolBar, QToolButton, QVBoxLayout, QWidget)

from ..i18n import LANGUAGES, language, set_language, tr
from ..model import CORE_CONFIG_API_VERSION
from .guided import GuidedWizard
from .professional import ProfessionalEditor
from .session import ProjectSession
from .style import STYLE

GUIDED = "guided"
PROFESSIONAL = "professional"


def settings() -> QSettings:
    return QSettings("KymoCore", "Configurator")


def initial_language(requested: str | None = None) -> str:
    """Command line choice, else the saved choice, else German for a German system."""
    saved = settings().value("language", "")
    system = "de" if QLocale.system().name().startswith("de") else "en"
    return next(code for code in (requested, saved, system) if code in LANGUAGES)


class StartPage(QWidget):
    """Choose how to work; shown when no mode is given on the command line."""

    def __init__(self, window: "MainWindow"):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(48, 36, 48, 36)
        title = QLabel(tr("KymoCore Configurator"))
        title.setObjectName("startTitle")
        layout.addWidget(title)
        intro = QLabel(tr("Creates the build configuration for KymoCore 7 (configuration API "
                          "{version}): which features are compiled in, which channels are "
                          "sent and which of your functions connect KymoCore to the hardware.",
                          version=CORE_CONFIG_API_VERSION))
        intro.setWordWrap(True)
        intro.setObjectName("sectionIntro")
        layout.addWidget(intro)
        cards = QHBoxLayout()
        for mode, heading, text, button_text in (
            (GUIDED, tr("Guided setup"),
             tr("Step by step: KymoCore asks what you want to achieve and only shows what matters for it. "
                "Advanced options keep safe defaults. Recommended for the first project."),
             tr("Start guided setup")),
            (PROFESSIONAL, tr("Professional mode"),
             tr("All settings at once, grouped by topic, with live status, diagnostics and a preview "
                "of the generated files. For users who know KymoCore."),
             tr("Open professional mode")),
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
        open_button = QPushButton(tr("Open existing project…"))
        open_button.clicked.connect(window.open_project)
        files.addWidget(open_button)
        files.addStretch(1)
        layout.addLayout(files)
        layout.addStretch(1)


class MainWindow(QMainWindow):
    def __init__(self, path: Path | None = None, mode: str | None = None):
        super().__init__()
        self.resize(1180, 800)
        self.setStyleSheet(STYLE)
        self.session = ProjectSession()
        if path is not None and Path(path).exists():
            self.session.load(path)
        elif path is not None:
            self.session.path = Path(path)
        self.bar: QToolBar | None = None
        self._build_ui(mode)
        self.session.changed.connect(self._update_chrome)
        self.session.replaced.connect(self._load_profiles)

    def _build_ui(self, mode: str | None) -> None:
        """(Re)create menus, toolbar and pages in the current language; the session is kept."""
        if self.bar is not None:
            self.removeToolBar(self.bar)
            self.bar.deleteLater()
        self._build_menus()
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
        self._load_profiles()
        self.set_mode(mode) if mode in (GUIDED, PROFESSIONAL) else self.stack.setCurrentWidget(self.start_page)
        self._update_chrome()

    # --- chrome -------------------------------------------------------------
    def _build_menus(self) -> None:
        """File, View and Language menus; the language menu is labelled in both languages."""
        menus = self.menuBar()
        for old in menus.findChildren(QMenu, options=Qt.FindChildOption.FindDirectChildrenOnly):
            old.deleteLater()
        menus.clear()
        file_menu = menus.addMenu(tr("&File"))
        for text, shortcut, slot in ((tr("&New"), QKeySequence.StandardKey.New, self.new_project),
                                     (tr("&Open…"), QKeySequence.StandardKey.Open, self.open_project),
                                     (tr("&Save"), QKeySequence.StandardKey.Save, self.save),
                                     (tr("Save &as…"), QKeySequence.StandardKey.SaveAs, self.save_as)):
            action = file_menu.addAction(text)
            action.setShortcut(shortcut)
            action.triggered.connect(slot)
        file_menu.addSeparator()
        quit_action = file_menu.addAction(tr("&Quit"))
        quit_action.setShortcut(QKeySequence.StandardKey.Quit)
        quit_action.triggered.connect(self.close)
        view_menu = menus.addMenu(tr("&View"))
        modes = QActionGroup(view_menu)
        self.guided_action = view_menu.addAction(tr("Guided setup"))
        self.professional_action = view_menu.addAction(tr("Professional mode"))
        for action, target in ((self.guided_action, GUIDED), (self.professional_action, PROFESSIONAL)):
            action.setCheckable(True)
            modes.addAction(action)
            action.triggered.connect(lambda _=False, m=target: self.set_mode(m))
        language_menu = menus.addMenu("Language / Sprache")
        languages = QActionGroup(language_menu)
        for code, name in LANGUAGES.items():
            action = language_menu.addAction(name)
            action.setCheckable(True)
            action.setChecked(code == language())
            languages.addAction(action)
            # Deferred: rebuilding the menus inside their own signal would delete the sender.
            action.triggered.connect(lambda _=False, c=code: QTimer.singleShot(0, lambda: self.change_language(c)))

    def _build_toolbar(self) -> None:
        bar = QToolBar(tr("Project"))
        bar.setMovable(False)
        bar.setObjectName("mainToolbar")
        self.addToolBar(bar)
        self.bar = bar
        profile_tip = tr("A profile is a named set of settings for one firmware build, e.g. 'development' "
                         "with all measurements and 'production' with KymoCore switched off. "
                         "All steps edit the profile selected here.")
        profile_label = QLabel(" " + tr("Build profile:") + " ")
        profile_label.setToolTip(profile_tip)
        bar.addWidget(profile_label)
        self.profile = QComboBox()
        self.profile.setToolTip(profile_tip)
        self.profile.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToContents)
        self.profile.setMinimumContentsLength(12)
        self.profile.currentTextChanged.connect(self.session.set_profile)
        bar.addWidget(self.profile)
        duplicate = QAction(tr("New profile…"), bar)
        duplicate.setToolTip(tr("Adds a profile that starts as a copy of the selected one."))
        duplicate.triggered.connect(self.new_profile)
        duplicate_button = QToolButton()
        duplicate_button.setDefaultAction(duplicate)
        duplicate_button.setObjectName("toolbarButton")
        bar.addWidget(duplicate_button)
        spacer = QWidget()
        spacer.setSizePolicy(spacer.sizePolicy().horizontalPolicy().Expanding,
                             spacer.sizePolicy().verticalPolicy().Preferred)
        bar.addWidget(spacer)
        # Segmented mode switch; the buttons mirror the View menu actions.
        switch = QWidget()
        switch_layout = QHBoxLayout(switch)
        switch_layout.setContentsMargins(0, 0, 0, 0)
        switch_layout.setSpacing(0)
        for action, tip, name in (
                (self.guided_action, tr("Step by step, only what matters for your goal."), "segmentLeft"),
                (self.professional_action, tr("All settings at once."), "segmentRight")):
            button = QToolButton()
            button.setDefaultAction(action)
            button.setToolTip(tip)
            button.setObjectName(name)
            switch_layout.addWidget(button)
        bar.addWidget(switch)

    def change_language(self, code: str) -> None:
        if code == language():
            return
        set_language(code)
        settings().setValue("language", code)
        guided_step = self.guided.index
        mode = self.mode()
        self._build_ui(mode)
        self.guided.index = guided_step
        self.guided.refresh()

    def _load_profiles(self) -> None:
        self.profile.blockSignals(True)
        self.profile.clear()
        self.profile.addItems(self.session.profiles)
        self.profile.setCurrentText(self.session.profile)
        self.profile.blockSignals(False)

    def _update_chrome(self) -> None:
        name = self.session.path.name if self.session.path else tr("unsaved project")
        self.setWindowTitle(f"{tr('KymoCore Configurator')} — {name}{' *' if self.session.dirty else ''}")
        errors = len(self.session.errors())
        warnings = len(self.session.diagnostics()) - errors
        state = tr("ready to generate") if not errors else tr("{count} problem(s) to fix", count=errors)
        if warnings:
            state += ", " + tr("{count} warning(s)", count=warnings)
        self.statusBar().showMessage(tr("Profile '{name}': {state}", name=self.session.profile, state=state))
        self.notice.setText("\n".join(self.session.notes))
        self.notice.setVisible(bool(self.session.notes))

    def mode(self) -> str:
        current = self.stack.currentWidget()
        return GUIDED if current is self.guided else PROFESSIONAL if current is self.professional else ""

    def set_mode(self, mode: str) -> None:
        self.stack.setCurrentWidget(self.guided if mode == GUIDED else self.professional)
        self.guided_action.setChecked(mode == GUIDED)
        self.professional_action.setChecked(mode == PROFESSIONAL)

    def new_profile(self) -> None:
        name, ok = QInputDialog.getText(
            self, tr("New profile"),
            tr("The new profile starts as a copy of '{name}'.\n"
               "Afterwards you can change it without affecting the original.\n\n"
               "Name (e.g. production):", name=self.session.profile))
        name = name.strip()
        if not ok or not name:
            return
        try:
            self.session.duplicate_profile(name)
        except ValueError:
            QMessageBox.warning(self, tr("New profile"),
                                tr("A profile named '{name}' already exists.", name=name))

    def _open_in_professional(self, key: str) -> None:
        self.set_mode(PROFESSIONAL)
        self.professional.open_section(key)

    # --- files --------------------------------------------------------------
    def _ask_save_dirty(self) -> bool:
        if not self.session.dirty:
            return True
        answer = QMessageBox.question(
            self, tr("Unsaved changes"), tr("Save changes before continuing?"),
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
        name, _ = QFileDialog.getOpenFileName(self, tr("Open Kymo project"), "", tr("Kymo project (*.json)"))
        if name:
            try:
                self.session.load(name)
            except ValueError as exc:
                QMessageBox.critical(self, tr("Open failed"), str(exc))
                return
            if not self.mode():
                self.set_mode(PROFESSIONAL)

    def save(self) -> bool:
        if self.session.path is None:
            return self.save_as()
        try:
            self.session.save()
        except OSError as exc:
            QMessageBox.critical(self, tr("Save failed"), str(exc))
            return False
        return True

    def save_as(self) -> bool:
        name, _ = QFileDialog.getSaveFileName(self, tr("Save Kymo project"), "kymo.project.json",
                                              tr("Kymo project (*.json)"))
        if not name:
            return False
        try:
            self.session.save(name)
        except OSError as exc:
            QMessageBox.critical(self, tr("Save failed"), str(exc))
            return False
        return True

    def closeEvent(self, event) -> None:  # noqa: N802 - Qt override
        if self._ask_save_dirty():
            event.accept()
        else:
            event.ignore()
