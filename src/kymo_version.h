/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */

/**
 * @file kymo_version.h
 * @brief Publish the compile-time KymoCore configuration contract version.
 * @details This macro-only header is independent of project configuration.
 * @par Usage
 * Include it before checking KYMO_CONFIG_API_VERSION.
 */
#ifndef KYMO_VERSION_H
#define KYMO_VERSION_H
#ifdef KYMO_CONFIG_API_VERSION
#undef KYMO_CONFIG_API_VERSION
#endif

/** @brief Required version of the KymoCore compile-time configuration contract. */
#define KYMO_CONFIG_API_VERSION 1
#endif /* KYMO_VERSION_H */
