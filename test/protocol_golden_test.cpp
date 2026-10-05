/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "kymo.h"

class CaptureStream : public KymoStream
{
public:
    uint8_t bytes[KYMO_FRAME_MAX_SIZE];
    size_t length;
    unsigned int writes;

    /***************************************************************************
     * CaptureStream
     **************************************************************************/
    CaptureStream() : length(0), writes(0) {}

    /***************************************************************************
     * write
     **************************************************************************/
    size_t write(const uint8_t *data, size_t data_length) override
    {
        assert(data_length <= sizeof(bytes));
        memcpy(bytes, data, data_length);
        length = data_length;
        ++writes;
        return data_length;
    }
};

/*******************************************************************************
 * main
 ******************************************************************************/
int main()
{
    static const uint8_t minimal_golden[] = {
        0xA5, 0x5A, KYMO_ENABLE_CRC ? 0x40 : 0x41, 0x07,
        0x00, 0x00, 0x80, 0x3F, KYMO_ENABLE_CRC ? 0x54 : 0};
    static const uint8_t full_golden[] = {
        0xA5, 0x5A, KYMO_ENABLE_CRC ? 0x4E : 0x4F, 0x03, 0x00, 0x00, 0xA0, 0x3F,
        0x00, 0x00, 0x20, 0xC0, 0x00, 0x00, 0x10, 0x41,
        0xD2, 0x04, 0x00, 0x00, KYMO_ENABLE_CRC ? 0x89 : 0};

    uint8_t frame[KYMO_FRAME_MAX_SIZE];
    KymoDataPoint point = {};
    KymoDataPoint decoded = {};

    point.id = 3;
    point.flags = KYMO_FLAG_X | KYMO_FLAG_Z | KYMO_FLAG_TIMESTAMP;
    point.x = 1.25f;
    point.value = -2.5f;
    point.z = 9.0f;
    point.timestamp_ms = 1234;

    size_t length = kymo_encode_data(&point, frame, sizeof(frame));
    assert(length == sizeof(full_golden));
    assert(memcmp(frame, full_golden, sizeof(full_golden)) == 0);
    assert(kymo_decode_data(frame, length, &decoded) == 1);
    assert(decoded.id == point.id);
    assert(decoded.x == point.x);
    assert(decoded.value == point.value);
    assert(decoded.z == point.z);
    assert(decoded.timestamp_ms == point.timestamp_ms);

    CaptureStream stream;
    Kymo kymo(stream);
    kymo.send(7, 1.0f);
    assert(stream.writes == 1);
    assert(stream.length == sizeof(minimal_golden));
    assert(memcmp(stream.bytes, minimal_golden, sizeof(minimal_golden)) == 0);

    puts("Embedded protocol golden vectors passed");
    return 0;
}
