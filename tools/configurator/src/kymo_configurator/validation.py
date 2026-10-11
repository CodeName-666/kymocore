# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Stable, field-addressable validation shared by CLI and GUI."""
from __future__ import annotations
import re
from dataclasses import asdict, dataclass
from pathlib import PurePath
from typing import Any
from .model import FEATURES
from .resolution import resolve_profile

IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
PORTS = {"generic", "esp32_freertos", "stm32_baremetal"}
TOP_FIELDS={"schema_version","project_name","core_config_api_version","profiles","active_profile","channels","bindings","port","build","resources","symbols","contexts"}

@dataclass(frozen=True)
class Diagnostic:
    code: str
    severity: str
    path: str
    message: str
    suggestion: str = ""
    def to_dict(self): return asdict(self)

def validate(project: dict[str, Any], profile_name: str | None = None) -> list[Diagnostic]:
    out: list[Diagnostic] = []
    def add(code, path, message, suggestion="", severity="error"):
        out.append(Diagnostic(code,severity,path,message,suggestion))
    if not isinstance(project,dict): return [Diagnostic("project.type","error","$","Project root must be an object")]
    for key in sorted(set(project)-TOP_FIELDS): add("schema.unknown_field",key,f"Unknown top-level field {key!r} in schema version 1")
    if type(project.get("schema_version")) is not int: add("schema.type","schema_version","schema_version must be an integer")
    elif project["schema_version"] != 1: add("schema.version","schema_version",f"Unsupported schema version {project['schema_version']}; supported version is 1")
    if not isinstance(project.get("project_name"),str) or not project.get("project_name","").strip(): add("project.name","project_name","Project name must not be empty")
    if type(project.get("core_config_api_version")) is not int or project.get("core_config_api_version") != 1: add("core.api","core_config_api_version","This project targets configuration API version 1")
    profiles=project.get("profiles")
    if not isinstance(profiles,dict) or not profiles:
        add("profiles.empty","profiles","At least one named profile is required"); profiles={}
    selected=profile_name or project.get("active_profile")
    for pname in profiles if isinstance(profiles,dict) else ():
        if not isinstance(pname,str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}",pname): add("profile.name",f"profiles.{pname}","Profile names may contain only letters, digits, underscore and hyphen (1..64 characters)")
    if not isinstance(selected,str) or selected not in profiles:
        add("profile.missing","active_profile",f"Profile {selected!r} does not exist"); selected=None
    port=project.get("port",{})
    if not isinstance(port,dict): add("port.type","port","port must be an object"); port={}
    port_allowed={"kind","capabilities_verified","core_count"}
    for key in sorted(set(port)-port_allowed): add("schema.unknown_field",f"port.{key}",f"Unknown port field {key!r}")
    if port and port.get("kind") not in PORTS: add("port.unknown","port.kind","Unknown port kind")
    build=project.get("build",{})
    if not isinstance(build,dict): add("build.type","build","build must be an object"); build={}
    for key in sorted(set(build)-{"system","output_dir","include_dir","source_dir"}): add("schema.unknown_field",f"build.{key}",f"Unknown build field {key!r}")
    if build.get("system","gcc") not in ("gcc","cmake","platformio"): add("build.system","build.system","Build system must be gcc, cmake, or platformio")
    for key in ("output_dir","include_dir","source_dir"):
        value=build.get(key)
        if value is not None and (not isinstance(value,str) or not value.strip() or PurePath(value).is_absolute() or ".." in PurePath(value).parts): add("build.path",f"build.{key}","Use a non-empty relative path without parent-directory segments")
    resources=project.get("resources",{})
    if not isinstance(resources,dict): add("resources.type","resources","resources must be an object"); resources={}
    for key in sorted(set(resources)-{"ram_budget_bytes","transport_bytes_per_second"}): add("schema.unknown_field",f"resources.{key}",f"Unknown resource field {key!r}")
    for pname,p in profiles.items():
        base=f"profiles.{pname}"
        if not isinstance(p,dict): add("profile.type",base,"Profile must be an object"); continue
        for key in sorted(set(p)-{"enabled","features","trace"}): add("schema.unknown_field",f"{base}.{key}",f"Unknown profile field {key!r}")
        if type(p.get("enabled")) is not bool: add("boolean.type",base+".enabled","enabled must be boolean")
        feat=p.get("features")
        if not isinstance(feat,dict): add("features.type",base+".features","features must be an object"); feat={}
        for key in sorted(set(feat)-set(FEATURES)): add("schema.unknown_field",f"{base}.features.{key}",f"Unknown feature {key!r}")
        for key in FEATURES:
            if type(feat.get(key)) is not bool: add("boolean.type",f"{base}.features.{key}","Feature value must be boolean")
        trace=p.get("trace",{})
        if not isinstance(trace,dict): add("trace.type",base+".trace","trace must be an object"); trace={}
        for key in sorted(set(trace)-{"enabled","isr","scheduler","flow","port","clock","session","buffer","transport","bindings"}): add("schema.unknown_field",f"{base}.trace.{key}",f"Unknown trace field {key!r}")
        for key in ("enabled","isr","scheduler","flow"):
            if key in trace and type(trace[key]) is not bool: add("boolean.type",f"{base}.trace.{key}","Trace option must be boolean")
        sections={"port":{"kind","core_count","scheduler_complete","isr_complete","core_state_complete"},"clock":{"callback","frequency_hz","counter_bits","domain_id","smp_synchronized","reset_rule"},"session":{"callback"},"buffer":{"events_per_core","max_contexts","max_depth","max_isr_nesting"},"transport":{"mode","max_frame_bytes","drain_events","drain_bytes","max_flush_latency_ms","write_borrows_buffer"},"bindings":{"clock","current_context","core_id","critical_enter","critical_exit","session_id","trace_write","mux_write","transport_busy"}}
        for section,allowed in sections.items():
            data=trace.get(section,{})
            if not isinstance(data,dict): add("trace.section_type",f"{base}.trace.{section}",f"trace.{section} must be an object"); continue
            for key in sorted(set(data)-allowed): add("schema.unknown_field",f"{base}.trace.{section}.{key}",f"Unknown trace {section} field {key!r}")
        tport=trace.get("port",{}) if isinstance(trace.get("port",{}),dict) else {}
        if tport.get("kind") is not None and tport["kind"] not in PORTS: add("trace.port_kind",f"{base}.trace.port.kind","Unknown trace port kind")
        if "core_count" in tport and (type(tport["core_count"]) is not int or not 1<=tport["core_count"]<=2): add("trace.core_count",f"{base}.trace.port.core_count","core_count must be 1 or 2")
        for key in ("scheduler_complete", "isr_complete", "core_state_complete"):
            if key in tport and type(tport[key]) is not bool:
                add("boolean.type",f"{base}.trace.port.{key}",f"{key} must be boolean")
            elif tport.get(key, False) and port.get("capabilities_verified") is not True:
                add("trace.capability_unverified",f"{base}.trace.port.{key}","Trace capability claims require port.capabilities_verified=true")
        clock=trace.get("clock",{}) if isinstance(trace.get("clock",{}),dict) else {}
        if clock.get("callback") and (not isinstance(clock["callback"],str) or not IDENT.fullmatch(clock["callback"])): add("trace.clock_callback",f"{base}.trace.clock.callback","Clock callback must be an identifier")
        if "frequency_hz" in clock and (type(clock["frequency_hz"]) is not int or clock["frequency_hz"]<=0): add("trace.clock_frequency",f"{base}.trace.clock.frequency_hz","Clock frequency must be a positive integer")
        if "counter_bits" in clock and clock["counter_bits"] not in (32,64): add("trace.counter_bits",f"{base}.trace.clock.counter_bits","counter_bits must be 32 or 64")
        if "domain_id" in clock and (type(clock["domain_id"]) is not int or not 0<=clock["domain_id"]<=65535): add("trace.domain_id",f"{base}.trace.clock.domain_id","Clock domain ID must be 0..65535")
        if "smp_synchronized" in clock and type(clock["smp_synchronized"]) is not bool: add("boolean.type",f"{base}.trace.clock.smp_synchronized","smp_synchronized must be boolean")
        if clock.get("reset_rule") not in (None,"new_session"): add("trace.reset_rule",f"{base}.trace.clock.reset_rule","Only reset rule new_session is supported")
        session=trace.get("session",{}) if isinstance(trace.get("session",{}),dict) else {}
        if session.get("callback") and (not isinstance(session["callback"],str) or not IDENT.fullmatch(session["callback"])): add("trace.session_callback",f"{base}.trace.session.callback","Session callback must be an identifier")
        buffer=trace.get("buffer",{}) if isinstance(trace.get("buffer",{}),dict) else {}
        for key,low,high in (("events_per_core",16,4096),("max_contexts",1,256),("max_depth",1,255),("max_isr_nesting",1,16)):
            value=buffer.get(key)
            if value is not None and (type(value) is not int or not low<=value<=high or (key=="events_per_core" and value&(value-1))): add("trace.buffer_limit",f"{base}.trace.buffer.{key}",f"{key} must be in {low}..{high}"+(" and a power of two" if key=="events_per_core" else ""))
        transport=trace.get("transport",{}) if isinstance(trace.get("transport",{}),dict) else {}
        if transport.get("mode") not in (None,"dedicated_trace","shared_v2"): add("trace.transport_mode",f"{base}.trace.transport.mode","Transport mode must be dedicated_trace or shared_v2")
        for key,low,high in (("max_frame_bytes",64,240),("drain_events",1,4096),("drain_bytes",1,65535),("max_flush_latency_ms",1,2147483647)):
            value=transport.get(key)
            if value is not None and (type(value) is not int or not low<=value<=high): add("trace.transport_limit",f"{base}.trace.transport.{key}",f"{key} must be {low}..{high}")
        if "write_borrows_buffer" in transport and type(transport["write_borrows_buffer"]) is not bool:
            add("boolean.type",f"{base}.trace.transport.write_borrows_buffer","write_borrows_buffer must be boolean")
        trace_bindings=trace.get("bindings",{}) if isinstance(trace.get("bindings",{}),dict) else {}
        for key,value in trace_bindings.items():
            if not isinstance(value,str) or not IDENT.fullmatch(value): add("trace.binding",f"{base}.trace.bindings.{key}","Trace binding must be a valid identifier")
        if p.get("enabled") and trace.get("enabled") and trace.get("scheduler") and not trace.get("isr"):
            add("trace.scheduler_requires_isr",f"{base}.trace.scheduler","Scheduler tracing requires ISR tracing","Enable trace.isr or disable scheduler tracing.")
        if trace.get("enabled") and trace.get("scheduler") and p.get("enabled") and port.get("capabilities_verified") is not True:
            add("port.scheduler_unverified",f"{base}.trace.scheduler","Scheduler tracing is unavailable: this port has no verified scheduler/ISR capability","Select a port implementation that has passed capability tests.")
        if trace.get("enabled") and p.get("enabled"):
            callbacks = {
                "clock": trace_bindings.get("clock") or clock.get("callback"),
                "core_id": trace_bindings.get("core_id"),
                "current_context": trace_bindings.get("current_context"),
                "session_id": trace_bindings.get("session_id") or session.get("callback"),
                "trace_write": trace_bindings.get("trace_write") or trace_bindings.get("mux_write"),
            }
            for key, callback in callbacks.items():
                if not isinstance(callback, str) or not IDENT.fullmatch(callback):
                    add("trace.binding_required",f"{base}.trace.bindings.{key}",
                        f"An active trace profile requires a valid {key} callback binding")
            if bool(trace_bindings.get("critical_enter")) != bool(trace_bindings.get("critical_exit")):
                add("trace.critical_pair",f"{base}.trace.bindings",
                    "critical_enter and critical_exit must be supplied together")
            if transport.get("write_borrows_buffer", False) and not trace_bindings.get("transport_busy"):
                add("trace.busy_required",f"{base}.trace.bindings.transport_busy",
                    "A busy callback is required while the trace writer borrows its input buffer")
    channels=project.get("channels",[])
    if not isinstance(channels,list): add("channels.type","channels","channels must be an array"); channels=[]
    ids=set()
    effective={k:False for k in FEATURES}
    if selected:
        try: effective=resolve_profile(project,selected)["effective_features"]
        except (KeyError,TypeError,AttributeError): pass
    if effective.get("runtime") and not 1<=len(channels)<=256: add("channels.count","channels","Active runtime requires 1..256 channels")
    for i,ch in enumerate(channels):
        path=f"channels[{i}]"
        if not isinstance(ch,dict): add("channel.type",path,"Channel must be an object"); continue
        for key in sorted(set(ch)-{"id","name","period_ms","x","z","timestamp"}): add("schema.unknown_field",f"{path}.{key}",f"Unknown channel field {key!r}")
        cid=ch.get("id")
        if type(cid) is not int or not 0<=cid<=255: add("channel.id",path+".id","Channel ID must be an integer from 0 to 255")
        elif cid in ids: add("channel.duplicate_id",path+".id",f"Channel ID {cid} is duplicated")
        else: ids.add(cid)
        if not isinstance(ch.get("name"),str) or not ch.get("name","").strip(): add("channel.name",path+".name","Channel name must not be empty")
        period=ch.get("period_ms")
        if type(period) is not int or not 0<=period<=2147483647: add("channel.period",path+".period_ms","Period must be an integer from 0 to INT32_MAX")
        for key in ("x","z","timestamp"):
            if type(ch.get(key,False)) is not bool: add("boolean.type",path+"."+key,"Channel flag must be boolean")
            elif effective.get("runtime") and ch.get(key) and not effective.get(key): add("channel.feature_disabled",path+"."+key,f"Channel requests {key}, but the profile does not enable that feature",f"Enable feature {key} or clear this channel flag.")
    bindings=project.get("bindings",{})
    if not isinstance(bindings,dict): add("bindings.type","bindings","bindings must be an object"); bindings={}
    allowed_bindings={"clock","sample","write","busy","service","write_borrows_buffer","cpp_stream","cpp_clock","cpp_timestamp_default","clock_user","sample_user","transport_user"}
    for key in sorted(set(bindings)-allowed_bindings): add("schema.unknown_field",f"bindings.{key}",f"Unknown binding field {key!r}")
    active_profile = profiles.get(selected, {}) if selected else {}
    active_trace = active_profile.get("trace", {}) if isinstance(active_profile, dict) else {}
    runtime_shared_trace = bool(effective.get("runtime") and
        isinstance(active_trace, dict) and active_trace.get("enabled") and
        isinstance(active_trace.get("transport", {}), dict) and
        active_trace.get("transport", {}).get("mode") == "shared_v2")
    required=("clock","sample") if effective.get("runtime") else ()
    if effective.get("runtime") and not runtime_shared_trace:
        required += ("write",)
    for key in required:
        if not isinstance(bindings.get(key),str) or not IDENT.fullmatch(bindings[key]): add("binding.required",f"bindings.{key}",f"A valid C identifier is required for {key}")
    for key in ("write_borrows_buffer","cpp_timestamp_default"):
        if key in bindings and type(bindings[key]) is not bool: add("boolean.type",f"bindings.{key}","Binding option must be boolean")
    if effective.get("runtime") and not runtime_shared_trace and bindings.get("write_borrows_buffer",True) and not (isinstance(bindings.get("busy"),str) and IDENT.fullmatch(bindings["busy"])): add("binding.busy_required","bindings.busy","A busy callback is required when write borrows the transmit buffer")
    if effective.get("cpp") and not (isinstance(bindings.get("cpp_stream"),str) and IDENT.fullmatch(bindings["cpp_stream"])): add("binding.cpp_stream_required","bindings.cpp_stream","A valid hand-owned KymoStream object binding is required when C++ sender is active")
    if effective.get("cpp") and effective.get("timestamp") and not (isinstance(bindings.get("cpp_clock"),str) and IDENT.fullmatch(bindings["cpp_clock"])): add("binding.cpp_clock_required","bindings.cpp_clock","A no-argument millisecond clock callback is required for timestamped C++ sender")
    if effective.get("cpp") and not effective.get("timestamp") and bindings.get("cpp_timestamp_default") is True: add("binding.cpp_timestamp_disabled","bindings.cpp_timestamp_default","C++ timestamp default cannot be enabled when the DATA timestamp feature is disabled")
    for key in ("clock","sample","write","busy","service","cpp_stream","cpp_clock","clock_user","sample_user","transport_user"):
        value=bindings.get(key)
        if value and (not isinstance(value,str) or not IDENT.fullmatch(value)): add("binding.identifier",f"bindings.{key}","Binding must be a valid C/C++ identifier")
    for group,maximum in (("symbols",65535),("contexts",4294967294)):
        rows=project.get(group,[]); seen=set(); seen_names=set()
        if not isinstance(rows,list): add("registry.type",group,"Registry must be an array"); continue
        for i,row in enumerate(rows):
            path=f"{group}[{i}]"
            if not isinstance(row,dict): add("registry.row",path,"Entry must be an object"); continue
            for key in sorted(set(row)-{"id","name","kind"}): add("schema.unknown_field",f"{path}.{key}",f"Unknown registry field {key!r}")
            ident=row.get("id")
            if type(ident) is not int or not 1<=ident<=maximum: add("registry.id",path+".id",f"ID must be from 1 to {maximum}")
            elif ident in seen: add("registry.duplicate",path+".id",f"ID {ident} is duplicated")
            else: seen.add(ident)
            name=row.get("name")
            if not isinstance(name,str) or not IDENT.fullmatch(name): add("registry.name",path+".name","Name must be a valid identifier")
            elif name in seen_names: add("registry.duplicate_name",path+".name",f"Name {name!r} is duplicated")
            else: seen_names.add(name)
            allowed_kinds = {"function", "scope", "marker"} if group == "symbols" else {"main", "task", "isr"}
            if row.get("kind") not in allowed_kinds:
                add("registry.kind",path+".kind",f"Kind must be one of: {', '.join(sorted(allowed_kinds))}")
            if isinstance(name,str) and len(name.encode("utf-8")) > 128:
                add("registry.name_length",path+".name","Name must be at most 128 UTF-8 bytes")
    for section,allowed in (("build",{"system","output_dir","include_dir","source_dir"}),("resources",{"ram_budget_bytes","transport_bytes_per_second"})):
        value=project.get(section,{})
        if not isinstance(value,dict): add("section.type",section,f"{section} must be an object"); continue
        for key in sorted(set(value)-allowed): add("schema.unknown_field",f"{section}.{key}",f"Unknown {section} field {key!r}")
        if section=="resources":
            for key in allowed:
                item=value.get(key)
                if item is not None and (type(item) is not int or item<=0): add("resource.value",f"resources.{key}","Resource budgets must be positive integers or null")
    return out
