/**
 * @brief Optional platform-independent C++ push sender.
 *
 * @details Enable KYMO_ENABLE_CPP=1 to compile this API.
 * Use kymo_runtime.h for automatic cyclic sampling. This API accepts individual
 * measurements and retains one frame across short writes. No hardware ownership,
 * allocation or target-specific overloads are provided.
 * @file kymo.h
 * @defgroup kymo_cpp C++ sender
 * @{
 * @par Usage
 * Construct Kymo with an application stream, submit a point, and poll flush until complete.
 */
#ifndef KYMO_H
/**
 * @brief Include guard for kymo.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include this header normally; the compiler manages this guard.
 */
#define KYMO_H

#include "kymo_features.h"

#if KYMO_ENABLE_CPP || defined(DOXYGEN)
#include <stdint.h>
#include "kymo_protocol.h"
#include "kymo_stream.h"

/**
 * @brief Application clock callback used for optional C++ timestamps.
 *
 * @details No parameters or context pointer. Returns a monotonic millisecond count modulo 2^32.
 * Use a free/static uint32_t-returning wrapper around the application clock.
 * @return Current uint32 millisecond counter.
 * @par Usage
 * sender.setMillisecondCallback(application_clock_ms);
 */
typedef uint32_t (*GetMillisecondCallback)();

/**
 * @brief Single-buffer sender with application-controlled sampling.
 *
 * @details Methods are single-task and non-reentrant. The stream must outlive this object.
 * Accepted points are serialized into a 21-byte member buffer; partial writes preserve
 * the frame. A pending frame or busy transport rejects new submissions.
 * No method blocks by design, but driver callbacks may block if implemented that way.
 * Requests for fields disabled by build switches fail without submitting a frame.
 * @par Usage
 * @code{.cpp}
 * Kymo sender(outputStream);
 * bool accepted = sender.send(7, 1.0f);
 * // If accepted, call sender.flush() in later task cycles until it succeeds.
 * @endcode
 */
class Kymo
{
private:
    /** @name Internal persistent state
     * @{ */
    /**
     * @brief Borrowed output interface, NULL before attachment.
     *
     * @details Set by construction/begin; never deleted by Kymo.
     * @par Usage
     * Attach with begin(outputStream).
     */
    KymoStream *stream;
    /**
     * @brief Unsigned epoch subtracted from clock callback ticks.
     *
     * @details Defaults to zero; changing it affects only newly encoded frames.
     * @par Usage
     * Configure through setStartTime.
     */
    uint32_t startTimeMs;
    /**
     * @brief Master timestamp permission flag.
     *
     * @details A timestamp also requires a send overload requesting it and a non-null clock callback.
     * @par Usage
     * Configure through setTimestampEnabled.
     */
    bool useTimestamp;
    /**
     * @brief Persistent storage for the single encoded frame.
     *
     * @details Must remain alive and unchanged while a stream retains a pointer to it.
     * @par Usage
     * Internal only; use send/flush to manage contents.
     */
    uint8_t buffer[KYMO_FRAME_MAX_SIZE];
    /**
     * @brief Optional borrowed application clock function.
     *
     * @details NULL disables timestamp generation even when requested.
     * @par Usage
     * Configure through setMillisecondCallback.
     */
    GetMillisecondCallback getMillisecond;
    /**
     * @brief Current frame byte count.
     *
     * @details Zero before the first accepted frame; otherwise 9..21.
     * @par Usage
     * Internal transport bookkeeping; do not edit.
     */
    uint8_t txLength = 0;
    /**
     * @brief Bytes already accepted from the current frame.
     *
     * @details Updated by flush; never exceeds txLength during valid operation.
     * @par Usage
     * Internal transport bookkeeping; use flush.
     */
    uint8_t txOffset = 0;
    /**
     * @brief Latched invalid-write-count indication.
     *
     * @details Set when a stream claims more bytes than offered. Cleared by begin after driver repair.
     * @par Usage
     * Observe rejected send/flush results and recover with a safe begin.
     */
    bool txFault = false;
    /** @} */

    /**
     * @brief Validate and submit one frame through the shared implementation.
     *
     * @details Encodes only when attached, not faulted, not busy and no frame is pending.
     * Attempts one initial write; accepted partial frames require subsequent flush calls.
     * @param[in] channelId Channel identifier, 0..255.
     * @param[in] flags Optional coordinate flags; only supported protocol bits are valid.
     * @param[in] xValue Finite X when its flag is set; otherwise ignored.
     * @param[in] yValue Required finite Y/value.
     * @param[in] zValue Finite Z when its flag is set; otherwise ignored.
     * @param[in] includeTimestamp Request a timestamp subject to the master flag and clock callback.
     * @return True when queued without a driver-contract fault; false otherwise.
     * @par Usage
     * Called internally by send/send2D/send3D; applications use those public methods.
     */
    bool sendFrame(uint8_t channelId,
                   uint8_t flags,
                   float xValue,
                   float yValue,
                   float zValue,
                   bool includeTimestamp);

public:
    /** @name Lifecycle and transport binding
     * @{ */
    /**
     * @brief Construct a sender without an attached transport.
     *
     * @details Timestamp permission follows KYMO_ENABLE_TIMESTAMP; no clock is installed.
     * The epoch is zero. Timestamp requests fail when compiled without timestamp support.
     * @par Usage
     * Kymo sender; sender.begin(outputStream);
     */
    Kymo();

