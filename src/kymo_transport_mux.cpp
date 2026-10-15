/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial */
/**
 * @file kymo_transport_mux.cpp
 * @brief Bounded trace, legacy DATA, and metadata transport scheduler.
 */
#include "kymo_transport_mux.h"
#if KYMO_EFFECTIVE_TRACE
#include "kymo_protocol.h"
#include <string.h>

namespace {

/** ************************ crc32 ************************ */
uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xffffffffu;
    for (size_t index = 0u; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xedb88320u) : (crc >> 1);
        }
    }
    return crc ^ 0xffffffffu;
}

/** ************************ put16 ************************ */
void put16(uint8_t *output, size_t offset, uint16_t value)
{
    output[offset] = (uint8_t)value;
    output[offset + 1u] = (uint8_t)(value >> 8);
}

/** ************************ put32 ************************ */
void put32(uint8_t *output, size_t offset, uint32_t value)
{
    for (uint8_t index = 0u; index < 4u; ++index) {
        output[offset + index] = (uint8_t)(value >> (8u * index));
    }
}

/** ************************ put64 ************************ */
void put64(uint8_t *output, size_t offset, uint64_t value)
{
    for (uint8_t index = 0u; index < 8u; ++index) {
        output[offset + index] = (uint8_t)(value >> (8u * index));
    }
}

/** ************************ allRingsEmpty ************************ */
uint8_t allRingsEmpty(const KymoTraceRecorder *recorder)
{
    uint8_t empty = 1u;
    for (uint8_t core = 0u; core < KYMO_TRACE_CORE_COUNT; ++core) {
        if (recorder->counts[core] != 0u) {
            empty = 0u;
        }
    }
    return empty;
}

/** ************************ validDataFrame ************************ */
uint8_t validDataFrame(const uint8_t *frame, size_t length)
{
    uint8_t valid = 0u;
    if ((frame != NULL) && (length >= KYMO_FRAME_MIN_SIZE) &&
        (length <= KYMO_FRAME_MAX_SIZE) && (frame[0] == KYMO_SYNC_0) &&
        (frame[1] == KYMO_SYNC_1)) {
        uint8_t descriptor = frame[2];
        uint8_t flags = (uint8_t)(descriptor & KYMO_ALLOWED_FLAGS);
        size_t expected = KYMO_FRAME_MIN_SIZE;
        if (((descriptor & KYMO_DESCRIPTOR_VERSION_MASK) ==
             KYMO_DESCRIPTOR_DATA) &&
            ((descriptor & KYMO_DESCRIPTOR_RESERVED_MASK) == 0u) &&
            (((descriptor & KYMO_FLAG_TIMESTAMP_US) == 0u) ||
             ((flags & KYMO_FLAG_TIMESTAMP) != 0u))) {
            if ((flags & KYMO_FLAG_X) != 0u) {
                expected += 4u;
            }
            if ((flags & KYMO_FLAG_Z) != 0u) {
                expected += 4u;
            }
            if ((flags & KYMO_FLAG_TIMESTAMP) != 0u) {
                expected += 4u;
            }
            if (length == expected) {
                valid = 1u;
            }
        }
    }
    return valid;
}

/** ************************ prepareAdmin ************************ */
uint8_t prepareAdmin(KymoTraceMux *mux)
{
    uint8_t result = 0u;
    size_t fragmentCapacity = (size_t)mux->config.max_frame_size -
        KYMO_TRACE_ENVELOPE_SIZE - KYMO_TRACE_ADMIN_PREFIX_SIZE;
    size_t remaining = (size_t)(mux->admin_total - mux->admin_offset);
    mux->pending_admin_bytes = (uint16_t)((remaining < fragmentCapacity) ?
        remaining : fragmentCapacity);
    uint8_t payload[KYMO_TRACE_FRAME_MAX - KYMO_TRACE_ENVELOPE_SIZE];
    size_t payloadLength = KymoTrace_EncodeAdminFragment(mux->admin_kind,
        mux->admin_generation, mux->transfer_id, mux->admin_total,
        mux->admin_offset, mux->admin_record, mux->pending_admin_bytes,
        payload, sizeof(payload));
    if (payloadLength > 0u) {
        KymoTraceEnvelopeInfo info = {mux->config.recorder->config.session_id,
            mux->packet_seq++, mux->admin_kind};
        mux->frame_length = KymoTrace_EncodeEnvelope(&info, payload,
            payloadLength, mux->frame, sizeof(mux->frame));
        mux->pending_kind = 1u;
        result = (mux->frame_length > 0u) ? 1u : 0u;
    }
    return result;
}

