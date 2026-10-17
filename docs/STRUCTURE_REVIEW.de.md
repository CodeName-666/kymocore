# KymoCore – Strukturanalyse und Vereinfachung

Stand: 17.10.2026, Basis `main` @ `b5b2e08` (KymoCore 6.4.0).
Dieses Dokument ist ein Review-Dokument für den Branch
`claude/blissful-bardeen-5njyt8` und nicht Teil des exportierten Pakets.

## 1. Branch-Stand

| Branch | Letzter Commit | Verhältnis zu `main` |
|---|---|---|
| `main` | 17.10.2026 – *Require the contribution grant …* | **aktuellster Stand** |
| `develop` | 15.10.2026 – Release 6.4.0 | 4 Commits hinter `main`, nichts Eigenes |
| `docs/english` | 05.10.2026 | 14 hinter `main`, nichts Eigenes |
| `release/6.2.1` | 05.10.2026 | 16 hinter `main`, nichts Eigenes |

`main` enthält alle anderen Branches vollständig. Dieser Branch setzt auf `main` auf.

## 2. Ist-Analyse: Warum der Einstieg schwer fällt

Die Codebasis selbst ist nicht groß (≈ 4 600 Zeilen in `src/`), aber ein neuer
Nutzer muss sehr viele **Entscheidungen** treffen, bevor das erste Byte fließt.

### 2.1 Modulkarte

| Bereich | Header | Wozu | Braucht der Einsteiger das? |
|---|---|---|---|
| Konfiguration | `kymo_build_config.h`, `kymo_features.h`, `kymo_version.h` | 12 Schalter, Validierung, `KYMO_EFFECTIVE_*` | indirekt |
| **Telemetrie (Pull)** | `kymo_runtime.h` | `Kymo_Init`/`Kymo_Main`, zyklisches Abtasten | **ja – der Normalfall** |
| Telemetrie (Push, C++) | `kymo.h`, `kymo_stream.h` | `Kymo`-Klasse, `send/send2D/send3D` | selten |
| Protokoll | `kymo_protocol.h` | Encoder/Decoder, Frame-Konstanten | selten direkt |
| Tracing | `kymo_trace.h`, `kymo_trace_protocol.h` | Recorder, Ereignisse | nur für Trace |
| Instrumentierung | `kymo_instrumentation.h` | `KYMO_TRACE_*`-Makros (Compile-out) | nur für Trace |
| Transport-Mux | `kymo_transport_mux.h` | Trace + DATA auf einer Leitung | nur für Trace |
| Hilfsfunktionen | `common/embedded_*.h` | Bits, Bytes, CRC | nein |

→ **11 öffentliche Header + 3 Hilfsheader, ohne gemeinsamen Einstiegspunkt.**
Der Nutzer musste bisher selbst herausfinden, dass `kymo_runtime.h` der richtige ist.

### 2.2 Die größten Komplexitätstreiber (nach Wirkung sortiert)

1. **Kein zentraler Header.** Wer die Bibliothek einbindet, sieht 11 gleichrangige
   Dateien in einem flachen `src/`-Ordner. Telemetrie, Tracing, Protokoll und
   Konfiguration liegen ohne Gliederung nebeneinander.
2. **Positionale `KymoConfig`-Initialisierung.** Alle Beispiele nutzten
   ```c
   { channels, last, clock, nullptr, sample, nullptr, write, nullptr, nullptr, nullptr, 1U }
   ```
   11 Felder, davon nur 6 nötig; die Bedeutung der `nullptr`-Positionen ist
   nicht ablesbar, und `channel_count` musste von Hand gepflegt werden.
3. **Versteckte Beispiel-Logik.** Die Arduino-Sketches bestanden aus
   `setup(){Kymo_Init(..., &example_config);}` – die eigentliche Arbeit
   (Kanäle, Callbacks, Konfiguration) steckte in einem gemeinsamen Header
   `examples/common/arduino_serial_config.h` mit `#ifdef`-Varianten.
4. **12 Build-Schalter ohne Gliederung.** Payload-Optionen (X, Z, Zeitstempel,
   CRC), API-Auswahl (Runtime, C++) und Tracing standen gleichrangig untereinander.
5. **Drei Rückgabekonventionen.**
   `KymoStatus` (0 = OK, 1…6 = Zustände), Trace-API (1 = Erfolg, 0 = Fehler),
   `KymoTraceMux_Service` (0 = idle, 1 = gesendet, 2 = Fehler).
