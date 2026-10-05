# KymoCore für PlatformIO veröffentlichen

## Paket und Voraussetzungen

Veröffentlichungseinheit ist dieses Repository **kymocore**. In der Firmware
[kymoprobe](https://github.com/CodeName-666/kymoprobe) ist es als Git-Submodul `lib/KymoCore` eingebunden.
`library.json` liegt im Paketwurzelverzeichnis. Quellen, Lizenz, README,
Protokoll, Bilder und portables Beispiel werden explizit exportiert.
Die bestehenden Hardwarebeispiele bleiben im ECU-Repository.

PlatformIO benötigt ein gültiges Manifest. `headers` und `examples` sind
zusätzliche optionale Metadaten. Lizenzdatei, Changelog, Bilder und ein
Installationsbeispiel vervollständigen das nutzbare Paket; nicht alles davon
ist eine technische Pflicht des Registry-Validators.

Die Kurzbeschreibung im Manifest darf maximal 255 Zeichen haben. Die
ausführliche Registry-Dokumentation steht in README.md. PNG-Bilder liegen im
Archiv unter `docs/images` und sind relativ verlinkt; sie benötigen weder
Mermaid-Unterstützung noch einen externen Bildhost. Die tatsächliche Darstellung
im Registry-Frontend nach Veröffentlichung zusätzlich prüfen.

## Lokale Prüfung

Im Firmware-Repository kymoprobe (Submodul initialisiert), Python 3.10+, GCC/G++ und PlatformIO auf PATH:

```sh
python tools/test_native.py --app ../KymoStudio
python tools/test_package.py
pio run -e nodemcu-32s
pio run -d examples/arduino/multi_sensor_example -e uno
pio run -d examples/esp32/wifi_mqtt_example -e esp32dev
pio run -d examples/stm32/uart_example -e nucleo_f401re
```

`test_package.py` validiert das Manifest mit `pio pkg pack`, kontrolliert
Archivinhalt und lokale Dokumentationslinks, kompiliert und startet das
portable Beispiel und baut den vollständigen README-Sketch auf Uno und ESP32.
Diese Verbraucher installieren ausschließlich das Archiv, keine Repo-Symlinks.
Der schnelle Modus `--skip-firmware` lässt nur die beiden Boardbuilds aus.
Buildartefakte und getrennte Verbraucher liegen unter `.pio/package/`.

Ergebnis ist `KymoCore-6.2.0.tar.gz` mit separater SHA256-Datei. Das Archiv
enthält den aktuellen Arbeitsstand einschließlich neuer, noch uncommitteter
Dateien. Nach Änderungen neu packen und prüfen. CI prüft das Paket ebenfalls;
ein lokal erfolgreicher Lauf beweist keinen erfolgreichen Remote-CI-Lauf.

Die Dokumentationsbilder können mit Pillow neu erzeugt werden:
`python tools/render_library_diagrams.py`. Pillow ist nur ein Dokuwerkzeug,
keine Abhängigkeit der Library oder des Paketchecks.

## Vor dem Upload noch festlegen beziehungsweise prüfen

1. PlatformIO-Account oder Organisation und Schreibberechtigung bestimmen.
   GitHub-Owner und PlatformIO-Owner müssen nicht identisch sein.
2. Mit `pio pkg show PIO_OWNER/KymoCore@6.2.0` prüfen, ob die Version schon
   existiert. Netzwerk- oder Authentifizierungsfehler beweisen keine freie Version.
   Ein einmal veröffentlichtes Name-/Versionspaar lässt sich nicht erneut
   verwenden, auch nicht nach dem Löschen. Bei Bedarf beide Manifeste,
   Installationsbeispiele und Changelog auf eine neue Version setzen.
3. Lizenz ist `GPL-3.0-only OR LicenseRef-KymoCore-Commercial` (siehe
   COMMERCIAL.md). Im Manifest steht nur `GPL-3.0-only`, weil der
   PlatformIO-Validator ausschließlich Kennungen der SPDX-Liste akzeptiert. Die
   kommerzielle Option ist in COMMERCIAL.md, README und den Quelldateien angegeben. Vor Veröffentlichung sicherstellen, dass alle Quellen eine
   SPDX-Zeile tragen und für jeden fremden Beitrag die Rechteeinräumung aus
   CONTRIBUTING.md vorliegt.
4. Änderungen reviewen, gezielt committen und in diesem Repository pushen.
   Einen Release-Tag `vX.Y.Z` anlegen; die Git-Installation über `lib_deps`
   verwendet ihn. Danach in kymoprobe das Submodul `lib/KymoCore` auf den Tag
   setzen und committen.
5. Den kompatiblen KymoStudio-Stand öffentlich bereitstellen beziehungsweise
   dessen NO_CRC-Unterstützung prüfen. Der lokale Integrationstest verwendet
   `../KymoStudio`; er prüft nicht automatisch den öffentlich erreichbaren Stand.
6. `PIO_OWNER` in der README durch den bestätigten Namespace ersetzen, erneut
   packen und testen. Prüfergebnisse und unterstützte Boards dokumentieren.

## Upload als eigener Schritt

Diese Vorbereitung veröffentlicht nichts. Nach Freigabe am eigenen Account
anmelden und das geprüfte Archiv veröffentlichen:

```sh
pio account login
pio pkg publish .pio/package/KymoCore-6.2.0.tar.gz --owner PIO_OWNER --type library
```

Danach die Registry-Seite und Bilder öffnen, `pio pkg show` prüfen und in einem
frischen Projekt `PIO_OWNER/KymoCore @ 6.2.0` installieren. Das validiert die
Registry-Auslieferung zusätzlich zum bereits geprüften lokalen Archiv.

Hardwaretests sind gesondert nötig: Flashen, Live-Übertragung, DMA/IRQ, USB und
Broker-Verbindungen sind keine Folgen eines erfolgreichen Compile-Tests.

## Offizielle Referenzen

- [Library-Struktur und Veröffentlichung](https://docs.platformio.org/en/latest/librarymanager/creating.html)
- [Manifestfelder und Validierung](https://docs.platformio.org/en/latest/manifests/library-json/index.html)
- [Kurzbeschreibung bis 255 Zeichen](https://docs.platformio.org/en/latest/manifests/library-json/fields/description.html)
- [Paket packen](https://docs.platformio.org/en/latest/core/userguide/pkg/cmd_pack.html)
- [Veröffentlichen und unveränderliche Versionen](https://docs.platformio.org/en/latest/core/userguide/pkg/cmd_publish.html)
- [KymoStudio](https://github.com/CodeName-666/kymostudio)
