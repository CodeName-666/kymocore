/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/**
 * @file kymo_param.h
 * @defgroup kymo_param Back channel: host writes to parameters
 * @{
 * @brief Let the host change registered application parameters at run time.
 *
 * @details The host sends an ordinary Kymo DATA frame in the opposite direction:
 * the channel ID names the parameter, the value is the new setting. The receiver
 * reassembles frames from raw received bytes, and applies a write only when
 *
 * - the frame is complete, CRC-protected (NO_CRC frames are discarded) and has
 *   no X, Z or timestamp field,
 * - the ID belongs to a registered parameter, and
 * - the finite value lies within the parameter's [minimum, maximum].
 *
 * Every accepted write and every out-of-range write marks the parameter for a
 * readback. The application sends the readback as a normal DATA frame with the
 * same ID and the value the parameter now holds; the host confirms its write by
 * that readback. An out-of-range write therefore reports the unchanged value.
 * Writes with an unknown ID, a bad CRC or a bad layout get no readback.
 *
 * Cost: KymoParam_Feed does constant work per byte and one table lookup of at
 * most KYMO_MAX_PARAMETERS entries per complete frame. Nothing runs unless the
 * application feeds bytes; nothing is compiled unless KYMO_ENABLE_PARAMETERS=1.
 * No heap, no callbacks, no SDK access: the application reads its transport.
 *
 * Concurrency: the receiver is not thread-safe. Call Feed and NextReadback from
 * one task. A parameter is a float; a 32-bit aligned float store is single-copy
 * atomic on 32-bit MCUs, so another task may read it without a lock. On 8-bit
 * MCUs read it in the same task or with interrupts disabled.
 *
 * The table maps an ID to an address with limits. A later firmware-image (ELF)
 * based mode can produce the same kind of entries from symbol addresses; the
 * range and readback rules above stay the contract.
 * @par Usage
 * @code{.c}
 * static float kp = 1.0f;
 * static const KymoParameter params[] = { { 100u, &kp, 0.0f, 10.0f } };
 * static KymoParamReceiver receiver;
 * KymoParam_Init(&receiver, params, 1u);
 * // In the application loop, with bytes read without blocking:
 * KymoParam_Feed(&receiver, rx, rx_length);
 * KymoDataPoint readback;
 * while (KymoParam_NextReadback(&receiver, &readback) == KYMO_OK) { send(readback); }
 * @endcode
 */
#ifndef KYMO_PARAM_H
/**
 * @brief Include guard for kymo_param.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include KymoCore.h normally; the compiler manages this guard.
 */
#define KYMO_PARAM_H

#include <stddef.h>
#include <stdint.h>
#include "../kymo_types.h"
#include "../telemetry/kymo_protocol.h"

#if KYMO_EFFECTIVE_PARAMETERS || defined(DOXYGEN)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One parameter the host may write.
 *
 * @details Owned by the application, typically a const table. The receiver keeps
 * a pointer to the table, so it must outlive the receiver.
 * @par Usage
 * { 100u, &kp, 0.0f, 10.0f } lets the host set kp between 0 and 10 via ID 100.
 */
typedef struct {
    /**
     * @brief Wire ID of writes to this parameter and of its readback.
     * @details Input; unique within the table. Choose IDs that the measurement
     * channels do not use, so readbacks are not mixed with measured values.
     */
    uint8_t id;
    /**
     * @brief Application variable that accepted writes update.
     * @details Input; must not be NULL and must stay valid while the receiver is used.
     */
    float *target;
    /**
     * @brief Smallest accepted value, inclusive.
     * @details Input; finite and not greater than maximum.
     */
    float minimum;
    /**
     * @brief Largest accepted value, inclusive.
     * @details Input; finite and not smaller than minimum.
     */
    float maximum;
} KymoParameter;

/**
 * @brief Receiver state: frame reassembly, pending readbacks and counters.
 *
 * @details Treat all fields as read-only after KymoParam_Init; the counters are
 * diagnostics and wrap modulo 2^32.
 * @par Usage
 * Declare one static KymoParamReceiver per receiving transport.
 */
typedef struct {
    /** @brief Registered parameter table (not owned). */
    const KymoParameter *table;
    /** @brief Number of table entries (1..KYMO_MAX_PARAMETERS). */
    size_t count;
    /** @brief Bytes of the frame being reassembled. */
    uint8_t frame[KYMO_FRAME_MAX_SIZE];
    /** @brief Number of valid bytes in frame. */
    size_t length;
    /** @brief Complete-frame length once the descriptor was seen, else zero. */
    size_t expected;
    /** @brief One bit per table index: a readback is due. */
    uint8_t pending[(KYMO_MAX_PARAMETERS + 7u) / 8u];
    /** @brief Table index where the next readback search starts (round robin). */
    size_t cursor;
    /** @brief Writes applied to a parameter. */
    uint32_t accepted;
    /** @brief Valid frames refused: unknown ID or value out of range. */
    uint32_t rejected;
    /** @brief Frames discarded: unsupported descriptor, NO_CRC, bad CRC or extra fields. */
    uint32_t invalid;
} KymoParamReceiver;

/**
 * @brief Validate a parameter table and reset the receiver.
 *
 * @details Rejects NULL arguments, an empty table, more than KYMO_MAX_PARAMETERS
 * entries, a NULL target, non-finite or inverted limits and duplicate IDs. Current
 * parameter values are not checked or changed. On failure the receiver is unusable
 * until a successful Init.
 * @param[out] receiver Receiver to reset; NULL is rejected.
 * @param[in] table Parameter table that outlives the receiver; NULL is rejected.
 * @param[in] count Number of entries, 1..KYMO_MAX_PARAMETERS.
 * @return KYMO_OK, or KYMO_BAD_CONFIG for an invalid table or argument.
 * @par Usage
 * Call once during start-up, before the first KymoParam_Feed.
 */
KymoStatus KymoParam_Init(KymoParamReceiver *receiver, const KymoParameter *table,
                          size_t count);

/**
 * @brief Consume received bytes and apply every valid parameter write in them.
 *
 * @details Bytes may arrive in any split; a frame may span calls. Bytes outside a
 * frame are skipped until the next sync marker. Constant work per byte plus one
 * table lookup per complete frame.
 * @param[in,out] receiver Initialized receiver; NULL or uninitialized input is ignored.
 * @param[in] bytes Received bytes; NULL is treated as empty.
 * @param[in] length Number of readable bytes.
 * @return Number of writes applied during this call.
 * @par Usage
 * Feed whatever a non-blocking read returned, then drain KymoParam_NextReadback.
 */
size_t KymoParam_Feed(KymoParamReceiver *receiver, const uint8_t *bytes, size_t length);

/**
 * @brief Take the next due readback.
 *
 * @details Fills id and the parameter's current value; flags and the remaining
 * fields are zero. The application may add a timestamp before encoding. Taking a
 * readback clears it, so encode and send it in the same step.
 * @param[in,out] receiver Initialized receiver; NULL is rejected.
 * @param[out] point Destination; NULL is rejected.
 * @return KYMO_OK with a readback, KYMO_IDLE if none is due, KYMO_BAD_CONFIG for NULL input.
 * @par Usage
 * while (KymoParam_NextReadback(&receiver, &point) == KYMO_OK) { encode and send point; }
 */
KymoStatus KymoParam_NextReadback(KymoParamReceiver *receiver, KymoDataPoint *point);

#ifdef __cplusplus
}
#endif

#endif /* KYMO_EFFECTIVE_PARAMETERS */

/** @} */
#endif /* KYMO_PARAM_H */
