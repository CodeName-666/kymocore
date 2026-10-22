/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
#include "../kymo_config.h"
#if KYMO_EFFECTIVE_PARAMETERS
#include "kymo_param.h"
#include "../common/embedded_bits.h"
#include <math.h>

/*******************************************************************************
 * entry_valid
 ******************************************************************************/
static uint8_t entry_valid(const KymoParameter *entry)
{
    uint8_t valid = (uint8_t)((entry->target != nullptr) && isfinite(entry->minimum) &&
                              isfinite(entry->maximum) && (entry->minimum <= entry->maximum));
    return valid;
}

/*******************************************************************************
 * ids_unique
 ******************************************************************************/
static uint8_t ids_unique(const KymoParameter *table, size_t count)
{
    uint8_t seen[32] = {};
    uint8_t unique = 1u;
    for (size_t index = 0u; (index < count) && unique; ++index) {
        unique = (uint8_t)!emb_bitset_test(seen, sizeof(seen), table[index].id);
        (void)emb_bitset_set(seen, sizeof(seen), table[index].id);
    }
    return unique;
}

/*******************************************************************************
 * KymoParam_Init
 ******************************************************************************/
KymoStatus KymoParam_Init(KymoParamReceiver *receiver, const KymoParameter *table,
                          size_t count)
{
    KymoStatus result = KYMO_BAD_CONFIG;
    uint8_t valid = (uint8_t)((receiver != nullptr) && (table != nullptr) && (count >= 1u) &&
                              (count <= KYMO_MAX_PARAMETERS));
    for (size_t index = 0u; (index < count) && valid; ++index) {
        valid = entry_valid(&table[index]);
    }
    if (valid && ids_unique(table, count)) {
        *receiver = KymoParamReceiver();
        receiver->table = table;
        receiver->count = count;
        result = KYMO_OK;
    } else if (receiver != nullptr) {
        *receiver = KymoParamReceiver();
    }
    return result;
}

/*******************************************************************************
 * find_index
 ******************************************************************************/
/* Returns count when the ID is not registered. */
static size_t find_index(const KymoParamReceiver *receiver, uint8_t id)
{
    size_t found = receiver->count;
    for (size_t index = 0u; (index < receiver->count) && (found == receiver->count); ++index) {
        if (receiver->table[index].id == id) {
            found = index;
        }
    }
    return found;
}

/*******************************************************************************
 * apply_frame
 ******************************************************************************/
/* A write must be CRC-protected and carry only an ID and a value. */
static size_t apply_frame(KymoParamReceiver *receiver)
{
    size_t applied = 0u;
    KymoDataPoint point = {};
    if (!emb_u8_has_any(receiver->frame[2], KYMO_DESCRIPTOR_NO_CRC) &&
        (KymoProtocol_Decode(receiver->frame, receiver->length, &point) == KYMO_OK) &&
        (point.flags == 0u)) {
        const size_t index = find_index(receiver, point.id);
        if (index == receiver->count) {
            ++receiver->rejected;
        } else {
            const KymoParameter *entry = &receiver->table[index];
            if ((point.value >= entry->minimum) && (point.value <= entry->maximum)) {
                *entry->target = point.value;
                ++receiver->accepted;
                applied = 1u;
            } else {
                ++receiver->rejected;
            }
            (void)emb_bitset_set(receiver->pending, sizeof(receiver->pending), index);
        }
    } else {
        ++receiver->invalid;
    }
    return applied;
}

/*******************************************************************************
 * restart_at
 ******************************************************************************/
/* After a lost sync, a sync byte may already start the next frame. */
static void restart_at(KymoParamReceiver *receiver, uint8_t byte)
{
    receiver->expected = 0u;
    receiver->length = (byte == KYMO_SYNC_0) ? 1u : 0u;
    receiver->frame[0] = byte;
}

/*******************************************************************************
 * feed_byte
 ******************************************************************************/
static size_t feed_byte(KymoParamReceiver *receiver, uint8_t byte)
{
    size_t applied = 0u;
    if (receiver->length == 0u) {
        restart_at(receiver, byte);
    } else if (receiver->length == 1u) {
        if (byte == KYMO_SYNC_1) {
            receiver->frame[1] = byte;
            receiver->length = 2u;
        } else {
            restart_at(receiver, byte);
        }
    } else if (receiver->length == 2u) {
        receiver->expected = KymoProtocol_FrameLength(byte);
        if (receiver->expected == 0u) {
            ++receiver->invalid;
            restart_at(receiver, byte);
        } else {
            receiver->frame[2] = byte;
            receiver->length = 3u;
        }
    } else {
        receiver->frame[receiver->length] = byte;
        ++receiver->length;
        if (receiver->length == receiver->expected) {
            applied = apply_frame(receiver);
            receiver->length = 0u;
            receiver->expected = 0u;
        }
    }
    return applied;
}

/*******************************************************************************
 * KymoParam_Feed
 ******************************************************************************/
size_t KymoParam_Feed(KymoParamReceiver *receiver, const uint8_t *bytes, size_t length)
{
    size_t applied = 0u;
    if ((receiver != nullptr) && (receiver->table != nullptr) && (bytes != nullptr)) {
        for (size_t offset = 0u; offset < length; ++offset) {
            applied += feed_byte(receiver, bytes[offset]);
        }
    }
    return applied;
}

/*******************************************************************************
 * KymoParam_NextReadback
 ******************************************************************************/
KymoStatus KymoParam_NextReadback(KymoParamReceiver *receiver, KymoDataPoint *point)
{
    KymoStatus result = KYMO_BAD_CONFIG;
    if ((receiver != nullptr) && (point != nullptr) && (receiver->table != nullptr)) {
        result = KYMO_IDLE;
        for (size_t step = 0u; (step < receiver->count) && (result == KYMO_IDLE); ++step) {
            const size_t index = (receiver->cursor + step) % receiver->count;
            if (emb_bitset_test(receiver->pending, sizeof(receiver->pending), index)) {
                (void)emb_bitset_clear(receiver->pending, sizeof(receiver->pending), index);
                *point = KymoDataPoint();
                point->id = receiver->table[index].id;
                point->value = *receiver->table[index].target;
                receiver->cursor = (index + 1u) % receiver->count;
                result = KYMO_OK;
            }
        }
    }
    return result;
}

#endif /* KYMO_EFFECTIVE_PARAMETERS */