6. **Drei Uhr- und drei Schreib-Callback-Typen.**
   `KymoClockFn(void*)→uint32 ms`, `GetMillisecondCallback()→uint32` (ohne Kontext),
   Trace `clock_ticks(void*)→uint64`; `KymoWriteFn(…, uint8_t)`,
   `KymoStream::write(…, size_t)`, `KymoTraceWriteFrame(…, size_t)`.
7. **Uneinheitliche Namen.** `Kymo_Init`, `KymoTrace_Init`, `KymoTraceMux_Init`,
   `kymo_encode_data`, Klasse `Kymo`. Der Mux heißt „Trace“-Mux, transportiert
   aber auch DATA-Frames; der C++-Sender wird im Mux-Header als „legacy“ bezeichnet,
   in der Doku aber als unterstützte API geführt.
8. **Separates `last_sample_ms`-Array**, das parallel zur Kanaltabelle angelegt und
   in der Größe synchron gehalten werden muss.

## 3. In diesem Branch umgesetzt (nicht brechend)

Alle Änderungen sind **rückwärtskompatibel**: keine Funktion entfernt, kein
Struct-Layout, keine Wire-Daten und kein Default-Schalter geändert.
Bestehender Anwendercode kompiliert unverändert.

| # | Änderung | Wirkung für den Nutzer |
|---|---|---|
| 1 | **`src/KymoCore.h`** – ein Einstiegs-Header, bindet automatisch alle aktivierten Module ein (Runtime, Protokoll, Instrumentierung; mit `KYMO_ENABLE_CPP` die `Kymo`-Klasse, mit `KYMO_ENABLE_TRACE` Recorder + Mux). C- und C++-tauglich. | „Ein `#include <KymoCore.h>` – fertig.“ Kein Suchen mehr nach dem richtigen Header. |
| 2 | `library.properties`: `includes=KymoCore.h`, `library.json`: Header gelistet | Arduino-IDE fügt beim „Bibliothek einbinden“ den richtigen Header ein. |
| 3 | **`KYMO_COUNT_OF(array)`** in `kymo_runtime.h` | Kanalanzahl und `last_sample_ms`-Größe werden aus der Kanaltabelle abgeleitet – kein Auseinanderlaufen mehr. |
| 4 | **Konfiguration per Feldname** in allen Beispielen, README und Guide | `config.sample = readSample;` statt Position 5 in einer Liste. Optionale Felder bleiben einfach weg. |
| 5 | **Arduino-Beispiele eigenständig**, gegliedert in die vier Schritte **WAS** (Kanaltabelle) · **WANN** (Uhr) · **WERT** (Sample) · **WOHIN** (Write); `examples/common/arduino_serial_config.h` entfernt | Ein Sketch = eine Datei, von oben nach unten lesbar, direkt kopierbar. |
| 6 | **`kymo_build_config.h` gegliedert** in Master · 1. Telemetrie-API · 2. Telemetrie-Payload · 3. Tracing, mit Übersichtstabelle | Klar, welche Schalter für den Alltag relevant sind (meist nur Gruppe 2). |
| 7 | CHANGELOG-Eintrag „Unreleased“ | – |

### Vorher / Nachher (Arduino-Kern)

Vorher:
```cpp
#include <kymo_runtime.h>
static uint32_t lastSampleMs[1] = {};
static const KymoChannel channels[] = {{20U, 0U, 0U}};
static const KymoConfig config = {
    channels, lastSampleMs, readClockMs, nullptr, readSample, nullptr,
    writeBytes, nullptr, nullptr, nullptr, 1U
};
```

Nachher:
```cpp
#include <KymoCore.h>
static const KymoChannel channels[] = {{20U, 0U, 0U}};   // period_ms, id, flags
static uint32_t lastSampleMs[KYMO_COUNT_OF(channels)] = {};
static KymoConfig config = {};

void setup() {
    config.channels       = channels;                     // was
    config.channel_count  = KYMO_COUNT_OF(channels);
    config.last_sample_ms = lastSampleMs;
    config.clock_ms       = readClockMs;                  // wann
    config.sample         = readSample;                   // welche Werte
    config.write          = writeBytes;                   // wohin
    Kymo_Init(&context, &config);
}
```

## 4. Vorschlag Phase 2 (brechend → KymoCore 7.0)

