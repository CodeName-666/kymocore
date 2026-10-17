/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/**
 * @file KymoCore.h
 * @brief Single entry header for the complete KymoCore library.
 *
 * @details Include this one header instead of choosing between the individual
 * module headers. It always provides the build configuration, the wire
 * protocol and the cyclic Init/Main telemetry runtime. Optional modules are
 * added automatically when their build switch in kymo_build_config.h is
 * enabled:
 *
 * | Build switch            | Module added               | Header               |
 * |-------------------------|----------------------------|----------------------|
 * | (always)                | Telemetry runtime          | kymo_runtime.h       |
 * | (always)                | Wire protocol / codec      | kymo_protocol.h      |
 * | KYMO_ENABLE_CPP=1       | C++ push sender            | kymo.h               |
 * | (always, compile-out)   | Trace instrumentation      | kymo_instrumentation.h |
 * | KYMO_ENABLE_TRACE=1     | Trace recorder + transport | kymo_trace.h, kymo_transport_mux.h |
 *
 * The individual headers remain supported; this header adds no declarations,
 * resources or behavior of its own and is safe in C and C++ translation units.
 * With KYMO_ENABLED=0 every module compiles out exactly as before.
 * @par Usage
 * @code{.cpp}
 * #include <KymoCore.h>
 * static KymoContext context;
 * // Fill a KymoConfig, then: Kymo_Init(&context, &config); Kymo_Main(&context);
 * @endcode
 */
#ifndef KYMO_CORE_H
/**
 * @brief Include guard for KymoCore.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include this header normally; the compiler manages this guard.
 */
#define KYMO_CORE_H

#include "kymo_features.h"
#include "kymo_protocol.h"
#include "kymo_runtime.h"
#include "kymo_instrumentation.h"

#if KYMO_EFFECTIVE_CPP && defined(__cplusplus)
#include "kymo.h"
#endif

#if KYMO_EFFECTIVE_TRACE
#include "kymo_transport_mux.h"
#endif

#endif /* KYMO_CORE_H */
