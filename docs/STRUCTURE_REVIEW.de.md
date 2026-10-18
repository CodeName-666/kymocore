# KymoCore 7.0 – Strukturanalyse und Umsetzung

Stand: 18.10.2026, Branch `claude/blissful-bardeen-5njyt8`, Basis `main` @ `b5b2e08` (6.4.0).
Review-Dokument, nicht Teil des exportierten Pakets.

## 1. Ausgangslage (6.4.0)

`main` war der aktuellste Branch und enthielt `develop`, `docs/english` und
`release/6.2.1` vollständig. Der Code war mit rund 4 600 Zeilen klein, der Einstieg
aber schwer, weil viele Entscheidungen vor dem ersten Byte nötig waren:

1. 11 gleichrangige öffentliche Header in einem flachen `src/`, kein Einstiegspunkt.
2. Positionale `KymoConfig` mit 11 Feldern (6 nötig) plus separates `last_sample_ms`-Array,
   das die Config überleben musste.
3. Beispiel-Logik versteckt in einem gemeinsamen Header mit `#ifdef`-Varianten.
4. 12 Build-Schalter ohne Gliederung, Größen-Makros verstreut über Trace- und Mux-Header.
5. Drei Rückgabekonventionen (`KymoStatus`, 1/0, 0/1/2).
6. Drei Uhr- und drei Schreib-Callback-Typen.
7. Uneinheitliche Namen (`Kymo_Init`, `KymoTraceMux_Init`, `kymo_encode_data`, Klasse `Kymo`);
   „Trace“-Mux transportierte auch DATA.
8. Doppelte Byte-/CRC-Helfer in Trace und Mux statt `common/`.

### Plattformunabhängigkeit (geprüft)

`src/` enthielt schon in 6.4.0 **keine** Arduino-, SDK- oder MCU-Abhängigkeit: kein
`Arduino.h`, kein `millis`/`Serial`, keine `#ifdef ARDUINO/ESP/STM`-Zweige. Arduino
kommt nur in `examples/`, in der Doku und im Paket-Manifest `library.properties`
(Arduino-Library-Format) vor. 7.0 behält das bei; in `KymoCore.h`, im Guide und in
`AGENTS.md` ist es als Grundsatz dokumentiert.

## 2. Umgesetzt in 7.0

| Bereich | 6.4.0 | 7.0 |
|---|---|---|
| Einstieg | 11 Header zur Auswahl | **`#include <KymoCore.h>`** |
| Ordner | flach | `telemetry/`, `trace/`, `transport/`, `common/` |
| Konfiguration | 3 Header, Größen verstreut | **`kymo_config.h`**: Version, Profile, alle Schalter, alle Größen, Validierung |
| Profile | – | `KYMO_PROFILE_MINIMAL` / `_XYZ` / `_TRACE` (nur Defaults, alles einzeln überschreibbar) |
| Status | 3 Konventionen | **ein `KymoStatus`** überall, `KYMO_OK` = 0 = Erfolg (+ `KYMO_REJECTED`) |
| Callbacks | je 3 Typen | `KymoClockFn`, `KymoWriteFn`, `KymoBusyFn`, `KymoServiceFn` in `kymo_types.h` |
| Runtime-Config | positional, muss leben, + `last_sample_ms` | Pflichtfelder vorne, **wird kopiert (darf lokal sein)**, Zustand im Kontext (`KYMO_MAX_CHANNELS`) |
| Codec | `kymo_encode_data` … | `KymoProtocol_Encode/Decode/FrameLength/Crc8` |
| C++ | `Kymo` + `kymo_stream.h`, `bool` | `KymoSender` + `KymoStream` in einer Datei, `KymoStatus`, `setClock(fn, user)` |
| Trace | `core_id`/`current_context` Pflicht | optional; nur die Uhr ist Pflicht |
| Mux | `KymoTraceMux`, `KymoTraceDataAdapter`, gemeinsamer `user` | `KymoMux`, `KymoMuxDataSlot`, `transport_user` + `data_user`, `busy` optional, sinnvolle Defaults |
| Helfer | doppelt in Trace/Mux | `common/`: CRC-16, CRC-32, u16/u64 |
| Beispiele | versteckter Shared-Header | eigenständige Sketches: WAS · WANN · WERT · WOHIN |
| Tests | Runtime ohne Test im Repo | `test/native_test.py` inkl. Runtime-, Sender-, Common-, Profil- und Strukturprüfung |

