# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Use-case presets that mirror the KymoCore 7.0 KYMO_PROFILE values.

A preset only fills feature defaults of one configurator profile; every value can
be changed afterwards (the profile then counts as "custom"). Qt-free.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True)
class Preset:
    """One selectable starting point."""
    key: str
    title: str
    question: str
    description: str
    core_profile: str
    features: dict[str, bool] = field(default_factory=dict)
    trace: dict[str, bool] = field(default_factory=dict)


_BASE = {"runtime": True, "x": False, "z": False, "timestamp": False,
         "crc": False, "decoder": False, "cpp": False}
_NO_TRACE = {"enabled": False, "isr": False, "scheduler": False, "flow": False}

PRESETS: dict[str, Preset] = {
    "minimal": Preset(
        "minimal", "Live values",
        "Show single measurements (temperature, current, ...) live as curves",
        "One value per channel, as small as possible. Matches KYMO_PROFILE_MINIMAL.",
        "KYMO_PROFILE_MINIMAL", dict(_BASE), dict(_NO_TRACE)),
    "xyz": Preset(
        "xyz", "XY / XYZ data with time",
        "Plot X/Y or X/Y/Z points with timestamps (positions, trajectories, phase plots)",
        "Enables X, Z and timestamps. Matches KYMO_PROFILE_XYZ.",
        "KYMO_PROFILE_XYZ", {**_BASE, "x": True, "z": True, "timestamp": True},
        dict(_NO_TRACE)),
    "trace": Preset(
        "trace", "Execution tracing",
        "Record which functions, interrupts and flows run when (timing analysis)",
        "XYZ telemetry plus trace recorder with ISR and flow events. Scheduler events "
        "additionally need a verified target port. Corresponds to KYMO_PROFILE_TRACE.",
        "KYMO_PROFILE_TRACE", {**_BASE, "x": True, "z": True, "timestamp": True},
        {"enabled": True, "isr": True, "scheduler": False, "flow": True}),
}

CUSTOM = "custom"

TRACE_DEFAULT_BINDINGS = {"clock": "kymo_port_trace_clock", "trace_write": "kymo_port_trace_write"}
"""Function names suggested when tracing is chosen; implemented in the port file."""


def apply_preset(profile: dict[str, Any], key: str) -> None:
    """Overwrite the feature and trace switches of a profile with a preset."""
    preset = PRESETS[key]
    profile.setdefault("features", {}).update(preset.features)
    trace = profile.setdefault("trace", {})
    trace.update(preset.trace)
    if preset.trace.get("enabled"):
        bindings = trace.setdefault("bindings", {})
        for name, function in TRACE_DEFAULT_BINDINGS.items():
            bindings.setdefault(name, function)
    profile["preset"] = key


def matching_preset(profile: dict[str, Any]) -> str:
    """Return the preset whose switches equal the profile, else "custom"."""
    features = profile.get("features", {})
    trace = profile.get("trace", {})
    result = CUSTOM
    for key, preset in PRESETS.items():
        if (all(features.get(k) == v for k, v in preset.features.items()) and
                all(bool(trace.get(k, False)) == v for k, v in preset.trace.items())):
            result = key
            break
    return result
