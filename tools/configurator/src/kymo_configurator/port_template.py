# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Create an explicitly requested, never-overwriting callback scaffold."""
from __future__ import annotations
import shutil, tempfile
from pathlib import Path
from typing import Any

def render_port_template(project:dict[str,Any])->dict[str,str]:
    b=project.get("bindings",{})
    clock=b.get("clock","kymo_port_clock_ms"); sample=b.get("sample","kymo_port_sample"); write=b.get("write","kymo_port_write")
    busy=b.get("busy","kymo_port_busy"); service=b.get("service","kymo_port_service")
    declarations=f'''#pragma once
#include <kymo_runtime.h>
#ifdef __cplusplus
extern "C" {{
#endif
uint32_t {clock}(void *user);
uint8_t {sample}(void *user, uint8_t id, KymoSample *sample);
uint8_t {write}(void *user, const uint8_t *bytes, uint8_t length);
'''
    if busy: declarations+=f"uint8_t {busy}(void *user);\n"
    if service: declarations+=f"void {service}(void *user);\n"
    declarations+="#ifdef __cplusplus\n}\n#endif\n"
    bodies=f'''/* User-owned starter only. Replace each TODO with target-specific behavior. */
#include "kymo_port.h"
#include <kymo_features.h>
#if KYMO_EFFECTIVE_RUNTIME
#error Implement the Kymo port callbacks in this file before enabling runtime
extern "C" uint32_t {clock}(void *user) {{
    (void)user;
    /* TODO: return a monotonic millisecond counter. */
    return 0u;
}}
extern "C" uint8_t {sample}(void *user, uint8_t id, KymoSample *sample) {{
    (void)user; (void)id; (void)sample;
    /* TODO: copy a measurement and return nonzero only when available. */
    return 0u;
}}
extern "C" uint8_t {write}(void *user, const uint8_t *bytes, uint8_t length) {{
    (void)user; (void)bytes; (void)length;
    /* TODO: accept and report a prefix of the provided bytes. */
    return 0u;
}}
'''
    if busy: bodies+=f'''extern "C" uint8_t {busy}(void *user) {{
    (void)user;
    /* TODO: report whether a borrowed TX buffer is still in use. */
    return 0u;
}}
'''
    if service: bodies+=f'''extern "C" void {service}(void *user) {{
    (void)user;
    /* TODO: perform bounded transport service, if required. */
}}
'''
    bodies+="#endif /* KYMO_EFFECTIVE_RUNTIME */\n"
    return {"kymo_port.h":declarations,"kymo_port.cpp":bodies}

def create_port_template(project:dict[str,Any],destination:str|Path)->Path:
    target=Path(destination).resolve()
    if target.exists(): raise FileExistsError(f"Refusing to overwrite existing port template path: {target}")
    target.parent.mkdir(parents=True,exist_ok=True)
    staging=Path(tempfile.mkdtemp(prefix=".kymo-port-",dir=target.parent))
    try:
        for name,content in render_port_template(project).items(): (staging/name).write_text(content,encoding="utf-8",newline="\n")
        staging.replace(target)
    except Exception:
        shutil.rmtree(staging,ignore_errors=True); raise
    return target
