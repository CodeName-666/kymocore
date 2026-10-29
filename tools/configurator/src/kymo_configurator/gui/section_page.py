# SPDX-License-Identifier: MIT
"""Render one catalog section; used by both modes."""
from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import QFrame, QLabel, QScrollArea, QVBoxLayout, QWidget

from .. import catalog
from ..i18n import tr
from .fields import FieldEditor
from .session import ProjectSession
from .widgets import create_widget


class SectionPage(QScrollArea):
    """Header, explained field groups and custom widgets of a section."""

    navigate = Signal(str)

    def __init__(self, section: catalog.Section, session: ProjectSession, guided: bool = False):
        super().__init__()
        self.section = section
        self.session = session
        self.guided = guided
        self.setWidgetResizable(True)
        self.setFrameShape(QFrame.Shape.NoFrame)
        body = QWidget()
        body.setObjectName("sectionBody")
        layout = QVBoxLayout(body)
        layout.setContentsMargins(24, 18, 24, 18)
        title = QLabel(tr(section.guided_title if guided and section.guided_title else section.title))
        title.setObjectName("sectionTitle")
        title.setWordWrap(True)
        layout.addWidget(title)
        intro = QLabel(tr(section.guided_intro if guided and section.guided_intro else section.description))
        intro.setObjectName("sectionIntro")
        intro.setWordWrap(True)
        layout.addWidget(intro)
        self.editors: dict[str, FieldEditor] = {}
        self.groups: list[tuple[catalog.Group, QFrame]] = []
        for group in section.groups:
            if guided and group.advanced:
                continue
            fields = [f for f in group.fields if not (guided and f.advanced)]
            if not fields:
                continue
            frame = QFrame()
            frame.setObjectName("groupFrame")
            group_layout = QVBoxLayout(frame)
            heading = QLabel(tr(group.title))
            heading.setObjectName("groupTitle")
            group_layout.addWidget(heading)
            description = group.guided_description if guided and group.guided_description else group.description
            if description:
                note = QLabel(tr(description))
                note.setObjectName("fieldHelp")
                note.setWordWrap(True)
                group_layout.addWidget(note)
            for field in fields:
                editor = FieldEditor(field, session, guided, frame)
                self.editors[field.key] = editor
                group_layout.addWidget(editor)
            layout.addWidget(frame)
            self.groups.append((group, frame))
        self.widgets: list[QWidget] = []
        for name in section.widgets:
            widget = create_widget(name, session, guided)
            if hasattr(widget, "navigate"):
                widget.navigate.connect(self.navigate)
            self.widgets.append(widget)
            layout.addWidget(widget, 1 if name == "review" else 0)
        if guided:
            hidden = sum(1 for g in section.groups for f in g.fields if f.advanced or g.advanced)
            if hidden:
                more = QLabel(tr("{count} rarely needed setting(s) are hidden here and keep safe "
                                 "defaults. The professional mode shows them.", count=hidden))
                more.setObjectName("fieldReason")
                more.setWordWrap(True)
                layout.addWidget(more)
        if "review" not in section.widgets:
            layout.addStretch(1)
        self.setWidget(body)
        session.changed.connect(self.refresh)

    def refresh(self) -> None:
        view = self.session.view()
        for editor in self.editors.values():
            editor.refresh()
        for group, frame in self.groups:
            frame.setVisible(group.visible_when is None or bool(group.visible_when(view)))
        for widget in self.widgets:
            refresh = getattr(widget, "refresh", None)
            if refresh:
                refresh()
