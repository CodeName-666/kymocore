# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Explicit schema migration entry point; preserves unknown/user data."""
from __future__ import annotations
import copy
from typing import Any


def migrate(project: dict[str, Any], to_schema: int = 1) -> dict[str, Any]:
    if to_schema != 1: raise ValueError("Only target schema 1 is currently supported")
    result=copy.deepcopy(project)
    version=result.get("schema_version")
    if version == 1: return result
    if version is None and "profiles" in result:
        # Early development fixture; add the fields that did not change meaning.
        result["schema_version"]=1
        result.setdefault("active_profile", next(iter(result.get("profiles",{})), "development"))
        result.setdefault("core_config_api_version",1)
        return result
    raise ValueError(f"No migration path from schema {version!r} to 1")
