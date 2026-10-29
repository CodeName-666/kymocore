# SPDX-License-Identifier: MIT
"""User interface language.

English text is the key: ``tr("Step {n} of {total}", n=1, total=6)`` returns the
German translation when German is active and falls back to English for
missing entries. Diagnostics keep English text for the CLI and are translated for display
through ``Diagnostic.localized``.
Qt-free; the GUI chooses the language at start and on the toolbar switch.
"""
from __future__ import annotations

from .i18n_de import DE

LANGUAGES = {"en": "English", "de": "Deutsch"}
_TABLES: dict[str, dict[str, str]] = {"de": DE}
_language = "en"


def set_language(code: str) -> None:
    """Select a language from ``LANGUAGES``; unknown codes select English."""
    global _language
    _language = code if code in LANGUAGES else "en"


def language() -> str:
    return _language


def tr(text: str, **values: object) -> str:
    """Translate ``text`` and fill ``{placeholders}`` from ``values``."""
    result = _TABLES.get(_language, {}).get(text, text)
    return result.format(**values) if values else result
