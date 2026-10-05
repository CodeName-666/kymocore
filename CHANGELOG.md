# Changelog

## 6.2.2

- Documentation is now English by default with a German version in parallel:
  `README.md`/`README.de.md`, `docs/GUIDE.md`/`docs/GUIDE.de.md`,
  `COMMERCIAL.md`/`COMMERCIAL.de.md`, `CONTRIBUTING.md`/`CONTRIBUTING.de.md`.
  A language switch at the top of each file toggles between them.
- `docs/ANLEITUNG.md` was renamed to `docs/GUIDE.de.md`.
- No change to code, API or protocol.

## 6.2.1

- README als Übersicht mit Logo, Screenshot, Einsatzgebieten und Schnellstart.
- Ausführliche Anleitung und API-Referenz in `docs/ANLEITUNG.md` (im Paket enthalten).
- Diagramme mit den Namen KymoCore/KymoStudio neu erzeugt.
- Keine Änderung an Code, API oder Protokoll.

## 6.2.0

- **Lizenzwechsel:** KymoCore ist ab dieser Version doppelt lizenziert:
  GPLv3 oder kommerzielle Lizenz (`GPL-3.0-only OR LicenseRef-KymoCore-Commercial`).
  Siehe COMMERCIAL.md. Versionen bis einschließlich 6.1.0 bleiben MIT-lizenziert.
- Alle Quelldateien tragen eine SPDX-Kennung und den Copyright-Vermerk.
- CONTRIBUTING.md beschreibt die Rechteeinräumung für Beiträge.
- Keine Änderung an API, Protokoll oder Wire-Format gegenüber 6.1.0.

## 6.1.0 — release candidate, not a publication record

- Portable C++11 implementation with C linkage for C99 callers; compile and
  link the implementation with the C++ toolchain.
- Cooperative Init/Main scheduling, static buffers, partial writes and explicit
  asynchronous transport ownership.
- Minimal scalar encoder/runtime by default. Independent switches for X, Z,
  timestamps, CRC, MCU decoding and the C++ push API.
- Protocol v6.1 keeps wire version 1 and 9–21 byte frames. NO_CRC is the default;
  older v6.0 receivers require outgoing CRC to be enabled.
- Arduino/STM32 adapters live in application examples outside the portable core.
  Direct `Kymo(Serial)` / `begin(Serial)` overloads are removed; use a
  `KymoStream` adapter. Rebuild all consumers after migration.
- Publication package includes the MIT license, a portable example, illustrated
  documentation, KymoStudio links and maintainer publication instructions.

The version was already present in the manifests before publication preparation.
Verify availability in the intended PlatformIO account before publishing it.
