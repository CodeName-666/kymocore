# SPDX-License-Identifier: MIT
"""Guided mode: one question per step, advanced settings keep defaults.

The steps are the catalog sections that apply to the current goal; for
example the tracing step only appears when tracing is part of the goal. A step
with open errors cannot be left forward, so the final review is reachable only
with a consistent configuration.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QHBoxLayout, QLabel, QListWidget, QListWidgetItem, QPushButton,
                               QStackedWidget, QVBoxLayout, QWidget)

from .. import catalog
from ..i18n import tr
from .section_page import SectionPage
from .session import ProjectSession


class GuidedWizard(QWidget):
    """Step list on the left, the current question on the right."""

    switch_to_professional = Signal(str)

    def __init__(self, session: ProjectSession):
        super().__init__()
        self.session = session
        self.index = 0
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        side = QWidget()
        side.setObjectName("sidebar")
        side.setFixedWidth(260)
        side_layout = QVBoxLayout(side)
        heading = QLabel(tr("Guided setup"))
        heading.setObjectName("sidebarTitle")
        side_layout.addWidget(heading)
        self.steps = QListWidget()
        self.steps.setObjectName("navigation")
        self.steps.itemClicked.connect(self._step_clicked)
        side_layout.addWidget(self.steps, 1)
        hint = QLabel(tr("You can switch to the professional mode at any time; nothing is lost."))
        hint.setObjectName("fieldHelp")
        hint.setWordWrap(True)
        side_layout.addWidget(hint)
        layout.addWidget(side)
        right = QVBoxLayout()
        self.progress = QLabel("")
        self.progress.setObjectName("progress")
        right.addWidget(self.progress)
        self.stack = QStackedWidget()
        right.addWidget(self.stack, 1)
        self.message = QLabel("")
        self.message.setObjectName("fieldProblem")
        self.message.setWordWrap(True)
        right.addWidget(self.message)
        buttons = QHBoxLayout()
        self.professional_button = QPushButton(tr("Open in professional mode"))
        self.professional_button.clicked.connect(
            lambda: self.switch_to_professional.emit(self.current_section().key))
        self.back_button = QPushButton(tr("← Back"))
        self.next_button = QPushButton(tr("Next →"))
        self.next_button.setObjectName("primaryButton")
        self.back_button.clicked.connect(self.back)
        self.next_button.clicked.connect(self.next)
        buttons.addWidget(self.professional_button)
        buttons.addStretch(1)
        buttons.addWidget(self.back_button)
        buttons.addWidget(self.next_button)
        right.addLayout(buttons)
        holder = QWidget()
        holder.setLayout(right)
        layout.addWidget(holder, 1)
        self.pages: dict[str, SectionPage] = {}
        session.replaced.connect(self.rebuild)
        session.changed.connect(self.refresh)
        self.rebuild()

    # --- steps --------------------------------------------------------------
    def step_sections(self) -> list[catalog.Section]:
        view = self.session.view()
        return [s for s in catalog.sections() if s.in_guided(view)]

    def current_section(self) -> catalog.Section:
        sections = self.step_sections()
        self.index = max(0, min(self.index, len(sections) - 1))
        return sections[self.index]

    def rebuild(self) -> None:
        while self.stack.count():
            page = self.stack.widget(0)
            self.stack.removeWidget(page)
            page.deleteLater()
        self.pages = {}
        for section in catalog.sections():
            page = SectionPage(section, self.session, guided=True)
            page.navigate.connect(self.open_section)
            self.pages[section.key] = page
            self.stack.addWidget(page)
        self.index = 0
        self.refresh()

    def refresh(self) -> None:
        sections = self.step_sections()
        current = self.current_section()
        self.stack.setCurrentWidget(self.pages[current.key])
        self.steps.blockSignals(True)
        self.steps.clear()
        for number, section in enumerate(sections):
            if number < self.index:
                marker = "✓"
            elif number == self.index:
                marker = "▶"
            else:
                marker = "○"
            item = QListWidgetItem(f"{marker}  {number + 1}. {tr(section.title)}")
            item.setData(Qt.ItemDataRole.UserRole, number)
            if number == self.index:
                font = item.font()
                font.setBold(True)
                item.setFont(font)
            self.steps.addItem(item)
        self.steps.blockSignals(False)
        self.progress.setText(tr("Step {n} of {total}", n=self.index + 1, total=len(sections)))
        self.back_button.setEnabled(self.index > 0)
        last = self.index == len(sections) - 1
        self.next_button.setVisible(not last)
        self.message.setText("")

    def blocking_errors(self) -> list:
        return [d for d in self.session.diagnostics_for(self.current_section()) if d.severity == "error"]

    def next(self) -> bool:
        errors = self.blocking_errors()
        if errors:
            self.message.setText(tr("Please fix this first: ") + "; ".join(d.localized()[0] for d in errors[:3]))
            return False
        self.index += 1
        self.refresh()
        return True

    def back(self) -> None:
        self.index = max(0, self.index - 1)
        self.refresh()

    def _step_clicked(self, item: QListWidgetItem) -> None:
        target = item.data(Qt.ItemDataRole.UserRole)
        if target <= self.index:
            self.index = target
            self.refresh()
        else:
            while self.index < target and self.next():
                pass

    def open_section(self, key: str) -> None:
        keys = [s.key for s in self.step_sections()]
        if key in keys:
            self.index = keys.index(key)
            self.refresh()
        else:
            self.switch_to_professional.emit(key)
