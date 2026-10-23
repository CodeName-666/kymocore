# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Generic editors that render catalog fields.

Every editor shows the label, the plain-language explanation, the C macro or
binding it affects (a tooltip in guided mode) and, when disabled, why. New field kinds are added in
``_make_input`` only; sections never build widgets by hand.
"""
from __future__ import annotations

from typing import Any

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFrame, QHBoxLayout, QLabel, QLineEdit,
                               QSpinBox, QVBoxLayout, QWidget)

from .. import catalog
from ..i18n import tr
from ..presets import PRESETS, apply_goals, goals
from .session import ProjectSession


class FieldEditor(QWidget):
    """One labelled, explained input bound to a catalog field."""

    def __init__(self, field: catalog.Field, session: ProjectSession, guided: bool = False,
                 parent: QWidget | None = None):
        # Created inside its parent: a parentless widget shown by refresh() would flash as a window.
        super().__init__(parent)
        self.field = field
        self.session = session
        self.guided = guided
        self._updating = False
        self.setObjectName("fieldEditor")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 8)
        layout.setSpacing(2)
        self.title = QLabel(tr(field.label))
        self.title.setObjectName("fieldTitle")
        self.title.setWordWrap(True)
        self.input = self._make_input()
        header = QHBoxLayout()
        if field.kind == "bool":
            header.addWidget(self.input)
        header.addWidget(self.title, 1)
        self.need = QLabel("")
        header.addWidget(self.need, 0, Qt.AlignmentFlag.AlignRight)
        if field.target and guided:
            self.title.setToolTip(tr("Technical detail: {target}", target=field.target))
        elif field.target:
            badge = QLabel(field.target)
            badge.setObjectName("targetBadge")
            badge.setToolTip(tr("Affected C macro, configuration field or function signature"))
            badge.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
            header.addWidget(badge, 0, Qt.AlignmentFlag.AlignRight)
        layout.addLayout(header)
        if field.kind != "bool":
            layout.addWidget(self.input)
        self.help = QLabel(tr(field.guided_help if guided and field.guided_help else field.help))
        self.help.setObjectName("fieldHelp")
        self.help.setWordWrap(True)
        layout.addWidget(self.help)
        self.example = QLabel("")
        self.example.setObjectName("codeExample")
        self.example.setTextFormat(Qt.TextFormat.PlainText)
        self.example.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        self.example.setToolTip(tr("Example of the function in your own code; the name follows your entry."))
        layout.addWidget(self.example)
        self.example.setVisible(bool(field.example))
        self.reason = QLabel("")
        self.reason.setObjectName("fieldReason")
        self.reason.setWordWrap(True)
        layout.addWidget(self.reason)
        self.problem = QLabel("")
        self.problem.setObjectName("fieldProblem")
        self.problem.setWordWrap(True)
        layout.addWidget(self.problem)
        self.refresh()

    # --- construction -------------------------------------------------------
    def _make_input(self) -> QWidget:
        kind = self.field.kind
        if kind == "bool":
            widget = QCheckBox()
            widget.toggled.connect(self._commit)
        elif kind == "int":
            widget = QSpinBox()
            widget.setRange(self.field.minimum if self.field.minimum is not None else -2147483647,
                            self.field.maximum if self.field.maximum is not None else 2147483647)
            widget.valueChanged.connect(self._commit)
        elif kind == "choice":
            widget = QComboBox()
            for value, label in self.field.choices:
                widget.addItem(tr(label), value)
            widget.currentIndexChanged.connect(self._commit)
        elif kind == "preset" and self.guided:
            widget = self._make_cards()
        elif kind == "preset":
            widget = QComboBox()
            for value, label in self.field.choices:
                widget.addItem(tr(label), value)
            widget.currentIndexChanged.connect(self._commit)
        else:
            widget = QLineEdit()
            widget.setPlaceholderText(self.field.placeholder or
                                      (tr("optional") if kind == "optional_int" else ""))
            widget.editingFinished.connect(self._commit)
            widget.textEdited.connect(self._commit_text)
        return widget

    def _make_cards(self) -> QWidget:
        """Guided goals as checkboxes: live values always, XYZ and tracing on top."""
        frame = QWidget()
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(0, 0, 0, 0)
        self._goals: list[QCheckBox] = []
        cards = (
            (PRESETS["minimal"], tr("Always included.") + " " + tr(PRESETS["minimal"].question) + ".",
             tr("Each sample carries one number, the Y value. KymoStudio draws it over time.\n"
                "Example: temperature, motor current, battery voltage.\n"
                "Available in every project, also together with the options below.")),
            (PRESETS["xyz"], tr(PRESETS["xyz"].question) + ". " +
             tr("Curves over time keep working; each channel decides in step 3 which kind it sends."),
             tr("A sample may carry its own X (and Z) next to the Y value, plus the time it was "
                "measured.\nKymoStudio then draws Y over X, or a point in 3D, instead of Y over time.\n"
                "Example: pen position of a plotter, robot arm position, torque over angle.\n"
                "Compiles in X, Z and timestamps (KYMO_ENABLE_X/_Z/_TIMESTAMP).")),
            (PRESETS["trace"], tr(PRESETS["trace"].question) + ". " + tr("Works with or without X/Y points."),
             tr("Records when your functions, interrupts and data flows run and shows them on a "
                "timeline in KymoStudio.\nExample: find out why a control loop is sometimes late.\n"
                "Compiles in the trace recorder (KYMO_ENABLE_TRACE).")),
        )
        for index, (preset, text, tip) in enumerate(cards):
            card = QFrame()
            card.setObjectName("presetCard")
            card.setToolTip(tip)
            inner = QVBoxLayout(card)
            button = QCheckBox(tr(preset.title))
            button.setObjectName("presetTitle")
            label = QLabel(text)
            label.setWordWrap(True)
            label.setObjectName("fieldHelp")
            inner.addWidget(button)
            inner.addWidget(label)
            layout.addWidget(card)
            if index == 0:
                button.setChecked(True)
                button.setEnabled(False)
            else:
                button.toggled.connect(self._commit_goals)
                self._goals.append(button)
        return frame

    def _commit_goals(self, *_: Any) -> None:
        if self._updating:
            return
        profile = self.session.view().profile
        xyz, trace = (box.isChecked() for box in self._goals)
        if (xyz, trace) != goals(profile):
            apply_goals(profile, xyz, trace)
            self.session.touch()

    # --- data flow ----------------------------------------------------------
    def current_input(self) -> Any:
        kind = self.field.kind
        widget = self.input
        if kind == "bool":
            return widget.isChecked()
        if kind == "int":
            return widget.value()
        if kind == "preset" and self.guided:
            return self.session.value(self.field)
        if kind in ("choice", "preset"):
            return widget.currentData()
        text = widget.text().strip()
        if kind == "optional_int":
            return int(text) if text.isdigit() else (None if not text else text)
        return text

    def set_input(self, value: Any) -> None:
        kind = self.field.kind
        widget = self.input
        if kind == "bool":
            widget.setChecked(bool(value))
        elif kind == "int":
            widget.setValue(int(value) if isinstance(value, int) else int(self.field.default or 0))
        elif kind == "preset" and self.guided:
            for box, selected in zip(self._goals, goals(self.session.view().profile)):
                box.setChecked(selected)
        elif kind in ("choice", "preset"):
            index = widget.findData(value)
            widget.setCurrentIndex(index if index >= 0 else 0)
        else:
            text = "" if value is None else str(value)
            if widget.text() != text:
                widget.setText(text)

    def _commit_text(self, *_: Any) -> None:
        """Store text immediately so dependent editors update while typing."""
        self._commit()

    def _commit(self, *_: Any) -> None:
        if self._updating:
            return
        value = self.current_input()
        if value != self.session.value(self.field):
            self.session.set_value(self.field, value)

    def refresh(self) -> None:
        """Pull value, enabled state and diagnostics from the session."""
        view = self.session.view()
        self._updating = True
        try:
            if not (self.field.kind in ("identifier", "text", "optional_int") and self.input.hasFocus()):
                self.set_input(self.field.value(view))
        finally:
            self._updating = False
        enabled = self.field.is_enabled(view)
        self.input.setEnabled(enabled)
        self.title.setEnabled(enabled)
        self.reason.setText("" if enabled else tr(self.field.requires))
        self.reason.setVisible(not enabled and bool(self.field.requires))
        required = bool(self.field.need(view)) if self.field.need is not None else None
        self.need.setText(tr("Required") if required else tr("Optional"))
        self.need.setObjectName("needRequired" if required else "needOptional")
        self.need.setStyleSheet("")  # re-apply the window style for the new object name
        self.need.setVisible(enabled and required is not None)
        if self.field.example:
            name = str(self.field.value(view) or self.field.placeholder or "my_function")
            self.example.setText(tr("In your code, for example:") + "\n" +
                                 self.field.example.replace("{name}", name))
            self.example.setVisible(enabled)
        self.setVisible(self.field.is_visible(view))
        path = self.session.concrete_path(self.field)
        problems = [d for d in self.session.diagnostics() if d.path == path]
        lines = []
        for d in problems:
            message, suggestion = d.localized()
            lines.append(("⚠ " if d.severity != "error" else "✖ ") + message +
                         (f" — {suggestion}" if suggestion else ""))
        text = "\n".join(lines)
        self.problem.setText(text)
        self.problem.setVisible(bool(text))
