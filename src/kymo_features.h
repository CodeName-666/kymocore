/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */

/**
 * @file kymo_features.h
 * @brief Load settings and derive validated effective KymoCore features.
 * @details Requested values remain unchanged. KYMO_ENABLED gates every effective
 * feature. Boolean settings accept only literal 0 or 1.
 * @par Usage
 * Include this header through any KymoCore API; compiler overrides win.
 */
#ifndef KYMO_FEATURES_H
#define KYMO_FEATURES_H

#include "kymo_version.h"
#include "kymo_build_config.h"

#if defined(__cplusplus) && (__cplusplus < 201103L)
#error "KymoCore requires C++11 or newer"
#endif

/* Token validation rejects empty values, unknown identifiers and non-boolean tokens. */

/** @brief Internal token-paste step for checking boolean settings. */
#define KYMO_BOOL_VALID_TOKEN_I(value_) KYMO_BOOL_VALID_##value_

/** @brief Internal validation wrapper for a literal boolean setting. */
#define KYMO_BOOL_VALID_TOKEN(value_) KYMO_BOOL_VALID_TOKEN_I(value_)

/** @brief Internal marker indicating literal zero is accepted. */
#define KYMO_BOOL_VALID_0 1

/** @brief Internal marker indicating literal one is accepted. */
#define KYMO_BOOL_VALID_1 1
#if !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLED) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_RUNTIME) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_X) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_Z) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_TIMESTAMP) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_CRC) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_DECODER) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_CPP) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_TRACE) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_TRACE_ISR) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_TRACE_SCHEDULER) || \
    !KYMO_BOOL_VALID_TOKEN(KYMO_ENABLE_TRACE_FLOW)
#error "Kymo feature switches must be 0 or 1 (literal tokens only)"
#endif

#undef KYMO_BOOL_VALID_TOKEN
#undef KYMO_BOOL_VALID_TOKEN_I
#undef KYMO_BOOL_VALID_0
#undef KYMO_BOOL_VALID_1

/** @brief Effective cooperative runtime gate. */
#define KYMO_EFFECTIVE_RUNTIME (KYMO_ENABLED && KYMO_ENABLE_RUNTIME)

/** @brief Effective X-coordinate gate. */
#define KYMO_EFFECTIVE_X (KYMO_ENABLED && KYMO_ENABLE_X)

/** @brief Effective Z-coordinate gate. */
#define KYMO_EFFECTIVE_Z (KYMO_ENABLED && KYMO_ENABLE_Z)

/** @brief Effective DATA timestamp gate. */
#define KYMO_EFFECTIVE_TIMESTAMP (KYMO_ENABLED && KYMO_ENABLE_TIMESTAMP)

/** @brief Effective encoder CRC gate. */
#define KYMO_EFFECTIVE_CRC (KYMO_ENABLED && KYMO_ENABLE_CRC)

/** @brief Effective MCU decoder gate. */
#define KYMO_EFFECTIVE_DECODER (KYMO_ENABLED && KYMO_ENABLE_DECODER)

/** @brief Effective optional C++ sender gate. */
#define KYMO_EFFECTIVE_CPP (KYMO_ENABLED && KYMO_ENABLE_CPP)

/** @brief Effective trace recorder gate. */
#define KYMO_EFFECTIVE_TRACE (KYMO_ENABLED && KYMO_ENABLE_TRACE)

/** @brief Effective trace ISR gate. */
#define KYMO_EFFECTIVE_TRACE_ISR (KYMO_EFFECTIVE_TRACE && KYMO_ENABLE_TRACE_ISR)

/** @brief Effective scheduler trace gate. */
#define KYMO_EFFECTIVE_TRACE_SCHEDULER \
    (KYMO_EFFECTIVE_TRACE_ISR && KYMO_ENABLE_TRACE_SCHEDULER)

/** @brief Effective flow trace gate. */
#define KYMO_EFFECTIVE_TRACE_FLOW (KYMO_EFFECTIVE_TRACE && KYMO_ENABLE_TRACE_FLOW)

#if KYMO_ENABLED && KYMO_ENABLE_TRACE_SCHEDULER && !KYMO_ENABLE_TRACE_ISR
#error "KYMO_ENABLE_TRACE_SCHEDULER requires KYMO_ENABLE_TRACE_ISR=1"
#endif

#if KYMO_ENABLED && KYMO_ENABLE_TRACE_ISR && !KYMO_ENABLE_TRACE
#error "KYMO_ENABLE_TRACE_ISR requires KYMO_ENABLE_TRACE=1"
#endif

#if KYMO_ENABLED && KYMO_ENABLE_TRACE_FLOW && !KYMO_ENABLE_TRACE
#error "KYMO_ENABLE_TRACE_FLOW requires KYMO_ENABLE_TRACE=1"
#endif

#endif /* KYMO_FEATURES_H */
