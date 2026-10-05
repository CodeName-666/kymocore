# Changelog

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
