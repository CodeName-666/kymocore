/**
 * @brief Portable codec for Kymo binary protocol v6.
 *
 * @details Frames contain sync, descriptor, channel, optional X, mandatory Y, optional Z,
 * optional timestamp and a CRC/trailer byte. CRC is disabled by default.
 * Multi-byte payloads are little-endian. The codec
 * allocates no memory and never transmits the native structure layout.
 * @file kymo_protocol.h
 * @defgroup kymo_protocol Wire protocol and codec
 * @{
 * @par Usage
 * Include this header when encoding or decoding complete frames.
 * @code{.c}
 * KymoDataPoint point = {0};
 * uint8_t frame[KYMO_FRAME_MAX_SIZE];
 * point.id = 7; point.value = 1.0f;
 * size_t length = kymo_encode_data(&point, frame, sizeof(frame));
 * @endcode
 */
#ifndef KYMO_PROTOCOL_H
/**
 * @brief Include guard for kymo_protocol.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include this header normally; the compiler manages this guard.
 */
#define KYMO_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "common/embedded_bits.h"
#include "kymo_features.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @name Protocol constants
 * @{ */
/**
 * @brief Human-facing major protocol revision.
 *
 * @details Revision 6 names the compact binary protocol; this is not the descriptor version field.
 * @par Usage
 * Use for diagnostics or firmware compatibility information.
 */
#define KYMO_PROTOCOL_VERSION_MAJOR 6

/**
 * @brief Binary descriptor version before placement in bits 7..6.
 *
 * @details Version 1 is encoded by KYMO_DESCRIPTOR_DATA. Other versions are rejected.
 * @par Usage
 * Compare supported protocol versions without changing wire constants.
 */
#define KYMO_WIRE_VERSION 1

/**
 * @brief Descriptor bit indicating that CRC was not calculated.
 *
 * @details Set by the encoder when KYMO_ENABLE_CRC is zero. Frame length is
 * unchanged; the sender writes a zero trailer and receivers ignore that byte.
 * This wire bit is not a payload flag in KymoDataPoint or KymoChannel.
 * @par Usage
 * Inspect descriptor & KYMO_DESCRIPTOR_NO_CRC to identify unprotected frames.
 */
#define KYMO_DESCRIPTOR_NO_CRC 0x01u

/**
 * @brief First synchronization byte at frame offset zero.
 *
 * @details Followed by KYMO_SYNC_1. Synchronization bytes are excluded from the CRC.
 * @par Usage
 * Search byte streams for the two-byte sync marker before framing.
 */
#define KYMO_SYNC_0 0xA5u

/**
 * @brief Second synchronization byte at frame offset one.
 *
 * @details Must immediately follow KYMO_SYNC_0; neither byte is a text delimiter.
 * @par Usage
 * Use with KYMO_SYNC_0 when locating a candidate frame.
 */
#define KYMO_SYNC_1 0x5Au

/**
 * @brief Base descriptor for wire version 1 and a data message.
 *
 * @details Bits 7..6 contain 01; message-type bits 5..4 contain 00. Optional flags are combined with this byte.
 * @par Usage
 * The encoder combines this base with KymoDataPoint::flags.
 */
#define KYMO_DESCRIPTOR_DATA 0x40u

/**
 * @brief Descriptor bit indicating a four-byte X coordinate.
 *
 * @details Adds one finite binary32 value before the mandatory Y/value field.
 * @par Usage
 * Set in KymoChannel::flags or KymoDataPoint::flags for XY or XYZ measurements.
 */
#define KYMO_FLAG_X 0x08u

/**
 * @brief Descriptor bit indicating a four-byte Z coordinate.
 *
 * @details Adds a finite binary32 value after Y. Z can also be used without X.
 * @par Usage
 * Combine with KYMO_FLAG_X for three-dimensional measurements.
 */
#define KYMO_FLAG_Z 0x04u

/**
 * @brief Descriptor bit indicating relative uint32 milliseconds.
 *
 * @details Adds four bytes after all coordinates. The counter wraps modulo 2^32; the app converts it to seconds.
 * @par Usage
 * Set on a channel to include time relative to Kymo_Init.
 */
#define KYMO_FLAG_TIMESTAMP 0x02u

/**
 * @brief Mask of every supported optional payload field.
 *
 * @details Only X, Z and timestamp flags are permitted on the wire. Build-time
 * feature switches further restrict usable combinations through KYMO_SUPPORTED_FLAGS.
 * @par Usage
 * Use for XYZ with timestamps or to validate a supplied flag byte.
 */
#define KYMO_ALLOWED_FLAGS EMB_U8_OR(KYMO_FLAG_X, EMB_U8_OR(KYMO_FLAG_Z, KYMO_FLAG_TIMESTAMP))

/**
 * @brief Payload flags compiled into this library build.
 * @details Wire constants remain unchanged; requests outside this mask fail.
 * Zero selects the default scalar-only build. No fields are silently removed.
 * @par Usage
 * Validate application channel flags against KYMO_SUPPORTED_FLAGS.
 */
#define KYMO_SUPPORTED_FLAGS EMB_U8_OR( \
    (KYMO_ENABLE_X ? KYMO_FLAG_X : 0u), EMB_U8_OR( \
    (KYMO_ENABLE_Z ? KYMO_FLAG_Z : 0u), \
    (KYMO_ENABLE_TIMESTAMP ? KYMO_FLAG_TIMESTAMP : 0u)))