/** ************************ prepareTrace ************************ */
uint8_t prepareTrace(KymoTraceMux *mux)
{
    uint8_t result = 0u;
    KymoTraceEvent events[6];
    uint8_t payload[KYMO_TRACE_FRAME_MAX - KYMO_TRACE_ENVELOPE_SIZE];
    size_t budget = (mux->config.max_frame_size - KYMO_TRACE_ENVELOPE_SIZE - 4u) /
        KYMO_TRACE_EVENT_SIZE;
    size_t limit = (mux->config.max_batch_events == 0u) ? 6u :
        mux->config.max_batch_events;
    if (limit > budget) {
        limit = budget;
    }
    if (limit > 6u) {
        limit = 6u;
    }
    size_t count = 0u;
    while ((count < limit) &&
           (KymoTrace_Pop(mux->config.recorder, &events[count]) != 0u)) {
        ++count;
    }
    if (count > 0u) {
        size_t payloadLength = KymoTrace_EncodeBatch(
            mux->config.recorder->config.clock_domain_id, events, count,
            payload, sizeof(payload));
        if (payloadLength > 0u) {
            KymoTraceEnvelopeInfo info = {mux->config.recorder->config.session_id,
                mux->packet_seq++, KYMO_TRACE_PAYLOAD_BATCH};
            mux->frame_length = KymoTrace_EncodeEnvelope(&info, payload,
                payloadLength, mux->frame, sizeof(mux->frame));
            mux->pending_kind = 2u;
            result = (mux->frame_length > 0u) ? 1u : 0u;
        }
    }
    return result;
}

/** ************************ prepareData ************************ */
uint8_t prepareData(KymoTraceMux *mux)
{
    uint8_t result = 0u;
    if (mux->config.next_data != NULL) {
        uint8_t data[KYMO_TRACE_FRAME_MAX];
        size_t capacity = (size_t)mux->config.max_frame_size -
            KYMO_TRACE_ENVELOPE_SIZE;
        size_t dataLength = mux->config.next_data(mux->config.user, data,
                                                   capacity);
        if ((dataLength <= capacity) &&
            (validDataFrame(data, dataLength) != 0u)) {
            KymoTraceEnvelopeInfo info = {mux->config.recorder->config.session_id,
                mux->packet_seq++, KYMO_TRACE_PAYLOAD_DATA_V1};
            mux->frame_length = KymoTrace_EncodeEnvelope(&info, data,
                dataLength, mux->frame, sizeof(mux->frame));
            mux->pending_kind = 3u;
            result = (mux->frame_length > 0u) ? 1u : 0u;
        }
    }
    return result;
}

/** ************************ finishAccepted ************************ */
void finishAccepted(KymoTraceMux *mux)
{
    mux->last_frame_length = mux->frame_length;
    mux->last_frame_events = (mux->pending_kind == 2u) ? mux->frame[23u] : 0u;
    if (mux->pending_kind == 1u) {
        mux->admin_offset += mux->pending_admin_bytes;
        if (mux->admin_offset >= mux->admin_total) {
            mux->admin_pending = 0u;
        }
    }
    mux->pending = 0u;
    mux->pending_kind = 0u;
    mux->frame_length = 0u;
    mux->frame_offset = 0u;
}

/** ************************ prepareHealth ************************ */
uint8_t prepareHealth(KymoTraceMux *mux)
{
    uint8_t queued = 0u;
    uint8_t shouldQueue = 0u;
    uint8_t body[4u + 32u * KYMO_TRACE_CORE_COUNT];
    KymoTraceRecorder *recorder = mux->config.recorder;
    if ((mux->config.health_period_ticks > 0u) &&
        (mux->admin_pending == 0u) && (mux->pending == 0u)) {
        if (recorder->config.critical_enter != NULL) {
            recorder->config.critical_enter(recorder->config.user);
        }
        uint64_t now = recorder->config.clock_ticks(recorder->config.user);
        if ((now >= mux->next_health_ticks) &&
            (allRingsEmpty(recorder) != 0u)) {
            body[0] = 1u;
            body[1] = KYMO_TRACE_CORE_COUNT;
            put16(body, 2u, 0u);
            for (uint8_t core = 0u; core < KYMO_TRACE_CORE_COUNT; ++core) {
                size_t offset = 4u + (size_t)core * 32u;
                recorder->settled_through[core] = recorder->producer_next[core];
                body[offset] = core;
                body[offset + 1u] = 1u;
                put16(body, offset + 2u, 0u);
                put16(body, offset + 4u, recorder->high_water[core]);
                put16(body, offset + 6u, 0u);
                put32(body, offset + 8u, recorder->producer_next[core]);
                put32(body, offset + 12u, recorder->settled_through[core]);
                put32(body, offset + 16u, recorder->drops[core]);
                put32(body, offset + 20u, recorder->errors[core]);
                put64(body, offset + 24u, now);
            }
            mux->next_health_ticks = now + mux->config.health_period_ticks;
            shouldQueue = 1u;
        }
        if (recorder->config.critical_exit != NULL) {
            recorder->config.critical_exit(recorder->config.user);
        }
        if (shouldQueue != 0u) {
            queued = KymoTraceMux_QueueAdmin(mux, KYMO_TRACE_PAYLOAD_HEALTH,
                                             1u, body, sizeof(body));
        }
    }
    return queued;
}
}

