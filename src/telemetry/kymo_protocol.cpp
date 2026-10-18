/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
#include "../kymo_config.h"
#if KYMO_ENABLED
#include "kymo_protocol.h"
#include "../common/embedded_bytes.h"
#if KYMO_ENABLE_CRC || KYMO_ENABLE_DECODER
#include "../common/embedded_crc.h"
#endif
#include <math.h>

/*******************************************************************************
 * flags_valid
 ******************************************************************************/
/* The microsecond unit is meaningless without a timestamp field. */
static uint8_t flags_valid(uint8_t flags)
{
    uint8_t valid = (uint8_t)(emb_u8_only_bits(flags, KYMO_CODEC_FLAGS) &&
        (!emb_u8_has_any(flags, KYMO_FLAG_TIMESTAMP_US) ||
         emb_u8_has_any(flags, KYMO_FLAG_TIMESTAMP)));
    return valid;
}

/*******************************************************************************
 * point_valid
 ******************************************************************************/
static uint8_t point_valid(const KymoDataPoint *point)
{
    uint8_t valid = 0;
    if (point != nullptr) {
        valid = (uint8_t)(flags_valid(point->flags) &&
            isfinite(point->value)
#if KYMO_ENABLE_X
            && (!emb_u8_has_any(point->flags, KYMO_FLAG_X) || isfinite(point->x))
#endif
#if KYMO_ENABLE_Z
            && (!emb_u8_has_any(point->flags, KYMO_FLAG_Z) || isfinite(point->z))
#endif
            );
    }
    return valid;
}

#if KYMO_ENABLE_CRC || KYMO_ENABLE_DECODER
/*******************************************************************************
 * KymoProtocol_Crc8
 ******************************************************************************/
uint8_t KymoProtocol_Crc8(const uint8_t *data, size_t length)
{
    uint8_t result = emb_crc8_msb(data, length, 0x07u, 0u);
    return result;
}

#endif

/*******************************************************************************
 * KymoProtocol_FrameLength
 ******************************************************************************/
size_t KymoProtocol_FrameLength(uint8_t descriptor)
{
    size_t length = 0;
    if ((emb_u8_mask(descriptor, KYMO_DESCRIPTOR_VERSION_MASK) == KYMO_DESCRIPTOR_DATA) &&
        !emb_u8_has_any(descriptor, KYMO_DESCRIPTOR_RESERVED_MASK) &&
        flags_valid(emb_u8_mask(descriptor, EMB_U8_OR(KYMO_ALLOWED_FLAGS,
                                                      KYMO_FLAG_TIMESTAMP_US)))) {
        length = KYMO_FRAME_MIN_SIZE;
#if KYMO_ENABLE_X
        if (emb_u8_has_any(descriptor, KYMO_FLAG_X)) {
            length += 4u;
        }
#endif
#if KYMO_ENABLE_Z
        if (emb_u8_has_any(descriptor, KYMO_FLAG_Z)) {
            length += 4u;
        }
#endif
#if KYMO_ENABLE_TIMESTAMP
        if (emb_u8_has_any(descriptor, KYMO_FLAG_TIMESTAMP)) {
            length += 4u;
        }
#endif
    }
    return length;
}

/*******************************************************************************
 * KymoProtocol_Encode
 ******************************************************************************/