/**
 * @brief Mask selecting the two wire-version bits.
 *
 * @details The masked descriptor must equal KYMO_DESCRIPTOR_DATA for a supported frame.
 * @par Usage
 * Used by kymo_frame_length before accepting a descriptor.
 */
#define KYMO_DESCRIPTOR_VERSION_MASK 0xC0u

/**
 * @brief Mask selecting unsupported message-type bits.
 *
 * @details Combines bits 5 and 4. Bit 0 selects NO_CRC and is supported.
 * @par Usage
 * Validate descriptor bytes with emb_u8_has_any.
 */
#define KYMO_DESCRIPTOR_RESERVED_MASK 0x30u

/**
 * @brief Size in bytes of a Y-only frame.
 *
 * @details Two sync bytes, descriptor, channel, binary32 Y and CRC/zero trailer.
 * @par Usage
 * Minimum complete-frame bound for decoding.
 */
#define KYMO_FRAME_MIN_SIZE 9u

/**
 * @brief Maximum complete-frame size in bytes.
 *
 * @details Includes X, Y, Z, timestamp and framing. A single buffer of this size supports all flag combinations.
 * @par Usage
 * Declare uint8_t frame[KYMO_FRAME_MAX_SIZE].
 */
#define KYMO_FRAME_MAX_SIZE 21u

/** @} */

/**
 * @brief Native application representation of one measurement.
 *
 * @details Input to the encoder and output of successful decoding. Padding and host byte order
 * are never sent. Zero-initialize before filling fields; absent decoded fields become zero.
 * All selected coordinates must be finite IEEE-754 binary32 values.
 * @par Usage
 * Set id/value and optional flags/fields, then call kymo_encode_data.
 */
typedef struct
{
    /**
     * @brief Channel identifier in the inclusive range 0..255.
     *
     * @details Input to encoding; output from decoding.
     * @par Usage
     * point.id = 7;
     */
    uint8_t id;
    /**
     * @brief Presence flags for X, Z and timestamp.
     *
     * @details Input/output; only KYMO_SUPPORTED_FLAGS bits are accepted by this build.
     * Zero selects Y only. Wire flags remain defined even when their features are disabled.
     * @par Usage
     * point.flags = KYMO_ALLOWED_FLAGS;
     */
    uint8_t flags;
    /**
     * @brief Optional X coordinate.
     *
     * @details Input/output; relevant only when KYMO_FLAG_X is set. Decoded absent X is zero.
     * @par Usage
     * Set point.x for Cartesian measurements.
     */
    float x;
    /**
     * @brief Mandatory Y coordinate or scalar measurement.
     *
     * @details Input/output; must be finite even when no optional fields are selected.
     * @par Usage
     * point.value = temperature;
     */
    float value;
    /**
     * @brief Optional Z coordinate.
     *
     * @details Input/output; relevant only when KYMO_FLAG_Z is set. Decoded absent Z is zero.
     * @par Usage
     * Set point.z for XYZ or YZ measurements.
     */
    float z;
    /**
     * @brief Optional timestamp in milliseconds relative to the sender epoch.
     *
     * @details Input/output; used only with KYMO_FLAG_TIMESTAMP. Wraps modulo 2^32.
     * @par Usage
     * Use elapsed milliseconds rather than Unix time.
     */
    uint32_t timestamp_ms;
} KymoDataPoint;

