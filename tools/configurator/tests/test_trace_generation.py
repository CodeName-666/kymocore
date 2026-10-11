# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Trace protocol metadata and dictionary generation tests."""
from __future__ import annotations

import hashlib
import json

from kymo_configurator.generation import _render
from kymo_configurator.model import default_project


def test_enabled_trace_emits_canonical_records_and_explicit_capabilities():
    project = default_project()
    project["symbols"] = [{"id": 7, "name": "read_sensor", "kind": "function"}]
    project["contexts"] = [{"id": 3, "name": "worker", "kind": "task"}]
    trace = project["profiles"]["development"]["trace"]
    trace["enabled"] = True
    trace["bindings"].update({
        "clock": "trace_clock",
        "core_id": "trace_core_id",
        "current_context": "trace_context",
        "session_id": "trace_session",
        "trace_write": "trace_write",
    })

    files = _render(project, "development")
    metadata = json.loads(files["kymo_trace_metadata.json"])
    dictionary_bytes = files["kymo_trace_dictionary.json"]
    dictionary = json.loads(dictionary_bytes)
    generated_header = files["kymo_trace_metadata.h"].decode("utf-8")

    assert metadata["dictionary_hash"] == hashlib.sha256(dictionary_bytes).hexdigest()
    assert metadata["features"]["trace"] is True
    assert metadata["port_capabilities"]["scheduler_complete"] is False
    assert dictionary["symbols"] == project["symbols"]
    assert dictionary["contexts"] == project["contexts"]
    assert "kymo_trace_session_metadata_length" in generated_header
    assert "kymo_trace_dictionary_length" in generated_header

    metadata_body = _array_bytes(generated_header, "kymo_trace_session_metadata")
    dictionary_body = _array_bytes(generated_header, "kymo_trace_dictionary")
    assert metadata_body == files["kymo_trace_metadata.json"]
    assert dictionary_body == dictionary_bytes


def test_global_profile_off_compiles_metadata_arrays_out():
    project = default_project()
    trace = project["profiles"]["development"]["trace"]
    trace["enabled"] = True
    trace["bindings"].update({
        "clock": "trace_clock",
        "core_id": "trace_core_id",
        "current_context": "trace_context",
        "session_id": "trace_session",
        "trace_write": "trace_write",
    })
    project["profiles"]["development"]["enabled"] = False
    generated_header = _render(project, "development")["kymo_trace_metadata.h"].decode("utf-8")
    assert "#if KYMO_EFFECTIVE_TRACE" in generated_header
    assert "static const uint8_t kymo_trace_session_metadata[]" in generated_header


def _array_bytes(header: str, name: str) -> bytes:
    prefix = f"static const uint8_t {name}[] = {{"
    line = next(row for row in header.splitlines() if row.startswith(prefix))
    tokens = line[len(prefix):-2].split(", ")
    return bytes(int(token[:-1], 16) for token in tokens)