size_t KymoProtocol_Encode(const KymoDataPoint *point,
                           uint8_t *output, size_t output_capacity)
{
    size_t result = 0;
    if ((output != nullptr) && point_valid(point)) {
        uint8_t descriptor = EMB_U8_OR(KYMO_DESCRIPTOR_DATA, point->flags);
#if !KYMO_ENABLE_CRC
        descriptor = EMB_U8_OR(descriptor, KYMO_DESCRIPTOR_NO_CRC);
#endif
        size_t required = KymoProtocol_FrameLength(descriptor);
        if ((required != 0u) && (output_capacity >= required)) {
            size_t offset = 4;
            output[0] = KYMO_SYNC_0;
            output[1] = KYMO_SYNC_1;
            output[2] = descriptor;
            output[3] = point->id;
#if KYMO_ENABLE_X
            if (emb_u8_has_any(point->flags, KYMO_FLAG_X)) {
                emb_write_f32_le(output + offset, point->x);
                offset += 4u;
            }
#endif
            emb_write_f32_le(output + offset, point->value);
            offset += 4u;
#if KYMO_ENABLE_Z
            if (emb_u8_has_any(point->flags, KYMO_FLAG_Z)) {
                emb_write_f32_le(output + offset, point->z);
                offset += 4u;
            }
#endif
#if KYMO_ENABLE_TIMESTAMP
            if (emb_u8_has_any(point->flags, KYMO_FLAG_TIMESTAMP)) {
                emb_write_u32_le(output + offset, point->timestamp_ms);
                offset += 4u;
            }
#endif
#if KYMO_ENABLE_CRC
            output[offset] = KymoProtocol_Crc8(output + 2, offset - 2u);
#else
            output[offset] = 0u;
#endif
            result = offset + 1u;
        }
    }
    return result;
}

#if KYMO_ENABLE_DECODER
/*******************************************************************************
 * frame_valid
 ******************************************************************************/
static uint8_t frame_valid(const uint8_t *frame, size_t length)
{
    uint8_t valid = 0;
    if ((frame != nullptr) && (length >= KYMO_FRAME_MIN_SIZE)) {
        size_t expected = KymoProtocol_FrameLength(frame[2]);
        if ((frame[0] == KYMO_SYNC_0) && (frame[1] == KYMO_SYNC_1) &&
            (expected != 0u) && (expected == length)) {
            valid = (uint8_t)(emb_u8_has_any(frame[2], KYMO_DESCRIPTOR_NO_CRC) ||
                (KymoProtocol_Crc8(frame + 2, length - 3u) == frame[length - 1u]));
        }
    }
    return valid;
}

/* Only called after frame_valid: the descriptor proves all payload bounds. */

/*******************************************************************************
 * decode_payload
 ******************************************************************************/
static void decode_payload(const uint8_t *frame, KymoDataPoint *point)
{
    size_t offset = 4;
    point->id = frame[3];
    point->flags = emb_u8_mask(frame[2], EMB_U8_OR(KYMO_ALLOWED_FLAGS, KYMO_FLAG_TIMESTAMP_US));
#if KYMO_ENABLE_X
    if (emb_u8_has_any(point->flags, KYMO_FLAG_X)) {
        point->x = emb_read_f32_le(frame + offset);
        offset += 4u;
    }
#endif
    point->value = emb_read_f32_le(frame + offset);
    offset += 4u;
#if KYMO_ENABLE_Z
    if (emb_u8_has_any(point->flags, KYMO_FLAG_Z)) {
        point->z = emb_read_f32_le(frame + offset);
        offset += 4u;
    }
#endif
#if KYMO_ENABLE_TIMESTAMP
    if (emb_u8_has_any(point->flags, KYMO_FLAG_TIMESTAMP)) {
        point->timestamp_ms = emb_read_u32_le(frame + offset);
    }
#endif
}

/*******************************************************************************
 * KymoProtocol_Decode
 ******************************************************************************/
KymoStatus KymoProtocol_Decode(const uint8_t *frame, size_t frame_length,
                               KymoDataPoint *output)
{
    KymoStatus result = KYMO_BAD_CONFIG;
    if (output != nullptr) {
        result = KYMO_BAD_SAMPLE;
        if (frame_valid(frame, frame_length)) {
            KymoDataPoint point = {};
            decode_payload(frame, &point);
            if (point_valid(&point)) {
                *output = point;
                result = KYMO_OK;
            }
        }
    }
    return result;
}

#endif /* KYMO_ENABLE_DECODER */

#endif /* KYMO_ENABLED */