/** @name Codec operations
 * @{ */
#if KYMO_ENABLE_CRC || KYMO_ENABLE_DECODER || defined(DOXYGEN)
/**
 * @brief Calculate CRC-8/ATM over a byte range.
 *
 * @details Polynomial 0x07, initial zero, no reflection and no final XOR. There are eight
 * bit iterations per input byte. Null data is treated as empty and produces zero.
 * Compiled only when encoder CRC or the decoder is enabled.
 * @param[in] data Readable byte range; may be NULL for the defined empty-input behavior.
 * @param[in] length Number of bytes to read; must fit the actual data allocation when data is non-null.
 * @return CRC byte in 0..255.
 * @par Usage
 * kymo_crc8(frame + 2, frame_length - 3) covers descriptor through payload.
 */
uint8_t kymo_crc8(const uint8_t *data, size_t length);
#endif

/**
 * @brief Encode a validated point into a caller-owned byte buffer.
 *
 * @details No allocation, text conversion or struct casting. Validation failures leave the output
 * buffer unchanged. Disabled payload flags are rejected via KYMO_SUPPORTED_FLAGS.
 * Length depends only on the selected optional fields.
 * KYMO_ENABLE_CRC selects protected frames (1) or NO_CRC with zero trailer (0).
 * @param[in] point Measurement to encode; NULL is rejected. Selected coordinates must be finite.
 * @param[out] output Writable frame buffer; NULL is rejected. Must not overlap point.
 * @param[in] output_capacity Actual writable capacity in bytes; 21 bytes handles every point.
 * @return Encoded length (9, 13, 17 or 21), or zero for invalid input/insufficient capacity.
 * @par Usage
 * size_t n = kymo_encode_data(&point, frame, sizeof(frame));
 * Transmit only frame[0..n-1] when n is nonzero.
 */
size_t kymo_encode_data(const KymoDataPoint *point,
                           uint8_t *output,
                           size_t output_capacity);

#if KYMO_ENABLE_DECODER || defined(DOXYGEN)
/**
 * @brief Validate and decode exactly one complete binary frame.
 *
 * @details Checks synchronization, descriptor, exact length and finite coordinates.
 * CRC is checked unless the wire descriptor sets NO_CRC; its trailer is ignored.
 * Available with KYMO_ENABLE_DECODER=1. Both CRC modes are accepted regardless
 * of encoder CRC setting; disabled X/Z/timestamp layouts are rejected.
 * Only a fully valid result is committed; failure leaves output unchanged. The caller
 * performs stream reassembly before calling this function.
 * @param[in] frame Readable frame bytes; NULL is rejected.
 * @param[in] frame_length Actual readable length; must match the descriptor-derived size exactly.
 * @param[out] output Destination measurement, or NULL to reject. Unselected fields are zero on success.
 * @return One on success; zero on validation failure.
 * @par Usage
 * Consume point only when kymo_decode_data(frame, length, &point) returns nonzero.
 */
int kymo_decode_data(const uint8_t *frame,
                        size_t frame_length,
                        KymoDataPoint *output);
#endif

/**
 * @brief Derive complete-frame length from a supported descriptor.
 *
 * @details Unsupported versions, message types or disabled payload fields yield zero.
 * No payload access is performed; the NO_CRC bit does not change frame length.
 * @param[in] descriptor Descriptor byte from offset two of a candidate frame.
 * @return 9, 13, 17 or 21 bytes for supported layouts; zero otherwise.
 * @par Usage
 * Wait for kymo_frame_length(descriptor) bytes before complete-frame decoding.
 */
size_t kymo_frame_length(uint8_t descriptor);
/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* KYMO_PROTOCOL_H */
