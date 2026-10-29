# SPDX-License-Identifier: MIT
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
#include <KymoCore.h>
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
#include <kymo_config.h>
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
    profile=project.get("profiles",{}).get(project.get("active_profile"),{})
    trace=profile.get("trace",{}) if isinstance(profile,dict) else {}
    if isinstance(trace,dict) and trace.get("enabled"):
        tb=trace.get("bindings",{}); session=tb.get("session_id") or trace.get("session",{}).get("callback")
        stubs=[(tb.get("clock"),"uint64_t {name}(void *user)","return the free-running trace tick counter","0u"),
               (session,"uint64_t {name}(void)","return a new random session ID per boot","0u"),
               (tb.get("trace_write"),"size_t {name}(void *user, const uint8_t *bytes, size_t length)","write a prefix of the mux frame and return its length","0u"),
               (tb.get("transport_busy"),"uint8_t {name}(void *user)","report nonzero while the driver owns the frame","0u")]
        declarations=declarations.replace("#ifdef __cplusplus\n}\n#endif\n","")
        declarations+="#if KYMO_EFFECTIVE_TRACE\n"+"".join(f"{sig.format(name=name)};\n" for name,sig,_,_ in stubs if name)+"#endif\n#ifdef __cplusplus\n}\n#endif\n"
        bodies+="#if KYMO_EFFECTIVE_TRACE\n"
        for name,sig,todo,value in stubs:
            if name:
                params=[p.strip().split()[-1].lstrip("*") for p in sig.split("(",1)[1].rstrip(")").split(",") if p.strip() not in ("void","")]
                unused=" ".join(f"(void){p};" for p in params)
                unused_line=f"    {unused}\n" if unused else ""
                bodies+=f'extern "C" {sig.format(name=name)} {{\n{unused_line}    /* TODO: {todo}. */\n    return {value};\n}}\n'
        bodies+="#endif /* KYMO_EFFECTIVE_TRACE */\n"
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
