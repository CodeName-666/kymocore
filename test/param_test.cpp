/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/* Behavior tests for the parameter back channel (KymoParam_*). */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "KymoCore.h"

static float sKp = 1.0f;
static float sSetpoint = 20.0f;
static float sGain = 0.5f;
static const KymoParameter sTable[] = {
    { 7u, &sKp, 0.0f, 10.0f },
    { 100u, &sSetpoint, -40.0f, 125.0f },
    { 101u, &sGain, 0.0f, 1.0f },
};

/*******************************************************************************
 * writeFrame
 ******************************************************************************/
/* Built by hand, independent of the encoder's CRC setting. */
static size_t writeFrame(uint8_t *frame, uint8_t id, float value, uint8_t withCrc)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    frame[0] = KYMO_SYNC_0;
    frame[1] = KYMO_SYNC_1;
    frame[2] = withCrc ? KYMO_DESCRIPTOR_DATA : (uint8_t)(KYMO_DESCRIPTOR_DATA | KYMO_DESCRIPTOR_NO_CRC);
    frame[3] = id;
    for (unsigned index = 0u; index < 4u; ++index) {
        frame[4u + index] = (uint8_t)(bits >> (8u * index));
    }
    frame[8] = withCrc ? KymoProtocol_Crc8(frame + 2, 6u) : 0u;
    return 9u;
}

/*******************************************************************************
 * resetValues
 ******************************************************************************/
static void resetValues(KymoParamReceiver *receiver)
{
    sKp = 1.0f;
    sSetpoint = 20.0f;
    sGain = 0.5f;
    assert(KymoParam_Init(receiver, sTable, 3u) == KYMO_OK);
}

/*******************************************************************************
 * testInitValidation
 ******************************************************************************/
static void testInitValidation(void)
{
    KymoParamReceiver receiver;
    float value = 0.0f;
    const KymoParameter nullTarget[] = { { 1u, nullptr, 0.0f, 1.0f } };
    const KymoParameter inverted[] = { { 1u, &value, 2.0f, 1.0f } };
    const KymoParameter notFinite[] = { { 1u, &value, 0.0f, (float)INFINITY } };
    const KymoParameter duplicate[] = { { 1u, &value, 0.0f, 1.0f }, { 1u, &value, 0.0f, 1.0f } };
    KymoParameter tooMany[KYMO_MAX_PARAMETERS + 1u];
    uint8_t frame[KYMO_FRAME_MAX_SIZE];
    KymoDataPoint point;
    for (unsigned index = 0u; index <= KYMO_MAX_PARAMETERS; ++index) {
        tooMany[index] = KymoParameter{ (uint8_t)index, &value, 0.0f, 1.0f };
    }
    assert(KymoParam_Init(nullptr, sTable, 3u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, nullptr, 3u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, sTable, 0u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, nullTarget, 1u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, inverted, 1u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, notFinite, 1u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, duplicate, 2u) == KYMO_BAD_CONFIG);
    assert(KymoParam_Init(&receiver, tooMany, KYMO_MAX_PARAMETERS + 1u) == KYMO_BAD_CONFIG);
    /* A failed Init leaves an unusable receiver, never a half-configured one. */
    assert(KymoParam_Feed(&receiver, frame, writeFrame(frame, 1u, 0.5f, 1u)) == 0u);
    assert(KymoParam_NextReadback(&receiver, &point) == KYMO_BAD_CONFIG);
    assert(KymoParam_NextReadback(nullptr, &point) == KYMO_BAD_CONFIG);
    assert(KymoParam_Feed(nullptr, frame, 9u) == 0u);
}

/*******************************************************************************
 * testAcceptedWriteAndReadback
 ******************************************************************************/
static void testAcceptedWriteAndReadback(void)
{
    KymoParamReceiver receiver;
    /* KymoStudio's encode_data_point(PlotDataPoint(id=7, value=1.0), crc_enabled=True). */
    const uint8_t studio[] = { 0xA5u, 0x5Au, 0x40u, 0x07u, 0x00u, 0x00u, 0x80u, 0x3Fu, 0x54u };
    uint8_t frame[KYMO_FRAME_MAX_SIZE];
    KymoDataPoint point;
    resetValues(&receiver);
    sKp = 3.0f;
    assert(KymoParam_Feed(&receiver, studio, sizeof(studio)) == 1u);
    assert(sKp == 1.0f);
    assert(KymoParam_NextReadback(&receiver, &point) == KYMO_OK);
    assert((point.id == 7u) && (point.value == 1.0f) && (point.flags == 0u));
    assert(KymoParam_NextReadback(&receiver, &point) == KYMO_IDLE);
    /* Byte by byte, as from a slow UART. */
    const size_t length = writeFrame(frame, 100u, 85.5f, 1u);
    size_t applied = 0u;
    for (size_t index = 0u; index < length; ++index) {
        applied += KymoParam_Feed(&receiver, frame + index, 1u);
    }
    assert((applied == 1u) && (sSetpoint == 85.5f) && (receiver.accepted == 2u));
    assert(KymoParam_NextReadback(&receiver, &point) == KYMO_OK);
    assert((point.id == 100u) && (point.value == 85.5f));
}

