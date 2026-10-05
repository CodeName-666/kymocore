/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/**
 * @file kymo_features.h
 * @brief Load and validate the internal feature configuration.
 * @details Defaults provide the C++11 scalar encoder and cooperative Init/Main runtime.
 * Edit kymo_build_config.h to select features centrally. Optional compiler
 * definitions override those settings. Only 0 and 1 are accepted.
 * C++ translation units require C++11 or newer; C callers retain the C API.
 * Public data layouts and wire constants do not depend on these switches.
 * @par Usage
 * Include this header through any KymoCore API; edit kymo_build_config.h.
 */
#ifndef KYMO_FEATURES_H
/**
 * @brief Include guard for feature configuration.
 * @details Internal header guard; not an application setting.
 * @par Usage
 * Include kymo_features.h normally; the compiler manages this macro.
 */
#define KYMO_FEATURES_H

#include "kymo_build_config.h"

#if defined(__cplusplus) && (__cplusplus < 201103L)
#error "KymoCore requires C++11 or newer"
#endif

#if ((KYMO_ENABLE_RUNTIME != 0) && (KYMO_ENABLE_RUNTIME != 1)) || \
    ((KYMO_ENABLE_X != 0) && (KYMO_ENABLE_X != 1)) || \
    ((KYMO_ENABLE_Z != 0) && (KYMO_ENABLE_Z != 1)) || \
    ((KYMO_ENABLE_TIMESTAMP != 0) && (KYMO_ENABLE_TIMESTAMP != 1)) || \
    ((KYMO_ENABLE_CRC != 0) && (KYMO_ENABLE_CRC != 1)) || \
    ((KYMO_ENABLE_DECODER != 0) && (KYMO_ENABLE_DECODER != 1)) || \
    ((KYMO_ENABLE_CPP != 0) && (KYMO_ENABLE_CPP != 1))
#error "Kymo feature switches must be 0 or 1"
#endif

#endif /* KYMO_FEATURES_H */
