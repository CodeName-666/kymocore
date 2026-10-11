# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
from __future__ import annotations
import copy, difflib, json
from pathlib import Path
from PySide6.QtCore import Qt, QAbstractTableModel, QModelIndex
from PySide6.QtWidgets import (QMainWindow,QWidget,QTabWidget,QVBoxLayout,QHBoxLayout,QFormLayout,
    QLineEdit,QCheckBox,QComboBox,QSpinBox,QTableView,QPushButton,QLabel,QPlainTextEdit,QFileDialog,
    QMessageBox,QGroupBox,QSplitter,QScrollArea)
from ..generation import generate, _render
from ..model import default_project,load_project,save_project,FEATURES
from ..resolution import resolve_profile
from ..validation import validate

FEATURE_LABELS={"runtime":"Cooperative runtime","x":"X coordinate","z":"Z coordinate","timestamp":"DATA timestamp","crc":"DATA CRC","decoder":"MCU decoder","cpp":"C++ push sender"}

class ChannelModel(QAbstractTableModel):
    headers=["ID","Name","Period (ms)","X","Z","Timestamp"]
    keys=["id","name","period_ms","x","z","timestamp"]
    def __init__(self, rows, changed): super().__init__(); self.rows=rows; self.changed=changed
    def rowCount(self,parent=QModelIndex()): return len(self.rows)
    def columnCount(self,parent=QModelIndex()): return len(self.keys)
    def data(self,index,role=Qt.ItemDataRole.DisplayRole):
        if not index.isValid(): return None
        value=self.rows[index.row()].get(self.keys[index.column()],False)
        if role in (Qt.ItemDataRole.DisplayRole,Qt.ItemDataRole.EditRole): return str(value) if not isinstance(value,bool) else ("✓" if value else "")
        if role==Qt.ItemDataRole.CheckStateRole and index.column()>=3: return Qt.CheckState.Checked if value else Qt.CheckState.Unchecked
    def headerData(self,section,orientation,role=Qt.ItemDataRole.DisplayRole):
        if role==Qt.ItemDataRole.DisplayRole and orientation==Qt.Orientation.Horizontal: return self.headers[section]
    def flags(self,index):
        if not index.isValid(): return Qt.ItemFlag.NoItemFlags
        flags=Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable
        return flags | (Qt.ItemFlag.ItemIsUserCheckable if index.column()>=3 else Qt.ItemFlag.ItemIsEditable)
    def setData(self,index,value,role=Qt.ItemDataRole.EditRole):
        if not index.isValid(): return False
        key=self.keys[index.column()]
        try:
            if index.column()>=3: value=value in (Qt.CheckState.Checked, Qt.CheckState.Checked.value, int(Qt.CheckState.Checked.value))
            elif key in ("id","period_ms"): value=int(value)
            self.rows[index.row()][key]=value
        except (TypeError,ValueError): return False
        self.dataChanged.emit(index,index,[role]); self.changed(); return True
    def refresh(self): self.beginResetModel(); self.endResetModel()

class RegistryModel(QAbstractTableModel):
    headers=["ID","Name","Kind"]
    def __init__(self,rows,changed): super().__init__(); self.rows=rows; self.changed=changed
    def rowCount(self,parent=QModelIndex()): return len(self.rows)
    def columnCount(self,parent=QModelIndex()): return 3
    def data(self,index,role=Qt.ItemDataRole.DisplayRole):
        if index.isValid() and role in (Qt.ItemDataRole.DisplayRole,Qt.ItemDataRole.EditRole): return str(self.rows[index.row()].get(self.headers[index.column()].lower(),""))
    def headerData(self,section,orientation,role=Qt.ItemDataRole.DisplayRole):
        if role==Qt.ItemDataRole.DisplayRole and orientation==Qt.Orientation.Horizontal: return self.headers[section]
    def flags(self,index): return Qt.ItemFlag.ItemIsEnabled|Qt.ItemFlag.ItemIsSelectable|Qt.ItemFlag.ItemIsEditable
    def setData(self,index,value,role=Qt.ItemDataRole.EditRole):
        if not index.isValid(): return False
        key=("id","name","kind")[index.column()]
        try: self.rows[index.row()][key]=int(value) if key=="id" else str(value)
        except ValueError: return False
        self.dataChanged.emit(index,index,[role]); self.changed(); return True