    /**
     * @brief Construct a sender borrowing an existing stream.
     *
     * @details The stream is not owned. Initial epoch is zero and no clock callback is installed.
     * @param[in,out] outputStream Persistent application stream used for writes/busy checks.
     * @param[in] enableTimestamp Master permission; timestamped sends still require a clock callback.
     * @par Usage
     * Kymo sender(outputStream, true);
     */
    Kymo(KymoStream &outputStream, bool enableTimestamp = true);

    /**
     * @brief Destroy the sender without flushing or cancelling the driver.
     *
     * @details The destructor does not delete the borrowed stream.
     * @par Usage
     * Stop transfers and release all borrowed buffer pointers before object destruction.
     */
    ~Kymo();

    /**
     * @brief Prohibit copying state containing transport and buffer ownership.
     *
     * @details Deleted at compile time; copying an in-flight buffer would invalidate lifetime assumptions.
     * @param[in] other Source object; copying is intentionally unavailable.
     * @par Usage
     * Create separate instances and independent driver ownership instead of copying.
     */
    Kymo(const Kymo& other) = delete;

    /**
     * @brief Prohibit assignment of sender state.
     *
     * @details Deleted at compile time so ownership cannot silently change.
     * @param[in] other Source object; assignment is intentionally unavailable.
     * @return No runtime return: this operator is deleted.
     * @par Usage
     * Use begin() to rebind an idle sender deliberately.
     */
    Kymo& operator=(const Kymo& other) = delete;

    /**
     * @brief Advance a pending short write once.
     *
     * @details Does not sample or encode new data. A ready sender with no pending bytes returns true.
     * Busy, unattached or faulted senders return false. Invalid driver counts latch a fault.
     * True reports accepted bytes, not hardware completion; busy still protects borrowed memory.
     * @return True when the current frame is fully accepted and the call is not blocked; false otherwise.
     * @par Usage
     * After an accepted send, call flush() once per application cycle until it succeeds.
     */
    bool flush();

    /**
     * @brief Attach/rebind a stream and reset pending state.
     *
     * @details Resets epoch, pending length/offset and fault flag. The installed clock callback is retained.
     * No hardware configuration or transfer cancellation is performed.
     * @param[in,out] outputStream Persistent stream to borrow after reinitialization.
     * @param[in] enableTimestamp New master timestamp permission.
     * @pre No asynchronous driver may retain the old member-buffer pointer.
     * @par Usage
     * Stop the old transport, then call sender.begin(newStream).
     */
    void begin(KymoStream &outputStream, bool enableTimestamp = true);
    /** @} */

    /** @name Timestamp configuration
     * @{ */
    /**
     * @brief Set the epoch for subsequently encoded timestamps.
     *
     * @details Unsigned subtraction supports wraparound. Pending frames retain their original timestamp.
     * @param[in] startTime Clock tick representing relative time zero in milliseconds.
     * @par Usage
     * sender.setStartTime(application_clock_ms());
     */
    void setStartTime(uint32_t startTime);

    /**
     * @brief Install or clear the millisecond clock.
     *
     * @details The callback has no user parameter and must return uint32_t; NULL suppresses timestamps.
     * @param[in] callback Application clock function pointer, or NULL.
     * @par Usage
     * sender.setMillisecondCallback(application_clock_ms);
     */
    void setMillisecondCallback(GetMillisecondCallback callback);

    /** @} */

    /** @name Measurement submission
     * @{ */
    /**
     * @brief Submit Y/value with optional timestamp.
     *
     * @details Accepts one point only when the previous frame is accepted and the stream is ready.
     * After true, poll flush() rather than resubmitting the same point. No sample queue exists.
     * Timestamp presence requires includeTimestamp, enabled permission and an installed clock.
     * @param[in] channelId Channel identifier in 0..255.
     * @param[in] yValue Finite Y/value coordinate.
     * @param[in] includeTimestamp Request a relative timestamp if permission and clock callback are available.
     * @return True if the new point was queued without a write-count fault; false if rejected.
     * @par Usage
     * sender.send(0, 1.0f, true);
     */
    bool send(uint8_t channelId, float yValue, bool includeTimestamp);