Funktionsumfang und Wire-Format (v6.2 / Wire-Version 1) sind unverändert; die
Trace-Wire-Vektoren sind byte-identisch. Die vollständige Umstiegstabelle steht im
Guide unter „Migration von KymoCore 6.x“.

### Vorher / Nachher

```cpp
// 6.4.0
#include <kymo_runtime.h>
static uint32_t lastSampleMs[1] = {};
static const KymoChannel channels[] = {{20U, 0U, 0U}};
static const KymoConfig config = {
    channels, lastSampleMs, readClockMs, nullptr, readSample, nullptr,
    writeBytes, nullptr, nullptr, nullptr, 1U
};

// 7.0
#include <KymoCore.h>
static const KymoChannel channels[] = {{20U, 0U, 0U}};   // period_ms, id, flags
static KymoContext context;
void setup() {
    KymoConfig config = {};
    config.channels      = channels;
    config.channel_count = KYMO_COUNT_OF(channels);
    config.clock_ms      = readClockMs;
    config.sample        = readSample;
    config.write         = writeBytes;
    Kymo_Init(&context, &config);
}
```

## 3. Prüfung

| Prüfung | Ergebnis |
|---|---|
| `python test/native_test.py` (Common, Golden-Vectors mit/ohne CRC, Runtime ×4, Sender, Einstiegsheader in C99 für alle Profile, Beispiel, Trace-Suite, Compile-out inkl. Profile/Größen, Single-Return-Check), `-pedantic` | PASS |
| Configurator-pytest-Suite | 20 passed, 2 skipped |
| PlatformIO: beide Arduino-Beispiele für `uno`, `nano`, `mega` | 6/6 SUCCESS |
| `python tools/test_package.py` (Export, Archiv, README-Beispiel Uno + ESP32 aus dem Archiv) | PASS |

Nicht ausführbar in dieser Umgebung: `../KymoProbe/tools/test_native.py --app ../KymoStudio`
und Doxygen über das KymoProbe-`Doxyfile`. **KymoProbe muss für 7.0 angepasst werden**
(neue Include-Pfade und Namen, `Doxyfile`-Inputs auf `src/**`). Solange KymoProbe
6.4.0 pinnt, ist es nicht betroffen.

## 4. Konfigurator 0.2

| Thema | Umsetzung |
|---|---|
| 7.0-Anpassung | Projektschema 2 (Migration von Schema 1, auch automatisch beim Öffnen in der GUI), Konfigurations-API 2, Generator für die 7.0-API, `KYMO_MAX_CHANNELS` aus der Kanaltabelle, `core_id`/`current_context` optional |
| Ziele statt Schalter | Presets „Live-Werte“, „XY/XYZ mit Zeit“, „Execution-Tracing“ entsprechen `KYMO_PROFILE_*`; manuelle Änderung → „custom“ |
| Geführter Modus | Startfrage nach dem Ziel, danach nur relevante Schritte (Tracing nur bei Trace-Ziel), fortgeschrittene Felder ausgeblendet, „Weiter“ gesperrt bei Fehlern des Schritts, Abschluss mit Prüfen & Generieren |
| Profi-Modus | Navigation mit Live-Zusammenfassung und Status (✓/⚠/✖) je Thema, Fehler direkt am Feld, Prüfseite mit effektiven Makros, Dateivorschau und Sprung von Problemen zum Feld |
| Verständlichkeit | Jede Einstellung mit Klartext-Erklärung, betroffenem Makro/Signatur und Begründung, warum sie gerade gesperrt ist; Kanaltabelle mit Rate, Bytes/Frame und Leitungsauslastung |
| Erweiterbarkeit | Beide Modi werden aus einem deklarativen Katalog (`catalog.py`) erzeugt; neue Features (z. B. Rückkanal) über `register_section`, `register_widget`, `register_validator`, `register_renderer` – siehe `tools/configurator/EXTENDING.md` |
| Tests | 39 Tests (vorher 22), u. a. alle Katalogfelder gegen den Validator, Presets, Migration, Erweiterungs-Rundlauf, GUI-Workflows beider Modi offscreen |

Die UI-Texte sind derzeit Englisch und liegen gesammelt im Katalog, sodass eine
deutsche Übersetzung später an einer Stelle ergänzt werden kann.
