/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial */

/** @file kymo_transport_mux.h @brief Single-writer trace/DATA transport mux. */
#ifndef KYMO_TRANSPORT_MUX_H
#define KYMO_TRANSPORT_MUX_H

#include "kymo_trace.h"

#if KYMO_EFFECTIVE_TRACE
#ifndef KYMO_TRACE_ADMIN_RECORD_MAX
#define KYMO_TRACE_ADMIN_RECORD_MAX 512u
#endif

#if (KYMO_TRACE_ADMIN_RECORD_MAX < 64u) || (KYMO_TRACE_ADMIN_RECORD_MAX > 8192u)
#error "KYMO_TRACE_ADMIN_RECORD_MAX must be from 64 through 8192"
#endif

typedef size_t (*KymoTraceWriteFrame)(void *user, const uint8_t *bytes, size_t length);

typedef uint8_t (*KymoTraceTransportBusy)(void *user);

typedef size_t (*KymoTraceNextData)(void *user, uint8_t *frame, size_t capacity);

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KymoTraceMuxConfig {
    KymoTraceRecorder *recorder;
    KymoTraceWriteFrame write_frame;
    KymoTraceTransportBusy is_busy;
    KymoTraceNextData next_data;
    void *user;
    uint16_t max_frame_size;
    uint32_t health_period_ticks;
    uint8_t max_batch_events;
} KymoTraceMuxConfig;

typedef struct KymoTraceMux {
    KymoTraceMuxConfig config;
    uint8_t frame[KYMO_TRACE_FRAME_MAX];
    uint8_t admin_record[KYMO_TRACE_ADMIN_RECORD_MAX];
    size_t frame_length;
    size_t frame_offset;
    size_t last_frame_length;
    uint32_t admin_total;
    uint32_t admin_offset;
    uint32_t admin_generation;
    uint32_t transfer_id;
    uint32_t packet_seq;
    uint64_t next_health_ticks;
    uint16_t pending_admin_bytes;
    uint8_t admin_kind;
    uint8_t admin_pending;
    uint8_t pending;
    uint8_t pending_kind;
    uint8_t last_frame_events;
    uint8_t lane;
    uint8_t fault;
    uint8_t initialized;
} KymoTraceMux;

/** @brief Bounded one-frame bridge from the legacy Kymo sender into the mux. */
typedef struct KymoTraceDataAdapter {
    uint8_t frame[21];
    uint8_t length;
    uint8_t pending;
    uint8_t initialized;
} KymoTraceDataAdapter;

/** @brief Initialize a persistent mux; write_frame must accept whole frames or zero. */
uint8_t KymoTraceMux_Init(KymoTraceMux *mux, const KymoTraceMuxConfig *config);

/** @brief Queue a bounded admin record body; copies it and appends CRC32. */
uint8_t KymoTraceMux_QueueAdmin(
    KymoTraceMux *mux,
    uint8_t kind,
    uint32_t generation,
    const uint8_t *body,
    size_t body_length);

/** @brief Queue SESSION_END after producer quiescence and ring settlement.
 * @details The application must stop all producers before calling this API.
 * The settled frontier is exclusive: it is one past the last accounted
 * record sequence, including attempts accounted as drops.
 * @param[in,out] mux Initialized mux whose recorder has no queued records.
 * @param[in] generation Session generation to include in the record.
 * @return 1 if the exclusive settled frontier was queued; otherwise 0.
 */
uint8_t KymoTraceMux_QueueSessionEnd(KymoTraceMux *mux, uint32_t generation);

/** @brief Service at most one whole message attempt. @return 1 accepted, 0 idle/busy, 2 error. */
uint8_t KymoTraceMux_Service(KymoTraceMux *mux);

/** @brief Initialize a persistent single-frame DATA adapter. */
uint8_t KymoTraceDataAdapter_Init(KymoTraceDataAdapter *adapter);

/** @brief Accept one complete v1 frame; returns byte count or zero for backpressure. */
uint8_t KymoTraceDataAdapter_Write(void *adapter, const uint8_t *bytes, uint8_t length);

/** @brief Report the adapter slot occupied. */
uint8_t KymoTraceDataAdapter_IsBusy(void *adapter);

/** @brief Copy and consume the queued v1 frame for mux scheduling. */
size_t KymoTraceDataAdapter_NextData(void *adapter, uint8_t *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
#endif
