#include "kymo.h"

#if KYMO_ENABLE_CPP

/*******************************************************************************
 * Kymo::Kymo
 ******************************************************************************/
Kymo::Kymo()
    : stream(nullptr), startTimeMs(0), useTimestamp(KYMO_ENABLE_TIMESTAMP != 0), getMillisecond(nullptr)
{
}


/*******************************************************************************
 * Kymo::Kymo
 ******************************************************************************/
Kymo::Kymo(KymoStream &outputStream, bool enableTimestamp)
    : stream(&outputStream), startTimeMs(0), useTimestamp((KYMO_ENABLE_TIMESTAMP != 0) && enableTimestamp),
      getMillisecond(nullptr)
{
}

/*******************************************************************************
 * Kymo::~Kymo
 ******************************************************************************/
Kymo::~Kymo()
{
}

/*******************************************************************************
 * Kymo::begin
 ******************************************************************************/
void Kymo::begin(KymoStream &outputStream, bool enableTimestamp)
{
    stream = &outputStream;
    useTimestamp = (KYMO_ENABLE_TIMESTAMP != 0) && enableTimestamp;
    startTimeMs = 0;
    txLength = txOffset = 0;
    txFault = false;
}


/*******************************************************************************
 * Kymo::setStartTime
 ******************************************************************************/
void Kymo::setStartTime(uint32_t startTime)
{
    startTimeMs = startTime;
}

/*******************************************************************************
 * Kymo::setMillisecondCallback
 ******************************************************************************/
void Kymo::setMillisecondCallback(GetMillisecondCallback callback)
{
    getMillisecond = callback;
}

/*******************************************************************************
 * Kymo::sendFrame
 ******************************************************************************/
bool Kymo::sendFrame(uint8_t channelId,
                        uint8_t flags,
                        float xValue,
                        float yValue,
                        float zValue,
                        bool includeTimestamp)
{
    bool accepted = false;
    if (stream && !txFault && (txOffset == txLength) &&
        emb_u8_only_bits(flags, KYMO_SUPPORTED_FLAGS) &&
        (KYMO_ENABLE_TIMESTAMP || !includeTimestamp) && !stream->busy()) {
        KymoDataPoint point = {};
        point.id = channelId;
        point.flags = flags;
#if KYMO_ENABLE_X
        point.x = xValue;
#else
        (void)xValue;
#endif
        point.value = yValue;
#if KYMO_ENABLE_Z
        point.z = zValue;
#else
        (void)zValue;
#endif
#if KYMO_ENABLE_TIMESTAMP
        if (includeTimestamp && useTimestamp && getMillisecond) {
            point.flags = EMB_U8_OR(point.flags, KYMO_FLAG_TIMESTAMP);
            point.timestamp_ms = (uint32_t)getMillisecond() - startTimeMs;
        }
#endif
        const size_t length = kymo_encode_data(&point, buffer, sizeof(buffer));
        if (length > 0u) {
            txLength = static_cast<uint8_t>(length);
            txOffset = 0;
            (void)flush();
            accepted = !txFault;
        }
    }
    return accepted;
}

/*******************************************************************************
 * Kymo::send
 ******************************************************************************/
bool Kymo::send(uint8_t channelId, float yValue, bool includeTimestamp)
{
    return sendFrame(channelId, 0, 0.0f, yValue, 0.0f, includeTimestamp);
}

/*******************************************************************************
 * Kymo::send
 ******************************************************************************/
bool Kymo::send(uint8_t channelId, float yValue)
{
    return send(channelId, yValue, false);
}

/*******************************************************************************
 * Kymo::send2D
 ******************************************************************************/
bool Kymo::send2D(uint8_t channelId, float xValue, float yValue, bool includeTimestamp)
{
    return sendFrame(channelId, KYMO_FLAG_X, xValue, yValue, 0.0f, includeTimestamp);
}

/*******************************************************************************
 * Kymo::send2D
 ******************************************************************************/
bool Kymo::send2D(uint8_t channelId, float xValue, float yValue)
{
    return send2D(channelId, xValue, yValue, false);
}

/*******************************************************************************
 * Kymo::send3D
 ******************************************************************************/
bool Kymo::send3D(uint8_t channelId,
                     float xValue,
                     float yValue,
                     float zValue,
                     bool includeTimestamp)
{
    return sendFrame(channelId,
              EMB_U8_OR(KYMO_FLAG_X, KYMO_FLAG_Z),
              xValue,
              yValue,
              zValue,
              includeTimestamp);
}

/*******************************************************************************
 * Kymo::send3D
 ******************************************************************************/
bool Kymo::send3D(uint8_t channelId, float xValue, float yValue, float zValue)
{
    return send3D(channelId, xValue, yValue, zValue, false);
}

/*******************************************************************************
 * Kymo::setTimestampEnabled
 ******************************************************************************/
void Kymo::setTimestampEnabled(bool enable)
{
    useTimestamp = (KYMO_ENABLE_TIMESTAMP != 0) && enable;
}

/*******************************************************************************
 * Kymo::isTimestampEnabled
 ******************************************************************************/
bool Kymo::isTimestampEnabled() const
{
    return useTimestamp;
}

/*******************************************************************************
 * Kymo::flush
 ******************************************************************************/
bool Kymo::flush()
{
    bool complete = false;
    if (stream && !txFault && !stream->busy()) {
        if (txOffset == txLength) {
            complete = true;
        } else {
            const size_t remaining = txLength - txOffset;
            const size_t written = stream->write(buffer + txOffset, remaining);
            if (written > remaining) {
                txFault = true;
            } else {
                txOffset = static_cast<uint8_t>(txOffset + written);
                complete = txOffset == txLength;
            }
        }
    }
    return complete;
}

#endif /* KYMO_ENABLE_CPP */
