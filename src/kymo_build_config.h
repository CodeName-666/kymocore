/**
 * @file kymo_build_config.h
 * @brief Editable internal feature configuration for the complete KymoCore build.
 * @details Defaults provide the C scalar encoder and cooperative Init/Main runtime.
 * Change the KYMO_ENABLE_* values below to 0 (off) or 1 (on), then rebuild
 * the entire library/application. Every translation unit includes these settings.
 * Compiler -D definitions take precedence through the individual ifndef guards.
 * Optional code is excluded at preprocessing time; no runtime configuration is used.
 * Public data layouts and wire constants do not depend on these switches.
 * @par Usage
 * Set KYMO_ENABLE_TIMESTAMP below to 1 to enable wire timestamps.
 */
#ifndef KYMO_BUILD_CONFIG_H
/**
 * @brief Include guard for feature configuration.
 * @details Internal header guard; edit only the feature switches below.
 * @par Usage
 * Include kymo_build_config.h normally; the compiler manages this macro.
 */
#define KYMO_BUILD_CONFIG_H

#ifndef KYMO_ENABLE_RUNTIME
/**
 * @brief Enable the cooperative Init/Main runtime; defaults to 1.
 * @details Set to 0 for codec-only or application-scheduled C++ sending.
 * Disabled runtime entrypoints are neither declared nor compiled.
 * @par Usage
 * Set KYMO_ENABLE_RUNTIME below to 0 when scheduling samples externally.
 */
#define KYMO_ENABLE_RUNTIME 1
#endif

#ifndef KYMO_ENABLE_X
/**
 * @brief Enable optional X coordinates; defaults to 0.
 * @details Disabled X flags are rejected by configuration, encoder and decoder.
 * @par Usage
 * Set KYMO_ENABLE_X below to 1 for XY or XYZ data.
 */
#define KYMO_ENABLE_X 0
#endif

#ifndef KYMO_ENABLE_Z
/**
 * @brief Enable optional Z coordinates; defaults to 0.
 * @details Independent of X; disabled Z flags are rejected rather than discarded.
 * @par Usage
 * Set KYMO_ENABLE_Z below to 1 for YZ or XYZ data.
 */
#define KYMO_ENABLE_Z 0
#endif

#ifndef KYMO_ENABLE_TIMESTAMP
/**
 * @brief Enable wire timestamps; defaults to 0.
 * @details The runtime still uses its clock for scheduling when timestamps are off.
 * Disabled timestamp flags are rejected. C++ timestamp requests fail when disabled.
 * @par Usage
 * Set KYMO_ENABLE_TIMESTAMP below to 1 to encode relative milliseconds.
 */
#define KYMO_ENABLE_TIMESTAMP 0
#endif

#ifndef KYMO_ENABLE_CRC
/**
 * @brief Enable encoder CRC calculation; defaults to 0.
 * @details Zero emits NO_CRC with a zero trailer. One emits protected legacy frames.
 * If the decoder is enabled, it always validates protected input regardless of
 * this sender setting; CRC support is then compiled for receive compatibility.
 * @par Usage
 * Set KYMO_ENABLE_CRC below to 1 to protect outgoing frames.
 */
#define KYMO_ENABLE_CRC 0
#endif

#ifndef KYMO_ENABLE_DECODER
/**
 * @brief Enable the MCU complete-frame decoder; defaults to 0.
 * @details Most telemetry MCUs only send. Enabling decoding also includes CRC
 * verification for protected frames. X/Z/time switches limit supported layouts.
 * The desktop KymoStudio decoder is independent of these firmware switches.
 * @par Usage
 * Set KYMO_ENABLE_DECODER below to 1 when receiving binary telemetry on MCU.
 */
#define KYMO_ENABLE_DECODER 0
#endif

#ifndef KYMO_ENABLE_CPP
/**
 * @brief Enable the optional C++ push sender; defaults to 0.
 * @details The C++11 codec/runtime need no sender wrapper. A disabled wrapper has no
 * class declaration or implementation; enable it before including kymo.h.
 * @par Usage
 * Set KYMO_ENABLE_CPP below to 1 for Kymo and KymoStream adapters.
 */
#define KYMO_ENABLE_CPP 0
#endif

#endif /* KYMO_BUILD_CONFIG_H */