class MainWindow(QMainWindow):
    def __init__(self,path=None):
        super().__init__(); self.path=path; self.doc=load_project(path) if path and path.exists() else default_project()
        self.dirty=False; self._loading=False; self.setWindowTitle("KymoCore Configurator"); self.resize(1100,760)
        root=QWidget(); outer=QVBoxLayout(root)
        top=QHBoxLayout(); outer.addLayout(top)
        self.project_name=QLineEdit(); self.profile=QComboBox(); self.enabled=QCheckBox("Kymo enabled (KYMO_ENABLED)")
        top.addWidget(QLabel("Project")); top.addWidget(self.project_name,2); top.addWidget(QLabel("Profile")); top.addWidget(self.profile); top.addWidget(self.enabled)
        self.duplicate_profile_button=QPushButton("Duplicate profile"); self.duplicate_profile_button.clicked.connect(self.duplicate_profile); top.addWidget(self.duplicate_profile_button)
        self.tabs=QTabWidget(); outer.addWidget(self.tabs,1)
        self._build_features(); self._build_channels(); self._build_bindings(); self._build_trace(); self._build_output()
        bottom=QHBoxLayout(); outer.addLayout(bottom)
        self.status=QLabel("Ready"); bottom.addWidget(self.status,1)
        for label,fn in (("New",self.new_project),("Open…",self.open_project),("Save",self.save),("Validate",self.validate_now),("Preview",self.preview_now),("Generate",self.generate_now)):
            b=QPushButton(label); b.clicked.connect(fn); bottom.addWidget(b)
        self.setCentralWidget(root)
        self.profile.currentTextChanged.connect(self.profile_changed); self.enabled.toggled.connect(self.mark_dirty)
        self.project_name.textEdited.connect(self.mark_dirty)
        self._load_ui()

    def _build_features(self):
        page=QWidget(); layout=QVBoxLayout(page)
        self.feature_checks={}
        for key,label in FEATURE_LABELS.items():
            box=QGroupBox(label); row=QHBoxLayout(box); cb=QCheckBox("Requested by this profile"); effective=QLabel(); row.addWidget(cb); row.addWidget(effective,1)
            cb.toggled.connect(self.mark_dirty); self.feature_checks[key]=(cb,effective); layout.addWidget(box)
        layout.addStretch(); self.tabs.addTab(page,"Core features")
    def _build_channels(self):
        page=QWidget(); layout=QVBoxLayout(page); self.channel_view=QTableView(); self.channel_view.setAlternatingRowColors(True); self.channel_view.horizontalHeader().setStretchLastSection(True); layout.addWidget(self.channel_view)
        row=QHBoxLayout()
        for title,fn in (("Add channel",self.add_channel),("Duplicate",self.duplicate_channel),("Remove",self.remove_channel)):
            b=QPushButton(title); b.clicked.connect(fn); row.addWidget(b)
        layout.addLayout(row); self.tabs.addTab(page,"Measurements")
    def _build_bindings(self):
        page=QWidget(); form=QFormLayout(page); self.binding_inputs={}
        descriptions={"clock":"Clock callback (uint32_t(void *))","clock_user":"Clock callback context pointer object","sample":"Sample callback (uint8_t(void *, id, sample))","sample_user":"Sample callback context pointer object","write":"Transport write callback","busy":"TX buffer ownership / busy callback","service":"Optional transport service","transport_user":"Shared transport context pointer object","cpp_stream":"Optional C++ KymoStream reference","cpp_clock":"C++ timestamp clock binding"}
        for key,label in descriptions.items():
            edit=QLineEdit(); edit.textEdited.connect(self.mark_dirty); self.binding_inputs[key]=edit; form.addRow(label,edit)
        self.borrow=QCheckBox("Write callback borrows TX buffer (requires busy callback)"); self.borrow.toggled.connect(self.mark_dirty); form.addRow(self.borrow)
        self.cpp_timestamp_default=QCheckBox("C++ sender permits requested timestamps by default"); self.cpp_timestamp_default.toggled.connect(self.mark_dirty); form.addRow(self.cpp_timestamp_default)
        note=QLabel("Callbacks are references to hand-written port code. The generator never creates or overwrites their implementations."); note.setWordWrap(True); form.addRow(note)
        self.tabs.addTab(page,"Callbacks & transport")
    def _build_trace(self):
        page=QWidget(); form=QFormLayout(page); self.trace_checks={}
        for key,label in (("enabled","Trace recording"),("isr","ISR hooks"),("scheduler","Scheduler tracing"),("flow","Flow tracing")):
            cb=QCheckBox(label); cb.toggled.connect(self.mark_dirty); self.trace_checks[key]=cb; form.addRow(cb)
        self.port=QComboBox(); self.port.addItems(["generic","esp32_freertos","stm32_baremetal"]); self.port.currentTextChanged.connect(self.mark_dirty); form.addRow("Target port",self.port)
        self.trace_core_count=self._spin(1,2,1,form,"Core count")
        self.trace_frequency=self._spin(1,2147483647,1000000,form,"Clock frequency (Hz)")
        self.trace_counter_bits=QComboBox(); self.trace_counter_bits.addItems(["32","64"]); self.trace_counter_bits.currentTextChanged.connect(self.mark_dirty); form.addRow("Counter width",self.trace_counter_bits)
        self.trace_domain=self._spin(0,65535,1,form,"Clock domain ID")
        self.trace_smp=QCheckBox("Clock synchronized across cores"); self.trace_smp.toggled.connect(self.mark_dirty); form.addRow(self.trace_smp)
        self.trace_events=self._spin(16,4096,16,form,"Events per core (power of two)")
        self.trace_contexts=self._spin(1,256,16,form,"Maximum contexts")
        self.trace_depth=self._spin(1,255,32,form,"Maximum scope depth")
        self.trace_isr_depth=self._spin(1,16,4,form,"Maximum ISR nesting")
        self.trace_transport=QComboBox(); self.trace_transport.addItems(["dedicated_trace","shared_v2"]); self.trace_transport.currentTextChanged.connect(self.mark_dirty); form.addRow("Transport mode",self.trace_transport)
        self.trace_frame=self._spin(64,240,128,form,"Maximum frame bytes")
        self.trace_drain_events=self._spin(1,4096,16,form,"Drain event budget")
        self.trace_drain_bytes=self._spin(1,65535,256,form,"Drain byte budget")
        self.trace_latency=self._spin(1,2147483647,10,form,"Maximum flush latency (ms)")
        self.trace_borrow=QCheckBox("Trace writer borrows frame memory until transport is no longer busy"); self.trace_borrow.toggled.connect(self.mark_dirty); form.addRow(self.trace_borrow)
        self.trace_session=QLineEdit("kymo_trace_session_id"); self.trace_session.textEdited.connect(self.mark_dirty); form.addRow("Session ID callback",self.trace_session)
        self.trace_binding_inputs={}
        for key,label in (("clock","Clock"),("current_context","Current context"),("core_id","Core ID"),("critical_enter","Critical enter"),("critical_exit","Critical exit"),("session_id","Session ID"),("trace_write","Trace write"),("transport_busy","Transport busy")):
            edit=QLineEdit(); edit.textEdited.connect(self.mark_dirty); self.trace_binding_inputs[key]=edit; form.addRow("Binding · "+label,edit)
        self.symbol_view=QTableView(); self.symbol_view.setMaximumHeight(125); self.context_view=QTableView(); self.context_view.setMaximumHeight(125)
        symbol_group=QGroupBox("Symbol dictionary"); sy=QVBoxLayout(symbol_group); sy.addWidget(self.symbol_view); symbol_buttons=QHBoxLayout()
        for label,fn in (("Add symbol",lambda:self.add_registry("symbols")),("Remove",lambda:self.remove_registry("symbols"))): b=QPushButton(label); b.clicked.connect(fn); symbol_buttons.addWidget(b)
        sy.addLayout(symbol_buttons); form.addRow(symbol_group)
        context_group=QGroupBox("Context dictionary"); cx=QVBoxLayout(context_group); cx.addWidget(self.context_view); context_buttons=QHBoxLayout()
        for label,fn in (("Add context",lambda:self.add_registry("contexts")),("Remove",lambda:self.remove_registry("contexts"))): b=QPushButton(label); b.clicked.connect(fn); context_buttons.addWidget(b)
        cx.addLayout(context_buttons); form.addRow(context_group)
        status=QLabel("Trace generates recorder and transport bindings. Scheduler and ISR capability claims require verified target integration; native tests alone do not verify hardware coverage."); status.setWordWrap(True); form.addRow(status)
        scroll=QScrollArea(); scroll.setWidgetResizable(True); scroll.setWidget(page); self.tabs.addTab(scroll,"Trace")
    def _spin(self,minimum,maximum,default,form,label):
        widget=QSpinBox(); widget.setRange(minimum,maximum); widget.setValue(default); widget.valueChanged.connect(self.mark_dirty); form.addRow(label,widget); return widget
    def _build_output(self):
        page=QWidget(); form=QFormLayout(page); self.build_system=QComboBox(); self.build_system.addItems(["gcc","cmake","platformio"]); self.build_system.currentTextChanged.connect(self.mark_dirty)
        self.out_dir=QLineEdit(); self.inc_dir=QLineEdit(); self.src_dir=QLineEdit()
        for w in (self.out_dir,self.inc_dir,self.src_dir): w.textEdited.connect(self.mark_dirty)
        form.addRow("Build export",self.build_system); form.addRow("Generated output",self.out_dir); form.addRow("Project include directory",self.inc_dir); form.addRow("Project source directory",self.src_dir)
        self.ram_budget=QLineEdit(); self.transport_budget=QLineEdit()
        self.ram_budget.setPlaceholderText("optional bytes"); self.transport_budget.setPlaceholderText("optional bytes/second")
        self.ram_budget.textEdited.connect(self.mark_dirty); self.transport_budget.textEdited.connect(self.mark_dirty)
        form.addRow("Target RAM budget (optional)",self.ram_budget); form.addRow("Transport budget (optional)",self.transport_budget)
        self.preview=QPlainTextEdit(); self.preview.setReadOnly(True); self.preview.setPlaceholderText("Effective settings, diagnostics and generated file preview appear here."); form.addRow("Preview",self.preview)
        self.tabs.addTab(page,"Build & preview")
    def active(self): return self.doc["profiles"][self.profile.currentText()]
    def mark_dirty(self,*_):
        if self._loading: return
        self.dirty=True; self.status.setText("Unsaved changes"); self._pull_ui(); self._update_effective()
    def _load_ui(self):
        self._loading=True; self.project_name.setText(self.doc.get("project_name","")); self.profile.clear(); self.profile.addItems(list(self.doc.get("profiles",{})))
        if self.doc.get("active_profile") in self.doc.get("profiles",{}): self.profile.setCurrentText(self.doc["active_profile"])
        self.profile_changed(self.profile.currentText()); self.dirty=False; self._loading=False; self._update_effective()
    def profile_changed(self,name):
        if not name or name not in self.doc.get("profiles",{}): return
        self._loading=True; p=self.doc["profiles"][name]; self.enabled.setChecked(p.get("enabled",False))
        for key,(cb,_) in self.feature_checks.items(): cb.setChecked(p.get("features",{}).get(key,False))
        trace=p.get("trace",{})
        for key,cb in self.trace_checks.items(): cb.setChecked(trace.get(key,False))
        tp=trace.get("port",{}); clock=trace.get("clock",{}); buf=trace.get("buffer",{}); transport=trace.get("transport",{})
        self.trace_core_count.setValue(tp.get("core_count",1)); self.trace_frequency.setValue(clock.get("frequency_hz",1000000)); self.trace_counter_bits.setCurrentText(str(clock.get("counter_bits",64))); self.trace_domain.setValue(clock.get("domain_id",1)); self.trace_smp.setChecked(clock.get("smp_synchronized",False))
        self.trace_events.setValue(buf.get("events_per_core",16)); self.trace_contexts.setValue(buf.get("max_contexts",16)); self.trace_depth.setValue(buf.get("max_depth",32)); self.trace_isr_depth.setValue(buf.get("max_isr_nesting",4))
        self.trace_transport.setCurrentText(transport.get("mode","dedicated_trace")); self.trace_frame.setValue(transport.get("max_frame_bytes",128)); self.trace_drain_events.setValue(transport.get("drain_events",16)); self.trace_drain_bytes.setValue(transport.get("drain_bytes",256)); self.trace_latency.setValue(transport.get("max_flush_latency_ms",10)); self.trace_session.setText(trace.get("session",{}).get("callback","kymo_trace_session_id"))
        self.trace_borrow.setChecked(transport.get("write_borrows_buffer",False))
        for key,w in self.trace_binding_inputs.items(): w.setText(str(trace.get("bindings",{}).get(key,"")))
        self.symbol_model=RegistryModel(self.doc.setdefault("symbols",[]),self.mark_dirty); self.symbol_view.setModel(self.symbol_model)
        self.context_model=RegistryModel(self.doc.setdefault("contexts",[]),self.mark_dirty); self.context_view.setModel(self.context_model)
        channels=self.doc.setdefault("channels",[]); self.channel_model=ChannelModel(channels,self.mark_dirty); self.channel_view.setModel(self.channel_model)
        bindings=self.doc.setdefault("bindings",{})
        for key,w in self.binding_inputs.items(): w.setText(str(bindings.get(key,"")))
        self.borrow.setChecked(bindings.get("write_borrows_buffer",True))
        self.cpp_timestamp_default.setChecked(bindings.get("cpp_timestamp_default",False))
        port=self.doc.setdefault("port",{}); self.port.setCurrentText(port.get("kind","generic"))
        build=self.doc.setdefault("build",{}); self.build_system.setCurrentText(build.get("system","gcc")); self.out_dir.setText(build.get("output_dir","generated/kymo")); self.inc_dir.setText(build.get("include_dir","include")); self.src_dir.setText(build.get("source_dir","src"))
        resources=self.doc.setdefault("resources",{}); self.ram_budget.setText("" if resources.get("ram_budget_bytes") is None else str(resources.get("ram_budget_bytes"))); self.transport_budget.setText("" if resources.get("transport_bytes_per_second") is None else str(resources.get("transport_bytes_per_second")))
        changed=self.doc.get("active_profile")!=name
        self.doc["active_profile"]=name; self._loading=False; self._update_effective()
        if changed: self.dirty=True; self.status.setText("Profile selection changed")
    def _pull_ui(self):
        if not self.doc.get("profiles") or self.profile.currentText() not in self.doc["profiles"]: return
        self.doc["project_name"]=self.project_name.text(); self.doc["active_profile"]=self.profile.currentText()
        p=self.active(); p["enabled"]=self.enabled.isChecked(); p.setdefault("features",{})
        for key,(cb,_) in self.feature_checks.items(): p["features"][key]=cb.isChecked()
        trace=p.setdefault("trace",{}); trace.update({key:cb.isChecked() for key,cb in self.trace_checks.items()})
        trace.setdefault("port",{}).update({"kind":self.port.currentText(),"core_count":self.trace_core_count.value()})
        trace.setdefault("clock",{}).update({"frequency_hz":self.trace_frequency.value(),"counter_bits":int(self.trace_counter_bits.currentText()),"domain_id":self.trace_domain.value(),"smp_synchronized":self.trace_smp.isChecked(),"reset_rule":"new_session"})
        trace.setdefault("session",{})["callback"]=self.trace_session.text().strip()
        trace.setdefault("buffer",{}).update({"events_per_core":self.trace_events.value(),"max_contexts":self.trace_contexts.value(),"max_depth":self.trace_depth.value(),"max_isr_nesting":self.trace_isr_depth.value()})
        trace.setdefault("transport",{}).update({"mode":self.trace_transport.currentText(),"max_frame_bytes":self.trace_frame.value(),"drain_events":self.trace_drain_events.value(),"drain_bytes":self.trace_drain_bytes.value(),"max_flush_latency_ms":self.trace_latency.value()})
        trace["transport"]["write_borrows_buffer"]=self.trace_borrow.isChecked()
        trace_bindings=trace.setdefault("bindings",{})
        for key,w in self.trace_binding_inputs.items():
            value=w.text().strip()
            if value: trace_bindings[key]=value
            else: trace_bindings.pop(key,None)
        b=self.doc.setdefault("bindings",{})
        for key,w in self.binding_inputs.items():
            value=w.text().strip()
            if value: b[key]=value
            else: b.pop(key,None)
        b["write_borrows_buffer"]=self.borrow.isChecked()
        b["cpp_timestamp_default"]=self.cpp_timestamp_default.isChecked()
        port=self.doc.setdefault("port",{})
        if port.get("kind") != self.port.currentText(): port["capabilities_verified"]=False
        port["kind"]=self.port.currentText()
        self.doc["build"]={"system":self.build_system.currentText(),"output_dir":self.out_dir.text(),"include_dir":self.inc_dir.text(),"source_dir":self.src_dir.text()}
        def optional_int(text):
            text=text.strip()
            return None if not text else (int(text) if text.isdigit() else text)
        self.doc["resources"]={"ram_budget_bytes":optional_int(self.ram_budget.text()),"transport_bytes_per_second":optional_int(self.transport_budget.text())}
    def _update_effective(self):
        if not self.profile.currentText(): return
        self._pull_ui()
        for key,(cb,label) in self.feature_checks.items():
            effective=bool(self.enabled.isChecked() and cb.isChecked()); label.setText("Effective: ON" if effective else ("Stored; compile-out active" if cb.isChecked() and not self.enabled.isChecked() else "Effective: OFF"))
            label.setStyleSheet("color:#16804a" if effective else "color:#687383")
            cb.setEnabled(self.enabled.isChecked())
        runtime_on=self.enabled.isChecked() and self.feature_checks["runtime"][0].isChecked()
        self.channel_view.setEnabled(runtime_on)
        for button in self.findChildren(QPushButton):
            if button.text() in ("Add channel","Duplicate","Remove"): button.setEnabled(runtime_on)
        for cb in self.trace_checks.values(): cb.setEnabled(self.enabled.isChecked())
        for widget in (self.trace_core_count,self.trace_frequency,self.trace_counter_bits,self.trace_domain,self.trace_smp,self.trace_events,self.trace_contexts,self.trace_depth,self.trace_isr_depth,self.trace_transport,self.trace_frame,self.trace_drain_events,self.trace_drain_bytes,self.trace_latency,self.trace_session,self.symbol_view,self.context_view): widget.setEnabled(self.enabled.isChecked())
        for widget in self.trace_binding_inputs.values(): widget.setEnabled(self.enabled.isChecked())
        for key,widget in self.binding_inputs.items():
            if key in ("cpp_stream","cpp_clock"): widget.setEnabled(self.enabled.isChecked() and self.feature_checks["cpp"][0].isChecked())
            else: widget.setEnabled(runtime_on)
        self.borrow.setEnabled(runtime_on)
        self.cpp_timestamp_default.setEnabled(self.enabled.isChecked() and self.feature_checks["cpp"][0].isChecked())
    def _ask_save_dirty(self):
        if not self.dirty: return True
        ans=QMessageBox.question(self,"Unsaved changes","Save changes before continuing?",QMessageBox.StandardButton.Save|QMessageBox.StandardButton.Discard|QMessageBox.StandardButton.Cancel)
        if ans==QMessageBox.StandardButton.Cancel: return False
        return self.save() if ans==QMessageBox.StandardButton.Save else True
    def new_project(self):
        if not self._ask_save_dirty(): return
        self.doc=default_project(); self.path=None; self._load_ui()
    def open_project(self):
        if not self._ask_save_dirty(): return
        name,_=QFileDialog.getOpenFileName(self,"Open Kymo project","","Kymo project (*.json);;JSON files (*.json)")
        if name:
            try: self.doc=load_project(name); self.path=Path(name); self._load_ui()
            except ValueError as exc: QMessageBox.critical(self,"Open failed",str(exc))
    def save(self):
        self._pull_ui()
        if not self.path:
            name,_=QFileDialog.getSaveFileName(self,"Save Kymo project","kymo.project.json","Kymo project (*.json)")
            if not name: return False
            self.path=Path(name)
        try: save_project(self.path,self.doc); self.dirty=False; self.status.setText(f"Saved {self.path}"); return True
        except OSError as exc: QMessageBox.critical(self,"Save failed",str(exc)); return False
    def validate_now(self):
        self._pull_ui(); ds=validate(self.doc,self.profile.currentText()); self.preview.setPlainText("\n".join(f"{d.severity.upper()} {d.code} · {d.path}: {d.message}" for d in ds) or "Configuration is valid.")
        self.status.setText("Valid" if not any(d.severity=="error" for d in ds) else f"{sum(d.severity=='error' for d in ds)} validation error(s)")
    def preview_now(self):
        self._pull_ui(); ds=validate(self.doc,self.profile.currentText()); resolved=resolve_profile(self.doc,self.profile.currentText())
        lines=[f"Project: {self.doc['project_name']}",f"Profile: {resolved['name']}",f"KYMO_ENABLED: {int(resolved['enabled'])}","Effective features:"]
        lines.extend(f"  KYMO_ENABLE_{k.upper()} = {int(v)}" for k,v in resolved["effective_features"].items())
        lines+= ["",f"Channels: {len(self.doc.get('channels',[]))}"]
        rate=sum(1000/max(1,c.get("period_ms",1)) for c in self.doc.get("channels",[]) if c.get("period_ms",0)>0)
        lines.append(f"Nominal DATA rate (periodic channels): {rate:.1f} frames/s; an estimate, not a throughput guarantee.")
        traffic=sum((1000/c["period_ms"])*(9+4*(int(c.get("x",False))+int(c.get("z",False))+int(c.get("timestamp",False)))) for c in self.doc.get("channels",[]) if type(c.get("period_ms")) is int and c.get("period_ms",0)>0)
        lines.append(f"Nominal traffic estimate: {traffic:.1f} bytes/s including the 9-byte base frame; channels with period 0 are excluded because their rate depends on Main call frequency.")
        ram=self.doc.get("resources",{}).get("ram_budget_bytes"); budget=self.doc.get("resources",{}).get("transport_bytes_per_second")
        if ram: lines.append(f"Configured RAM budget: {ram} bytes. Actual static RAM requires a target build/linker report.")
        if budget: lines.append(f"Configured transport budget: {budget} bytes/s; {traffic/budget*100:.1f}% estimated for periodic DATA channels." if isinstance(budget,int) and budget>0 else "Transport budget is invalid; see diagnostics.")
        lines.append("Trace capabilities: unverified for all listed ports.")
        lines.extend(["", "Diagnostics:"]+[f"{d.severity.upper()} {d.code} · {d.path}: {d.message}" for d in ds] if ds else ["No validation issues."])
        if not any(d.severity=="error" for d in ds):
            out=self._output_path()
            lines.extend(["", "Generated file preview:"])
            rendered=_render(self.doc,self.profile.currentText())
            previous=None
            try:
                selector=json.loads((out/"active_generation.json").read_text(encoding="utf-8"))
                previous=out/"generations"/selector["generation"]
            except (OSError,KeyError,json.JSONDecodeError): pass
            for name,data in sorted(rendered.items()):
                new=data.decode("utf-8").splitlines()
                old=(previous/name).read_text(encoding="utf-8").splitlines() if previous and (previous/name).exists() else []
                lines.append(f"\n--- {name} ({'changed' if old!=new else 'unchanged'}) ---")
                diff=list(difflib.unified_diff(old,new,fromfile="previous",tofile="proposed",lineterm=""))
                lines.extend(diff[:80] if diff else new[:80])
        self.preview.setPlainText("\n".join(lines)); self.tabs.setCurrentWidget(self.tabs.widget(4))
    def _output_path(self):
        configured=Path(self.doc.get("build",{}).get("output_dir","generated/kymo"))
        base=(self.path.parent if self.path else Path.cwd())/configured
        return base/self.profile.currentText()
    def generate_now(self):
        self._pull_ui(); ds=validate(self.doc,self.profile.currentText()); errors=[d for d in ds if d.severity=="error"]
        if errors: self.validate_now(); self.tabs.setCurrentWidget(self.tabs.widget(4)); return
        try:
            out=self._output_path(); gen=generate(self.doc,self.profile.currentText(),out)
            self.status.setText(f"Generated immutable snapshot {gen.name}"); self.preview.setPlainText(f"Generated snapshot:\n{gen}\n\nActive selector: {out/'active_generation.json'}"); self.tabs.setCurrentWidget(self.tabs.widget(4))
        except (ValueError,OSError) as exc: QMessageBox.critical(self,"Generation failed",str(exc))
    def add_channel(self):
        ids={c.get("id") for c in self.doc.get("channels",[])}; cid=next((i for i in range(256) if i not in ids),None)
        if cid is None: return
        self.channel_model.beginInsertRows(QModelIndex(),len(self.channel_model.rows),len(self.channel_model.rows)); self.channel_model.rows.append({"id":cid,"name":f"Channel {cid}","period_ms":20,"x":False,"z":False,"timestamp":False}); self.channel_model.endInsertRows(); self.mark_dirty()
    def duplicate_channel(self):
        row=self.channel_view.currentIndex().row()
        if row<0: return
        data=copy.deepcopy(self.doc["channels"][row]); ids={c["id"] for c in self.doc["channels"]}; data["id"]=next((i for i in range(256) if i not in ids),None)
        if data["id"] is None:return
        self.doc["channels"].append(data); self.channel_model.refresh(); self.mark_dirty()
    def remove_channel(self):
        row=self.channel_view.currentIndex().row()
        if row<0:return
        self.channel_model.beginRemoveRows(QModelIndex(),row,row); del self.doc["channels"][row]; self.channel_model.endRemoveRows(); self.mark_dirty()
    def add_registry(self,group):
        rows=self.doc.setdefault(group,[]); used={item.get("id") for item in rows}; ident=next((i for i in range(1,65536 if group=="symbols" else 4294967295) if i not in used),None)
        if ident is None:return
        model=self.symbol_model if group=="symbols" else self.context_model
        model.beginInsertRows(QModelIndex(),len(rows),len(rows)); rows.append({"id":ident,"name":f"{group[:-1]}_{ident}","kind":"function" if group=="symbols" else "task"}); model.endInsertRows(); self.mark_dirty()
    def remove_registry(self,group):
        view=self.symbol_view if group=="symbols" else self.context_view; model=self.symbol_model if group=="symbols" else self.context_model; row=view.currentIndex().row()
        if row<0:return
        model.beginRemoveRows(QModelIndex(),row,row); del model.rows[row]; model.endRemoveRows(); self.mark_dirty()
    def duplicate_profile(self):
        source=self.profile.currentText()
        if not source:return
        base=source+"_copy"; name=base; index=2
        while name in self.doc["profiles"]: name=f"{base}{index}"; index+=1
        self.doc["profiles"][name]=copy.deepcopy(self.doc["profiles"][source]); self.profile.addItem(name); self.profile.setCurrentText(name); self.doc["active_profile"]=name; self.mark_dirty()
    def closeEvent(self,event):
        if self._ask_save_dirty(): event.accept()
        else: event.ignore()
