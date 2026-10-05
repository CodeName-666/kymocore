/**
 * @brief Platform-neutral C++ transport abstraction.
 *
 * @details Available with KYMO_ENABLE_CPP=1. Applications implement transport
 * policy outside the library. The interface
 * owns no driver, allocates no storage and contains no platform-dependent types.
 * @file kymo_stream.h
 * @defgroup kymo_stream C++ transport contract
 * @{
 * @par Usage
 * Derive a stream, implement write/busy, then pass the persistent instance to Kymo.
 */
#ifndef KYMO_STREAM_H
/**
 * @brief Include guard for kymo_stream.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include this header normally; the compiler manages this guard.
 */
#define KYMO_STREAM_H

#include "kymo_features.h"

#if KYMO_ENABLE_CPP || defined(DOXYGEN)
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Application-owned binary output with explicit buffer lifetime.
 *
 * @details One producer must serialize writes to a stream. Stream and driver state must outlive
 * all senders using them. Asynchronous implementations must override busy(); the default
 * requires write() to copy/consume input before returning.
 * @par Usage
 * @code{.cpp}
 * class MyStream : public KymoStream {
 * public:
 *     size_t write(const uint8_t *data, size_t length) override;
 *     bool busy() const override;
 * };
 * @endcode
 */
class KymoStream {
public:
    /**
     * @brief Accept a prefix of the supplied binary frame.
     *
     * @details Called only when busy() is false. Zero indicates backpressure, not success.
     * Message-based drivers must accept the entire frame or zero. A borrowing driver must
     * report busy until the span is released and must never modify the bytes.
     * @param[in] data Non-null readable span owned by the sender.
     * @param[in] length Requested byte count; Kymo passes 1..21 bytes.
     * @return Accepted count in 0..length. Larger results latch a fault in Kymo.
     * @par Usage
     * Override to forward data to the application driver; report only consumed/accepted bytes.
     */
    virtual size_t write(const uint8_t* data, size_t length) = 0;

    /**
     * @brief Report driver availability and outstanding buffer ownership.
     *
     * @details Default false is suitable only for drivers that copy/consume before returning.
     * Override for DMA, USB and other borrowed-buffer interfaces. Query must not modify
     * the sender or recursively call it.
     * @return True while unavailable or retaining a transmit pointer; false when another write/reuse is allowed.
     * @par Usage
     * Override with the driver completion/connection check.
     */
    virtual bool busy() const { return false; }

    /**
     * @brief Destroy the stream interface without controlling hardware.
     *
     * @details Performs no implicit flush, cancellation or driver shutdown.
     * @par Usage
     * Ensure no sender uses this stream and no transfer retains its buffers before destruction.
     */
    virtual ~KymoStream() {}
};

/** @} */
#endif /* KYMO_ENABLE_CPP */
#endif /* KYMO_STREAM_H */
