# SPDX-License-Identifier: MIT
from __future__ import annotations
from typing import Any
from .model import FEATURES, OPTIONAL_FEATURES


def resolve_profile(project: dict[str, Any], profile_name: str) -> dict[str, Any]:
    profile = project["profiles"][profile_name]
    enabled = profile["enabled"]
    requested = dict(profile["features"])
    trace = dict(profile.get("trace", {}))
    effective = {key: bool(enabled and requested[key]) for key in FEATURES}
    effective.update({key: bool(enabled and requested.get(key, False)) for key in OPTIONAL_FEATURES})
    trace_enabled = bool(enabled and trace.get("enabled", False))
    trace_effective = {"enabled": trace_enabled}
    trace_effective.update({key: bool(trace_enabled and trace.get(key, False))
                            for key in ("isr", "scheduler", "flow")})
    return {"name": profile_name, "enabled": enabled, "requested_features": requested,
            "effective_features": effective, "requested_trace": trace,
            "effective_trace": trace_effective, "channels": project.get("channels", []),
            "bindings": project.get("bindings", {}), "port": project.get("port", {}),
            "build": project.get("build", {})}
