/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/**
 * @file kymo_build_config.h
 * @brief Resolve project settings and defaults for the complete KymoCore build.
 * @details Precedence is compiler definitions, optional project header, then library
 * defaults. All settings are preprocessor-only and create no firmware resources.
 * @par Usage
 * Set KYMO_USER_CONFIG_HEADER to a quoted header path or pass settings with -D.
 */
#ifndef KYMO_BUILD_CONFIG_H
#define KYMO_BUILD_CONFIG_H

#if defined(KYMO_USER_CONFIG_HEADER)
#include KYMO_USER_CONFIG_HEADER
#endif

/** @brief Global KymoCore gate; defaults to enabled. @par Usage Define 0 for compile-out. */
#ifndef KYMO_ENABLED
#define KYMO_ENABLED 1
#endif
/** @brief Enable cooperative Init/Main scheduling; defaults to enabled. */
#ifndef KYMO_ENABLE_RUNTIME
#define KYMO_ENABLE_RUNTIME 1
#endif
/** @brief Include X coordinates in the wire payload; defaults to disabled. */
#ifndef KYMO_ENABLE_X
#define KYMO_ENABLE_X 0
#endif
/** @brief Include Z coordinates in the wire payload; defaults to disabled. */
#ifndef KYMO_ENABLE_Z
#define KYMO_ENABLE_Z 0
#endif
/** @brief Include millisecond timestamps in DATA frames; defaults to disabled. */
#ifndef KYMO_ENABLE_TIMESTAMP
#define KYMO_ENABLE_TIMESTAMP 0
#endif
/** @brief Protect outgoing DATA frames with CRC; defaults to disabled. */
#ifndef KYMO_ENABLE_CRC
#define KYMO_ENABLE_CRC 0
#endif
/** @brief Compile complete-frame MCU decoding; defaults to disabled. */
#ifndef KYMO_ENABLE_DECODER
#define KYMO_ENABLE_DECODER 0
#endif
/** @brief Compile the optional C++ push sender; defaults to disabled. */
#ifndef KYMO_ENABLE_CPP
#define KYMO_ENABLE_CPP 0
#endif
/** @brief Compile the trace recorder and instrumentation API; defaults to disabled. */
#ifndef KYMO_ENABLE_TRACE
#define KYMO_ENABLE_TRACE 0
#endif
/** @brief Compile ISR tracing; requires trace and defaults to disabled. */
#ifndef KYMO_ENABLE_TRACE_ISR
#define KYMO_ENABLE_TRACE_ISR 0
#endif
/** @brief Compile scheduler tracing; requires ISR tracing and defaults to disabled. */
#ifndef KYMO_ENABLE_TRACE_SCHEDULER
#define KYMO_ENABLE_TRACE_SCHEDULER 0
#endif
/** @brief Compile flow correlation tracing; defaults to disabled. */
#ifndef KYMO_ENABLE_TRACE_FLOW
#define KYMO_ENABLE_TRACE_FLOW 0
#endif

#endif /* KYMO_BUILD_CONFIG_H */