/** ************************ KymoTraceMux_Init ************************ */
uint8_t KymoTraceMux_Init(KymoTraceMux *mux,
                          const KymoTraceMuxConfig *config)
{
    uint8_t result = 0u;
    if ((mux != NULL) && (config != NULL) && (config->recorder != NULL) &&
        (config->recorder->initialized != 0u) &&
        (config->recorder->config.clock_ticks != NULL) &&
        (config->write_frame != NULL) && (config->is_busy != NULL) &&
        (config->max_frame_size >= 64u) &&
        (config->max_frame_size <= KYMO_TRACE_FRAME_MAX) &&
        (config->max_batch_events <= 6u)) {
        memset(mux, 0, sizeof(*mux));
        mux->config = *config;
        mux->next_health_ticks = config->recorder->config.clock_ticks(
            config->recorder->config.user) + config->health_period_ticks;
        mux->initialized = 1u;
        result = 1u;
    }
    return result;
}

/** ************************ KymoTraceMux_QueueAdmin ************************ */
uint8_t KymoTraceMux_QueueAdmin(KymoTraceMux *mux, uint8_t kind,
                                uint32_t generation, const uint8_t *body,
                                size_t bodyLength)
{
    uint8_t result = 0u;
    if ((mux != NULL) && (mux->initialized != 0u) &&
        (mux->admin_pending == 0u) && (body != NULL) &&
        (bodyLength <= KYMO_TRACE_ADMIN_RECORD_MAX - 4u) &&
        (kind >= KYMO_TRACE_PAYLOAD_SESSION_META) &&
        (kind <= KYMO_TRACE_PAYLOAD_SESSION_END) &&
        (kind != KYMO_TRACE_PAYLOAD_BATCH) &&
        (kind != KYMO_TRACE_PAYLOAD_DATA_V1)) {
        for (size_t index = 0u; index < bodyLength; ++index) {
            mux->admin_record[index] = body[index];
        }
        put32(mux->admin_record, bodyLength, crc32(body, bodyLength));
        mux->admin_total = (uint32_t)(bodyLength + 4u);
        mux->admin_offset = 0u;
        mux->admin_generation = generation;
        mux->admin_kind = kind;
        ++mux->transfer_id;
        mux->admin_pending = 1u;
        result = 1u;
    }
    return result;
}

/** ************************ KymoTraceMux_QueueSessionEnd ************************ */
uint8_t KymoTraceMux_QueueSessionEnd(KymoTraceMux *mux, uint32_t generation)
{
    uint8_t result = 0u;
    uint8_t body[4u + 24u * KYMO_TRACE_CORE_COUNT] = {};
    if ((mux != NULL) && (mux->initialized != 0u) &&
        (mux->pending == 0u) && (mux->admin_pending == 0u) &&
        (mux->config.is_busy(mux->config.user) == 0u)) {
        KymoTraceRecorder *recorder = mux->config.recorder;
        if (recorder->config.critical_enter != NULL) {
            recorder->config.critical_enter(recorder->config.user);
        }
        uint8_t empty = allRingsEmpty(recorder);
        if (empty != 0u) {
            body[0] = 1u;
            body[1] = KYMO_TRACE_CORE_COUNT;
            put16(body, 2u, 0u);
            for (uint8_t core = 0u; core < KYMO_TRACE_CORE_COUNT; ++core) {
                const size_t offset = 4u + (size_t)core * 24u;
                recorder->settled_through[core] = recorder->producer_next[core];
                body[offset] = core;
                put32(body, offset + 4u, recorder->producer_next[core]);
                put32(body, offset + 8u, recorder->settled_through[core]);
                put32(body, offset + 12u, recorder->drops[core]);
                put64(body, offset + 16u,
                      recorder->config.clock_ticks(recorder->config.user));
            }
        }
        if (recorder->config.critical_exit != NULL) {
            recorder->config.critical_exit(recorder->config.user);
        }
        if (empty != 0u) {
            result = KymoTraceMux_QueueAdmin(mux,
                KYMO_TRACE_PAYLOAD_SESSION_END, generation, body, sizeof(body));
        }
    }
    return result;
}

