/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */

/**
 * @file kymo_trace_protocol.h
 * @brief Portable wire-v2 constants and fieldwise encoder.
 * @details No target layout is copied to the wire. This file has no transport or port APIs.
 * @par Usage Include from the trace mux and host-side native tests.
 */
#ifndef KYMO_TRACE_PROTOCOL_H
#define KYMO_TRACE_PROTOCOL_H

#include "kymo_features.h"

#if KYMO_EFFECTIVE_TRACE
#include <stddef.h>
#include <stdint.h>
#define KYMO_TRACE_FRAME_MAX 240u
#define KYMO_TRACE_ENVELOPE_SIZE 24u
#define KYMO_TRACE_EVENT_SIZE 32u
#define KYMO_TRACE_ADMIN_PREFIX_SIZE 18u

/** @brief Payload kind values from the v2 protocol. */
typedef enum KymoTracePayloadKind {
    KYMO_TRACE_PAYLOAD_SESSION_META = 1,
    KYMO_TRACE_PAYLOAD_DICTIONARY = 2,
    KYMO_TRACE_PAYLOAD_BATCH = 3,
    KYMO_TRACE_PAYLOAD_DATA_V1 = 4,
    KYMO_TRACE_PAYLOAD_HEALTH = 5,
    KYMO_TRACE_PAYLOAD_SESSION_END = 6
} KymoTracePayloadKind;

/** @brief Event values in event schema v1. */
typedef enum KymoTraceEventType {
    KYMO_TRACE_EVENT_ENTER = 1,
    KYMO_TRACE_EVENT_EXIT = 2,
    KYMO_TRACE_EVENT_MARK = 3,
    KYMO_TRACE_EVENT_ISR_ENTER = 4,
    KYMO_TRACE_EVENT_ISR_EXIT = 5,
    KYMO_TRACE_EVENT_CONTEXT_RESET = 6,
    KYMO_TRACE_EVENT_TASK_CREATE = 0x20,
    KYMO_TRACE_EVENT_TASK_END = 0x21,
    KYMO_TRACE_EVENT_TASK_SWITCH = 0x22,
    KYMO_TRACE_EVENT_CORE_STATE = 0x23,
    KYMO_TRACE_EVENT_FLOW_SEND = 0x30,
    KYMO_TRACE_EVENT_FLOW_RECEIVE = 0x31
} KymoTraceEventType;

/** @brief One logical trace event; serialized fieldwise. */
typedef struct KymoTraceEvent {
    uint64_t timestamp_ticks;
    uint32_t record_seq;
    uint32_t context_id;
    uint32_t context_seq;
    uint16_t symbol_id;
    uint16_t depth;
    uint8_t event_type;
    uint8_t core_id;
    uint16_t flags;
    uint32_t arg;
} KymoTraceEvent;

/** @brief Envelope identifiers supplied by the caller. */
typedef struct KymoTraceEnvelopeInfo {
    uint64_t session_id;
    uint32_t packet_seq;
    uint8_t payload_kind;
} KymoTraceEnvelopeInfo;

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Compute CRC16/CCITT-FALSE. @param[in] data Bytes. @param[in] length Size. @return CRC value. */
uint16_t KymoTrace_Crc16(const uint8_t *data, size_t length);

/** @brief Encode an envelope. @return Encoded byte count, or zero on invalid bounds. */
size_t KymoTrace_EncodeEnvelope(const KymoTraceEnvelopeInfo *info, const uint8_t *payload,
                               size_t payload_length, uint8_t *output, size_t capacity);

/** @brief Encode a trace batch. @return Encoded payload length, or zero on invalid bounds. */
size_t KymoTrace_EncodeBatch(uint16_t clock_domain_id, const KymoTraceEvent *events,
                             size_t event_count, uint8_t *output, size_t capacity);

/** @brief Encode one common-prefix administrative fragment. @return Encoded size or zero. */
size_t KymoTrace_EncodeAdminFragment(uint8_t kind, uint32_t generation, uint32_t transfer_id,
                                     uint32_t total_length, uint32_t offset,
                                     const uint8_t *record, size_t fragment_length,
                                     uint8_t *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
#endif