    /**
     * @brief Submit Y/value without a timestamp.
     *
     * @details Accepts one point only when the previous frame is accepted and the stream is ready.
     * After true, poll flush() rather than resubmitting the same point. No sample queue exists.
     * This overload always omits timestamps, regardless of the master timestamp setting.
     * @param[in] channelId Channel identifier in 0..255.
     * @param[in] yValue Finite Y/value coordinate.
     * @return True if the new point was queued without a write-count fault; false if rejected.
     * @par Usage
     * sender.send(0, 1.0f);
     */
    bool send(uint8_t channelId, float yValue);

    /**
     * @brief Submit X/Y/value with optional timestamp.
     *
     * @details Accepts one point only when the previous frame is accepted and the stream is ready.
     * After true, poll flush() rather than resubmitting the same point. No sample queue exists.
     * Timestamp presence requires includeTimestamp, enabled permission and an installed clock.
     * @param[in] channelId Channel identifier in 0..255.
     * @param[in] xValue Finite X coordinate.
     * @param[in] yValue Finite Y/value coordinate.
     * @param[in] includeTimestamp Request a relative timestamp if permission and clock callback are available.
     * @return True if the new point was queued without a write-count fault; false if rejected.
     * @par Usage
     * sender.send2D(0, 1.0f, 1.0f, true);
     */
    bool send2D(uint8_t channelId, float xValue, float yValue, bool includeTimestamp);

    /**
     * @brief Submit X/Y/value without a timestamp.
     *
     * @details Accepts one point only when the previous frame is accepted and the stream is ready.
     * After true, poll flush() rather than resubmitting the same point. No sample queue exists.
     * This overload always omits timestamps, regardless of the master timestamp setting.
     * @param[in] channelId Channel identifier in 0..255.
     * @param[in] xValue Finite X coordinate.
     * @param[in] yValue Finite Y/value coordinate.
     * @return True if the new point was queued without a write-count fault; false if rejected.
     * @par Usage
     * sender.send2D(0, 1.0f, 1.0f);
     */
    bool send2D(uint8_t channelId, float xValue, float yValue);

    /**
     * @brief Submit X/Y/value/Z with optional timestamp.
     *
     * @details Accepts one point only when the previous frame is accepted and the stream is ready.
     * After true, poll flush() rather than resubmitting the same point. No sample queue exists.
     * Timestamp presence requires includeTimestamp, enabled permission and an installed clock.
     * @param[in] channelId Channel identifier in 0..255.
     * @param[in] xValue Finite X coordinate.
     * @param[in] yValue Finite Y/value coordinate.
     * @param[in] zValue Finite Z coordinate.
     * @param[in] includeTimestamp Request a relative timestamp if permission and clock callback are available.
     * @return True if the new point was queued without a write-count fault; false if rejected.
     * @par Usage
     * sender.send3D(0, 1.0f, 1.0f, 1.0f, true);
     */
    bool send3D(uint8_t channelId, float xValue, float yValue, float zValue, bool includeTimestamp);

    /**
     * @brief Submit X/Y/value/Z without a timestamp.
     *
     * @details Accepts one point only when the previous frame is accepted and the stream is ready.
     * After true, poll flush() rather than resubmitting the same point. No sample queue exists.
     * This overload always omits timestamps, regardless of the master timestamp setting.
     * @param[in] channelId Channel identifier in 0..255.
     * @param[in] xValue Finite X coordinate.
     * @param[in] yValue Finite Y/value coordinate.
     * @param[in] zValue Finite Z coordinate.
     * @return True if the new point was queued without a write-count fault; false if rejected.
     * @par Usage
     * sender.send3D(0, 1.0f, 1.0f, 1.0f);
     */
    bool send3D(uint8_t channelId, float xValue, float yValue, float zValue);

    /** @} */

    /** @name Timestamp permission
     * @{ */
    /**
     * @brief Enable or disable future requested timestamps.
     *
     * @details Does not change already encoded frames or install a clock callback.
     * Permission remains false if KYMO_ENABLE_TIMESTAMP is zero.
     * @param[in] enable True permits requested timestamps; false suppresses them.
     * @par Usage
     * sender.setTimestampEnabled(false);
     */
    void setTimestampEnabled(bool enable);

    /**
     * @brief Read the master timestamp permission.
     *
     * @details This does not indicate whether a clock is installed or a particular frame contains time.
     * @return Current master permission flag.
     * @par Usage
     * Use for configuration diagnostics, not transport completion checks.
     */
    bool isTimestampEnabled() const;
    /** @} */
};

/** @} */
#endif /* KYMO_ENABLE_CPP */
#endif /* KYMO_H */
