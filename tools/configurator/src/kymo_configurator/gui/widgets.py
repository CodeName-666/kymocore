# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Custom section widgets and their registry.

A section names custom widgets in ``Section.widgets``; this registry maps the
names to factories ``factory(session, guided) -> QWidget`` with a ``refresh()``
method. Extensions add their own widgets with ``register_widget``.
"""
from __future__ import annotations

import copy
from typing import Callable

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QAbstractItemView, QComboBox, QHBoxLayout, QHeaderView, QLabel,
                               QListWidget, QListWidgetItem, QMessageBox, QPlainTextEdit,
                               QPushButton, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

from .. import catalog
from .session import ProjectSession

WidgetFactory = Callable[[ProjectSession, bool], QWidget]
WIDGETS: dict[str, WidgetFactory] = {}


def register_widget(name: str, factory: WidgetFactory) -> None:
    """Make a custom widget available to ``Section.widgets``."""
    WIDGETS[name] = factory


def create_widget(name: str, session: ProjectSession, guided: bool) -> QWidget:
    factory = WIDGETS.get(name)
    if factory is None:
        widget = QLabel(f"Missing widget '{name}'")
        widget.refresh = lambda: None  # type: ignore[attr-defined]
        return widget
    return factory(session, guided)


def _bool_item(checked: bool, enabled: bool, tooltip: str = "") -> QTableWidgetItem:
    item = QTableWidgetItem()
    flags = Qt.ItemFlag.ItemIsSelectable | Qt.ItemFlag.ItemIsUserCheckable
    if enabled:
        flags |= Qt.ItemFlag.ItemIsEnabled
    item.setFlags(flags)
    item.setCheckState(Qt.CheckState.Checked if checked else Qt.CheckState.Unchecked)
    item.setToolTip(tooltip)
    return item


def _readonly(text: str) -> QTableWidgetItem:
    item = QTableWidgetItem(text)
    item.setFlags(Qt.ItemFlag.ItemIsEnabled | Qt.ItemFlag.ItemIsSelectable)
    item.setForeground(Qt.GlobalColor.darkGray)
    return item


class ChannelEditor(QWidget):
    """Channel table with live rate and frame-size estimates."""

    HEADERS = ["ID", "Name", "Period (ms)", "Rate", "X", "Z", "Time", "Bytes/frame"]
    FLAGS = {4: "x", 5: "z", 6: "timestamp"}

    def __init__(self, session: ProjectSession, guided: bool = False):
        super().__init__()
        self.session = session
        self._updating = False
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.table = QTableWidget(0, len(self.HEADERS))
        self.table.setHorizontalHeaderLabels(self.HEADERS)
        self.table.setAlternatingRowColors(True)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.table.verticalHeader().setVisible(False)
        self.table.itemChanged.connect(self._item_changed)
        layout.addWidget(self.table)
        buttons = QHBoxLayout()
        self.add_button = QPushButton("Add channel")
        self.duplicate_button = QPushButton("Duplicate")
        self.remove_button = QPushButton("Remove")
        for button, slot in ((self.add_button, self.add_channel),
                             (self.duplicate_button, self.duplicate_channel),
                             (self.remove_button, self.remove_channel)):
            button.clicked.connect(slot)
            buttons.addWidget(button)
        buttons.addStretch(1)
        layout.addLayout(buttons)
        self.estimate = QLabel("")
        self.estimate.setObjectName("fieldHelp")
        self.estimate.setWordWrap(True)
        layout.addWidget(self.estimate)
        self.refresh()

    @property
    def rows(self) -> list:
        channels = self.session.doc.setdefault("channels", [])
        return channels if isinstance(channels, list) else []

    def refresh(self) -> None:
        view = self.session.view()
        self._updating = True
        try:
            self.table.setRowCount(len(self.rows))
            for row, channel in enumerate(self.rows):
                if not isinstance(channel, dict):
                    continue
                period = channel.get("period_ms", 0)
                size = 9 + 4 * sum(bool(channel.get(k)) for k in ("x", "z", "timestamp"))
                rate = f"{1000.0 / period:.1f} /s" if type(period) is int and period > 0 else "every cycle"
                values = [str(channel.get("id", "")), str(channel.get("name", "")), str(period)]
                for column, text in enumerate(values):
                    item = self.table.item(row, column)
                    if item is None or item.text() != text:
                        self.table.setItem(row, column, QTableWidgetItem(text))
                self.table.setItem(row, 3, _readonly(rate))
                for column, key in self.FLAGS.items():
                    allowed = view.feature(key)
                    tip = "" if allowed else f"Enable the {key} feature under Telemetry features first."
                    self.table.setItem(row, column, _bool_item(bool(channel.get(key)), allowed or bool(channel.get(key)), tip))
                self.table.setItem(row, 7, _readonly(str(size)))
        finally:
            self._updating = False
        enabled = view.feature("runtime")
        for widget in (self.table, self.add_button, self.duplicate_button, self.remove_button):
            widget.setEnabled(enabled)
        budget = self.session.doc.get("resources", {}).get("transport_bytes_per_second")
        summary = catalog.section("channels").summary(view) if catalog.section("channels").summary else ""
        text = f"Estimate: {summary}. Channels with period 0 depend on how often Kymo_Main runs."
        if isinstance(budget, int) and budget > 0:
            traffic = sum((1000.0 / c["period_ms"]) * (9 + 4 * sum(bool(c.get(k)) for k in ("x", "z", "timestamp")))
                          for c in self.rows if isinstance(c, dict) and type(c.get("period_ms")) is int and c["period_ms"] > 0)
            text += f" Link load: {traffic / budget * 100:.0f}% of {budget} bytes/s."
        self.estimate.setText(text)

    def _item_changed(self, item: QTableWidgetItem) -> None:
        if self._updating:
            return
        row = item.row()
        column = item.column()
        if row >= len(self.rows) or not isinstance(self.rows[row], dict):
            return
        channel = self.rows[row]
        if column in self.FLAGS:
            channel[self.FLAGS[column]] = item.checkState() == Qt.CheckState.Checked
        elif column == 1:
            channel["name"] = item.text()
        elif column in (0, 2):
            key = "id" if column == 0 else "period_ms"
            try:
                channel[key] = int(item.text())
            except ValueError:
                pass
        else:
            return
        self.session.touch()

    def _free_id(self):
        used = {c.get("id") for c in self.rows if isinstance(c, dict)}
        return next((i for i in range(256) if i not in used), None)

    def add_channel(self) -> None:
        cid = self._free_id()
        if cid is not None:
            self.rows.append({"id": cid, "name": f"Channel {cid}", "period_ms": 20,
                              "x": False, "z": False, "timestamp": False})
            self.session.touch()

    def duplicate_channel(self) -> None:
        row = self.table.currentRow()
        cid = self._free_id()
        if 0 <= row < len(self.rows) and cid is not None:
            data = copy.deepcopy(self.rows[row])
            data["id"] = cid
            self.rows.append(data)
            self.session.touch()

    def remove_channel(self) -> None:
        row = self.table.currentRow()
        if 0 <= row < len(self.rows):
            del self.rows[row]
            self.session.touch()


class RegistryEditor(QWidget):
    """Trace symbol or context dictionary."""

    def __init__(self, session: ProjectSession, group: str, guided: bool = False):
        super().__init__()
        self.session = session
        self.group = group
        self._updating = False
        self.kinds = ("function", "scope", "marker") if group == "symbols" else ("main", "task", "isr")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 8, 0, 0)
        title = QLabel("Symbol dictionary" if group == "symbols" else "Context dictionary")
        title.setObjectName("groupTitle")
        layout.addWidget(title)
        hint = QLabel("Functions, scopes and markers you instrument with KYMO_TRACE_*; the ID is "
                      "the symbol argument." if group == "symbols" else
                      "Tasks and interrupts that record events; the ID is the context argument.")
        hint.setObjectName("fieldHelp")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["ID", "Name", "Kind"])
        self.table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        self.table.verticalHeader().setVisible(False)
        self.table.setMaximumHeight(170)
        self.table.itemChanged.connect(self._item_changed)
        layout.addWidget(self.table)
        buttons = QHBoxLayout()
        self.add_button = QPushButton("Add symbol" if group == "symbols" else "Add context")
        self.remove_button = QPushButton("Remove")
        self.add_button.clicked.connect(self.add_row)
        self.remove_button.clicked.connect(self.remove_row)
        buttons.addWidget(self.add_button)
        buttons.addWidget(self.remove_button)
        buttons.addStretch(1)
        layout.addLayout(buttons)
        self.refresh()

    @property
    def rows(self) -> list:
        rows = self.session.doc.setdefault(self.group, [])
        return rows if isinstance(rows, list) else []

    def refresh(self) -> None:
        self._updating = True
        try:
            self.table.setRowCount(len(self.rows))
            for row, entry in enumerate(self.rows):
                if not isinstance(entry, dict):
                    continue
                self.table.setItem(row, 0, QTableWidgetItem(str(entry.get("id", ""))))
                self.table.setItem(row, 1, QTableWidgetItem(str(entry.get("name", ""))))
                combo = QComboBox()
                combo.addItems(self.kinds)
                combo.setCurrentText(str(entry.get("kind", self.kinds[0])))
                combo.currentTextChanged.connect(lambda text, r=row: self._kind_changed(r, text))
                self.table.setCellWidget(row, 2, combo)
        finally:
            self._updating = False
        visible = bool(self.session.view().get((catalog.PROFILE, "trace", "enabled"), False))
        self.setVisible(visible)

    def _item_changed(self, item: QTableWidgetItem) -> None:
        if self._updating or item.row() >= len(self.rows):
            return
        entry = self.rows[item.row()]
        if item.column() == 0:
            try:
                entry["id"] = int(item.text())
            except ValueError:
                return
        elif item.column() == 1:
            entry["name"] = item.text().strip()
        self.session.touch()

    def _kind_changed(self, row: int, text: str) -> None:
        if not self._updating and row < len(self.rows):
            self.rows[row]["kind"] = text
            self.session.touch()

    def add_row(self) -> None:
        used = {entry.get("id") for entry in self.rows if isinstance(entry, dict)}
        limit = 65536 if self.group == "symbols" else 4294967295
        ident = next((i for i in range(1, limit) if i not in used), None)
        if ident is not None:
            self.rows.append({"id": ident, "name": f"{self.group[:-1]}_{ident}", "kind": self.kinds[0]})
            self.session.touch()

    def remove_row(self) -> None:
        row = self.table.currentRow()
        if 0 <= row < len(self.rows):
            del self.rows[row]
            self.session.touch()


class ReviewPanel(QWidget):
    """Summary, effective macros, diagnostics, preview and generation."""

    navigate = Signal(str)
    """Emitted with a section key when the user opens a diagnostic."""

    def __init__(self, session: ProjectSession, guided: bool = False):
        super().__init__()
        self.session = session
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.summary = QLabel("")
        self.summary.setObjectName("reviewSummary")
        self.summary.setWordWrap(True)
        self.summary.setTextFormat(Qt.TextFormat.RichText)
        layout.addWidget(self.summary)
        problems_title = QLabel("Open problems")
        problems_title.setObjectName("groupTitle")
        layout.addWidget(problems_title)
        self.problems = QListWidget()
        self.problems.setMaximumHeight(150)
        self.problems.itemActivated.connect(self._open_problem)
        self.problems.itemDoubleClicked.connect(self._open_problem)
        layout.addWidget(self.problems)
        buttons = QHBoxLayout()
        self.preview_button = QPushButton("Preview generated files")
        self.generate_button = QPushButton("Generate configuration")
        self.generate_button.setObjectName("primaryButton")
        self.preview_button.clicked.connect(self.show_preview)
        self.generate_button.clicked.connect(self.generate)
        buttons.addWidget(self.preview_button)
        buttons.addStretch(1)
        buttons.addWidget(self.generate_button)
        layout.addLayout(buttons)
        self.output = QPlainTextEdit()
        self.output.setReadOnly(True)
        self.output.setObjectName("preview")
        self.output.setPlaceholderText("The generated files and their changes appear here.")
        layout.addWidget(self.output, 1)
        self.refresh()

    def refresh(self) -> None:
        view = self.session.view()
        rows = []
        for section in catalog.sections():
            if section.summary and section.is_visible(view):
                rows.append(f"<tr><td><b>{section.title}</b></td><td>{section.summary(view)}</td></tr>")
        macros = ", ".join(f"{name}={value}" for name, value in self.session.effective_macros())
        self.summary.setText("<table cellspacing='6'>" + "".join(rows) + "</table>"
                             f"<p><span style='color:#5b6472'>Generated settings:</span> <code>{macros}</code></p>")
        self.problems.clear()
        for diagnostic in self.session.diagnostics():
            owner = catalog.section_for_path(diagnostic.path, self.session.profile)
            prefix = "✖" if diagnostic.severity == "error" else "⚠"
            where = owner.title if owner else diagnostic.path
            item = QListWidgetItem(f"{prefix} {where}: {diagnostic.message}" +
                                   (f" — {diagnostic.suggestion}" if diagnostic.suggestion else ""))
            item.setData(Qt.ItemDataRole.UserRole, owner.key if owner else "")
            self.problems.addItem(item)
        if not self.session.diagnostics():
            self.problems.addItem("✓ No problems found. The configuration can be generated.")
        self.generate_button.setEnabled(not self.session.errors())

    def _open_problem(self, item: QListWidgetItem) -> None:
        key = item.data(Qt.ItemDataRole.UserRole)
        if key:
            self.navigate.emit(key)

    def show_preview(self) -> None:
        if self.session.errors():
            self.output.setPlainText("Resolve the open problems first.")
            return
        lines = []
        for name, (state, content) in self.session.preview().items():
            lines.append(f"--- {name} ({state}) ---")
            lines.extend(content[:80])
            lines.append("")
        self.output.setPlainText("\n".join(lines))

    def generate(self) -> None:
        try:
            snapshot = self.session.generate()
        except (ValueError, OSError) as exc:
            QMessageBox.critical(self, "Generation failed", str(exc))
            return
        self.output.setPlainText(
            f"Generated snapshot:\n{snapshot}\n\nActive selector: "
            f"{self.session.output_path() / 'active_generation.json'}\n\n"
            "Add the snapshot folder to your include path and define\n"
            "KYMO_USER_CONFIG_HEADER=\"kymo_user_config.h\" for all files, "
            "or use the generated build helper for your build system.")


register_widget("channels", lambda session, guided: ChannelEditor(session, guided))
register_widget("symbols", lambda session, guided: RegistryEditor(session, "symbols", guided))
register_widget("contexts", lambda session, guided: RegistryEditor(session, "contexts", guided))
register_widget("review", lambda session, guided: ReviewPanel(session, guided))
