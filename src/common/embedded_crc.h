/**
 * @brief Configurable, table-free MSB-first CRC8.
 *
 * @details No reflection or final XOR is performed. Memory consumption is constant and
 * work is bounded by eight bit iterations per input byte.
 * @file embedded_crc.h
 * @defgroup embedded_crc Common checksum
 * @{
 * @par Usage
 * Use polynomial 0x07 and initial zero for Kymo CRC-8/ATM.
 */
#ifndef EMBEDDED_COMMON_CRC_H
/**
 * @brief Include guard for embedded_crc.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include this header normally; the compiler manages this guard.
 */
#define EMBEDDED_COMMON_CRC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Accumulate an MSB-first CRC8 over a byte range.
 *
 * @details NULL is deliberately treated as an empty input regardless of length and returns
 * initial. For a non-null pointer, the caller guarantees length readable bytes.
 * A prior result can seed the next chunk; apply any protocol-specific final XOR outside.
 * @param[in] data Readable bytes, or NULL for empty-input behavior.
 * @param[in] length Number of input bytes; zero preserves initial.
 * @param[in] polynomial Low eight polynomial bits, with the x^8 term implicit.
 * @param[in] initial Initial remainder, or a previous chunk remainder.
 * @return Final raw eight-bit remainder, without reflection/xor-out.
 * @par Usage
 * uint8_t crc = emb_crc8_msb(data, length, 0x07u, 0u);
 */
static inline uint8_t emb_crc8_msb(const uint8_t *data, size_t length,
                                  uint8_t polynomial, uint8_t initial)
{
    uint8_t result = initial;
    size_t index;
    if (data != NULL) {
        for (index = 0; index < length; ++index) {
            uint8_t bit;
            result = (uint8_t)(result ^ data[index]);
            for (bit = 0; bit < 8u; ++bit) {
                uint8_t high = (uint8_t)(result & 0x80u);
                result = (uint8_t)((unsigned int)result << 1u);
                if (high != 0u) {
                    result = (uint8_t)(result ^ polynomial);
                }
            }
        }
    }
    return result;
}

/** @} */
#endif /* EMBEDDED_COMMON_CRC_H */
