/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/** @file kymo_trace_protocol.cpp @brief Fieldwise trace protocol encoding. */
#include "kymo_trace_protocol.h"

#if KYMO_EFFECTIVE_TRACE

namespace {

/** ************************ put16 ************************ */
void put16(uint8_t *output, size_t offset, uint16_t value) {
    output[offset] = static_cast<uint8_t>(value);
    output[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

/** ************************ put32 ************************ */
void put32(uint8_t *output, size_t offset, uint32_t value) {
    for (size_t index = 0u; index < 4u; ++index) {
        output[offset + index] = static_cast<uint8_t>(value >> (8u * index));
    }
}

/** ************************ put64 ************************ */
void put64(uint8_t *output, size_t offset, uint64_t value) {
    for (size_t index = 0u; index < 8u; ++index) {
        output[offset + index] = static_cast<uint8_t>(value >> (8u * index));
    }
}

}  // namespace

/** ************************ KymoTrace_Crc16 ************************ */
uint16_t KymoTrace_Crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xffffu;
    if (data != NULL) {
        for (size_t index = 0u; index < length; ++index) {
            crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[index]) << 8u));
            for (uint8_t bit = 0u; bit < 8u; ++bit) {
                const uint16_t shifted = static_cast<uint16_t>(crc << 1u);
                crc = (crc & 0x8000u) != 0u
                    ? static_cast<uint16_t>(shifted ^ 0x1021u)
                    : shifted;
            }
        }
    }
    return crc;
}

/** ************************ KymoTrace_EncodeEnvelope ************************ */
size_t KymoTrace_EncodeEnvelope(const KymoTraceEnvelopeInfo *info,
                                const uint8_t *payload,
                                size_t payload_length,
                                uint8_t *output,
                                size_t capacity) {
    size_t result = 0u;
    if ((info != NULL) && (output != NULL) &&
        ((payload != NULL) || (payload_length == 0u)) &&
        (payload_length <= KYMO_TRACE_FRAME_MAX - KYMO_TRACE_ENVELOPE_SIZE) &&
        (capacity >= payload_length + KYMO_TRACE_ENVELOPE_SIZE)) {
        output[0] = 0xa5u;
        output[1] = 0x5au;
        output[2] = 0x80u;
        output[3] = info->payload_kind;
        put16(output, 4u, static_cast<uint16_t>(payload_length));
        put16(output, 6u, 0u);
        put64(output, 8u, info->session_id);
        put32(output, 16u, info->packet_seq);
        put16(output, 20u, KymoTrace_Crc16(output + 2u, 18u));
        for (size_t index = 0u; index < payload_length; ++index) {
            output[22u + index] = payload[index];
        }
        put16(output, 22u + payload_length,
              KymoTrace_Crc16(output + 2u, 20u + payload_length));
        result = 24u + payload_length;
    }
    return result;
}

/** ************************ KymoTrace_EncodeBatch ************************ */
size_t KymoTrace_EncodeBatch(uint16_t domain,
                             const KymoTraceEvent *events,
                             size_t count,
                             uint8_t *output,
                             size_t capacity) {
    size_t result = 0u;
    if ((output != NULL) && ((events != NULL) || (count == 0u)) &&
        (count <= 255u) && (capacity >= 4u + count * KYMO_TRACE_EVENT_SIZE)) {
        output[0] = 1u;
        output[1] = static_cast<uint8_t>(count);
        put16(output, 2u, domain);
        for (size_t index = 0u; index < count; ++index) {
            const size_t offset = 4u + index * KYMO_TRACE_EVENT_SIZE;
            const KymoTraceEvent &event = events[index];
            put64(output, offset, event.timestamp_ticks);
            put32(output, offset + 8u, event.record_seq);
            put32(output, offset + 12u, event.context_id);
            put32(output, offset + 16u, event.context_seq);
            put16(output, offset + 20u, event.symbol_id);
            put16(output, offset + 22u, event.depth);
            output[offset + 24u] = event.event_type;
            output[offset + 25u] = event.core_id;
            put16(output, offset + 26u, event.flags);
            put32(output, offset + 28u, event.arg);
        }
        result = 4u + count * KYMO_TRACE_EVENT_SIZE;
    }
    return result;
}

/** ************************ KymoTrace_EncodeAdminFragment ************************ */
size_t KymoTrace_EncodeAdminFragment(uint8_t kind,
                                    uint32_t generation,
                                    uint32_t transfer,
                                    uint32_t total,
                                    uint32_t offset,
                                    const uint8_t *record,
                                    size_t length,
                                    uint8_t *output,
                                    size_t capacity) {
    size_t result = 0u;
    if ((output != NULL) && ((record != NULL) || (length == 0u)) &&
        (length <= capacity) && (capacity >= KYMO_TRACE_ADMIN_PREFIX_SIZE + length) &&
        (offset <= total) && (length <= total - offset) && (kind != 0u)) {
        output[0] = 1u;
        output[1] = kind;
        put32(output, 2u, generation);
        put32(output, 6u, transfer);
        put32(output, 10u, total);
        put32(output, 14u, offset);
        for (size_t index = 0u; index < length; ++index) {
            output[KYMO_TRACE_ADMIN_PREFIX_SIZE + index] = record[offset + index];
        }
        result = KYMO_TRACE_ADMIN_PREFIX_SIZE + length;
    }
    return result;
}

#endif