/*******************************************************************************
 * testRefusedWrites
 ******************************************************************************/
static void testRefusedWrites(void)
{
    KymoParamReceiver receiver;
    uint8_t frame[KYMO_FRAME_MAX_SIZE];
    KymoDataPoint point;
    size_t length = 0u;
    resetValues(&receiver);
    /* Out of range: unchanged, but the readback reports the value kept. */
    length = writeFrame(frame, 7u, 10.5f, 1u);
    assert(KymoParam_Feed(&receiver, frame, length) == 0u);
    assert((sKp == 1.0f) && (receiver.rejected == 1u));
    assert(KymoParam_NextReadback(&receiver, &point) == KYMO_OK);
    assert((point.id == 7u) && (point.value == 1.0f));
    /* Unknown ID: refused without a readback. */
    length = writeFrame(frame, 55u, 1.0f, 1u);
    assert(KymoParam_Feed(&receiver, frame, length) == 0u);
    assert((receiver.rejected == 2u) && (KymoParam_NextReadback(&receiver, &point) == KYMO_IDLE));
    /* No CRC, a corrupted CRC or a non-finite value: discarded. */
    length = writeFrame(frame, 7u, 2.0f, 0u);
    assert(KymoParam_Feed(&receiver, frame, length) == 0u);
    length = writeFrame(frame, 7u, 2.0f, 1u);
    frame[8] = (uint8_t)(frame[8] ^ 0x01u);
    assert(KymoParam_Feed(&receiver, frame, length) == 0u);
    length = writeFrame(frame, 7u, (float)NAN, 1u);
    assert(KymoParam_Feed(&receiver, frame, length) == 0u);
    assert((sKp == 1.0f) && (receiver.invalid == 3u));
    assert(KymoParam_NextReadback(&receiver, &point) == KYMO_IDLE);
    /* A write with a timestamp field is not a plain write. */
    uint8_t stamped[13] = { KYMO_SYNC_0, KYMO_SYNC_1, (uint8_t)(KYMO_DESCRIPTOR_DATA | KYMO_FLAG_TIMESTAMP),
                            7u, 0x00u, 0x00u, 0x00u, 0x40u, 1u, 0u, 0u, 0u, 0u };
    stamped[12] = KymoProtocol_Crc8(stamped + 2, 10u);
    (void)KymoParam_Feed(&receiver, stamped, sizeof(stamped));
    assert((sKp == 1.0f) && (receiver.invalid == 4u));
}

/*******************************************************************************
 * testResynchronization
 ******************************************************************************/
static void testResynchronization(void)
{
    KymoParamReceiver receiver;
    uint8_t stream[64] = {};
    size_t length = 0u;
    resetValues(&receiver);
    /* Noise, a lone sync byte, a doubled sync byte and a bad descriptor before two writes. */
    const uint8_t noise[] = { 0x00u, 0xFFu, 0xA5u, 0x00u, 0xA5u, 0xA5u, 0x5Au, 0x33u, 0x5Au };
    memcpy(stream, noise, sizeof(noise));
    length = sizeof(noise);
    length += writeFrame(stream + length, 101u, 0.25f, 1u);
    stream[length++] = 0xA5u;  /* sync byte right before the next frame */
    length += writeFrame(stream + length, 7u, 4.0f, 1u);
    assert(KymoParam_Feed(&receiver, stream, length) == 2u);
    assert((sGain == 0.25f) && (sKp == 4.0f));
    /* Readbacks rotate through the table. */
    KymoDataPoint first;
    KymoDataPoint second;
    assert(KymoParam_NextReadback(&receiver, &first) == KYMO_OK);
    assert(KymoParam_NextReadback(&receiver, &second) == KYMO_OK);
    assert((first.id == 7u) && (second.id == 101u));
    assert(KymoParam_Feed(&receiver, nullptr, 5u) == 0u);
}

/*******************************************************************************
 * main
 ******************************************************************************/
int main(void)
{
    testInitValidation();
    testAcceptedWriteAndReadback();
    testRefusedWrites();
    testResynchronization();
    printf("parameter back channel: PASS\n");
    return 0;
}