/** ************************ KymoTraceMux_Service ************************ */
uint8_t KymoTraceMux_Service(KymoTraceMux *mux)
{
    uint8_t result = 0u;
    if ((mux != NULL) && (mux->initialized != 0u)) {
        if (mux->fault != 0u) {
            result = 2u;
        } else if (mux->pending != 0u) {
            if (mux->config.is_busy(mux->config.user) == 0u) {
                if (mux->frame_offset < mux->frame_length) {
                    size_t remaining = mux->frame_length - mux->frame_offset;
                    size_t accepted = mux->config.write_frame(mux->config.user,
                        mux->frame + mux->frame_offset, remaining);
                    if (accepted > remaining) {
                        mux->fault = 1u;
                        result = 2u;
                    } else {
                        mux->frame_offset += accepted;
                    }
                }
                if ((mux->frame_offset == mux->frame_length) &&
                    (mux->config.is_busy(mux->config.user) == 0u)) {
                    finishAccepted(mux);
                    result = 1u;
                }
            }
        } else if (mux->config.is_busy(mux->config.user) == 0u) {
            (void)prepareHealth(mux);
            for (uint8_t attempt = 0u; attempt < 3u; ++attempt) {
                uint8_t lane = (uint8_t)((mux->lane + attempt) % 3u);
                uint8_t made = 0u;
                if ((lane == 0u) && (mux->admin_pending != 0u)) {
                    made = prepareAdmin(mux);
                } else if (lane == 1u) {
                    made = prepareTrace(mux);
                } else if (lane == 2u) {
                    made = prepareData(mux);
                }
                if (made != 0u) {
                    mux->pending = 1u;
                    mux->frame_offset = 0u;
                    mux->lane = (uint8_t)((lane + 1u) % 3u);
                    size_t accepted = mux->config.write_frame(mux->config.user,
                        mux->frame, mux->frame_length);
                    if (accepted > mux->frame_length) {
                        mux->fault = 1u;
                        result = 2u;
                    } else {
                        mux->frame_offset = accepted;
                        if ((mux->frame_offset == mux->frame_length) &&
                            (mux->config.is_busy(mux->config.user) == 0u)) {
                            finishAccepted(mux);
                            result = 1u;
                        }
                    }
                    break;
                }
            }
        }
    }
    return result;
}

/** ************************ KymoTraceDataAdapter_Init ************************ */
uint8_t KymoTraceDataAdapter_Init(KymoTraceDataAdapter *adapter)
{
    uint8_t result = 0u;
    if (adapter != NULL) {
        memset(adapter, 0, sizeof(*adapter));
        adapter->initialized = 1u;
        result = 1u;
    }
    return result;
}

/** ************************ KymoTraceDataAdapter_Write ************************ */
uint8_t KymoTraceDataAdapter_Write(void *user, const uint8_t *bytes,
                                   uint8_t length)
{
    uint8_t result = 0u;
    KymoTraceDataAdapter *adapter = static_cast<KymoTraceDataAdapter *>(user);
    if ((adapter != NULL) && (adapter->initialized != 0u) &&
        (adapter->pending == 0u) && (validDataFrame(bytes, length) != 0u)) {
        memcpy(adapter->frame, bytes, length);
        adapter->length = length;
        adapter->pending = 1u;
        result = length;
    }
    return result;
}

/** ************************ KymoTraceDataAdapter_IsBusy ************************ */
uint8_t KymoTraceDataAdapter_IsBusy(void *user)
{
    uint8_t result = 0u;
    KymoTraceDataAdapter *adapter = static_cast<KymoTraceDataAdapter *>(user);
    if ((adapter != NULL) && (adapter->initialized != 0u)) {
        result = adapter->pending;
    }
    return result;
}

/** ************************ KymoTraceDataAdapter_NextData ************************ */
size_t KymoTraceDataAdapter_NextData(void *user, uint8_t *output,
                                     size_t capacity)
{
    size_t result = 0u;
    KymoTraceDataAdapter *adapter = static_cast<KymoTraceDataAdapter *>(user);
    if ((adapter != NULL) && (output != NULL) &&
        (adapter->initialized != 0u) && (adapter->pending != 0u) &&
        (capacity >= adapter->length)) {
        memcpy(output, adapter->frame, adapter->length);
        result = adapter->length;
        adapter->length = 0u;
        adapter->pending = 0u;
    }
    return result;
}
#endif