Diese Punkte ändern API, Dateipfade oder Struct-Layouts und betreffen
KymoProbe, KymoStudio-Generator (Configurator) und bestehende Anwender.
Sie sind **bewusst nicht umgesetzt** und sollten erst nach Abstimmung angegangen werden.

### 4.1 Ordnerstruktur nach Aufgaben

```
src/
  KymoCore.h               einziger öffentlicher Einstieg
  kymo_config.h            build_config + features + version zusammengeführt
  telemetry/               kymo_runtime.*, kymo_protocol.*, kymo_sender.* (kymo.h + kymo_stream.h)
  trace/                   kymo_trace.*, kymo_trace_protocol.*, kymo_instrumentation.h
  transport/               kymo_mux.* (heute kymo_transport_mux.*)
  common/                  unverändert
```
Arduino (Library-Format 1.5) und PlatformIO kompilieren `src/` rekursiv; für
eine Übergangs-Version blieben flache Weiterleitungs-Header bestehen.
Anpassen müssten sich: `tools/test_package.py`, `test/*.py`, der Configurator
(`generation.py`, `port_template.py`) und das KymoProbe-`Doxyfile`.

### 4.2 Einheitliche Konventionen

| Thema | Heute | Vorschlag |
|---|---|---|
| Rückgabewerte | 3 Konventionen | überall `KymoStatus` (0 = `KYMO_OK`), Trace/Mux bekommen eigene Codes im selben Typ |
| Namensschema | `Kymo_`, `KymoTrace_`, `KymoTraceMux_`, `kymo_encode_data` | `Kymo<Modul>_<Verb>`: `Kymo_Init`, `KymoTrace_Init`, `KymoMux_Init`, `KymoProtocol_Encode` |
| Mux | `KymoTraceMux`, `KymoTraceDataAdapter` („legacy“) | `KymoMux`, `KymoMuxDataSlot`; „legacy“ aus der Doku entfernen |
| Uhr-Callbacks | 3 Typen | ein Typ mit `void *user` für Telemetrie und C++-Sender (`GetMillisecondCallback` entfällt) |
| C++-Sender | `kymo.h` + `kymo_stream.h` getrennt | eine Datei `kymo_sender.h` |
| Kanalzustand | separates `last_sample_ms`-Array | Zustand pro Kanal im Kontext über eine Speicher-Struktur `KymoChannelState`, Größe per `KYMO_COUNT_OF` |

### 4.3 Optionale Komfort-Profile

Vorschlag eines einzelnen, dokumentierten Schalters `KYMO_PROFILE`
(`MINIMAL`, `XYZ_TIME`, `TRACE`), der nur **Defaults** für die Einzelschalter
setzt. Einzelschalter und Compiler-Definitionen würden weiterhin gewinnen.
Spart Einsteigern das Wissen um die Abhängigkeiten `TRACE → TRACE_ISR → TRACE_SCHEDULER`.

## 5. Prüfung

Ausgeführt in diesem Branch:

| Prüfung | Ergebnis |
|---|---|
| `python test/compile_out_test.py` | PASS |
| `python test/trace_core_test.py` | PASS |
| `KymoCore.h` in C99 und C++11, `-Wall -Wextra -Werror`, 6 Feature-Kombinationen (Default, `KYMO_ENABLED=0`, CPP, Trace, alles an, Runtime aus) | PASS |
| `examples/basic` gebaut und ausgeführt | PASS (Exit 0) |
| Beide Arduino-Sketches gegen einen Arduino-Stub gebaut, `setup()` + `Kymo_Main` ausgeführt | PASS (Init `KYMO_OK`) |
| Code-Blöcke aus README/README.de/GUIDE/GUIDE.de extrahiert, gebaut und ausgeführt | PASS |
| Configurator-pytest-Suite (`tools/configurator`) | 20 passed, 2 skipped |
| PlatformIO: beide Arduino-Beispiele für `uno`, `nano`, `mega` | 6/6 SUCCESS |
| `python tools/test_package.py` (Export, Archivprüfung, README-Beispiel für Uno und ESP32 aus dem Archiv) | PASS |

Nicht ausführbar in dieser Umgebung (bitte lokal nachholen):
`python ../KymoProbe/tools/test_native.py --app ../KymoStudio` inklusive
Single-Return-Strukturcheck (KymoProbe/KymoStudio nicht ausgecheckt) sowie
Doxygen über das KymoProbe-`Doxyfile` (dort `KymoCore.h` als Input ergänzen).
