/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
 * Copyright (c) 2026 Christof Seidel */
/**
 * @brief Static, cooperative Init/Main telemetry runtime.
 *
 * @details The application owns all configuration, state and hardware. The runtime serializes
 * one measurement at a time into a fixed byte buffer. Calls must be single-task and
 * non-reentrant; synchronize values shared with ISRs outside this library.
 * @file kymo_runtime.h
 * @defgroup kymo_runtime Cyclic runtime and configuration
 * @{
 * @par Usage
 * Provide a persistent KymoConfig, call Kymo_Init once, then call Kymo_Main cyclically.
 */
#ifndef KYMO_RUNTIME_H
/**
 * @brief Include guard for kymo_runtime.h.
 *
 * @details Internal preprocessor symbol; do not define it in application code.
 * @par Usage
 * Include this header normally; the compiler manages this guard.
 */
#define KYMO_RUNTIME_H

#include "kymo_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @name Results
 * @{ */
/**
 * @brief Byte-sized result type shared by Init and Main.
 *
 * @details Uses named integer constants instead of an enum with compiler-dependent storage width.
 * @par Usage
 * KymoStatus status = Kymo_Main(&context);
 */
typedef uint8_t KymoStatus;

/**
 * @brief Initialization succeeded or a complete frame was accepted.
 *
 * @details Main success means driver acceptance, not physical completion or acknowledgement.
 * @par Usage
 * Compare the return of Init or Main against this constant.
 */
#define KYMO_OK 0u

/**
 * @brief No configured channel was due during this Main call.
 *
 * @details Neither a measurement nor a transmission was started; service may still have run.
 * @par Usage
 * Continue the cyclic task without an immediate retry loop.
 */
#define KYMO_IDLE 1u

/**
 * @brief Transport is busy or only part of the current frame was accepted.
 *
 * @details Pending bytes and sample timestamp are preserved. No additional sample is taken.
 * @par Usage
 * Call Main again later to progress transmission.
 */
#define KYMO_BUSY 2u

/**
 * @brief The selected sample callback reported no measurement.
 *
 * @details The channel period is consumed; other channels retain round-robin fairness.
 * @par Usage
 * Wait for a subsequent scheduled attempt; do not enqueue a replacement frame.
 */
#define KYMO_SKIPPED 3u

/**
 * @brief A required coordinate is not finite.
 *
 * @details The measurement is skipped and is not transmitted. This is not a latched fault.
 * @par Usage
 * Check the application sensor data; the next cycle can continue normally.
 */
#define KYMO_BAD_SAMPLE 4u

/**
 * @brief Configuration/context is missing or initialization validation failed.
 *
 * @details For a writable context, failed Init leaves it disabled. Zero-initialized contexts
 * produce this result from Main until initialized. Uninitialized automatic storage is not valid input.
 * @par Usage
 * Correct configuration and call Init before Main.
 */
#define KYMO_BAD_CONFIG 5u

/**
 * @brief A write callback reported more bytes than requested.
 *
 * @details The fault is latched because the actual stream position cannot be inferred.
 * Optional service still runs, but no further samples or writes occur.
 * @par Usage
 * Repair/reset the driver, ensure no borrowed buffer remains, then reinitialize.
 */
#define KYMO_IO_ERROR 6u

/** @} */

/** @name Channel and sample data
 * @{ */
/**
 * @brief Immutable scheduling and wire selection for one channel.
 *
 * @details Input configuration. Channel IDs must be unique within one KymoConfig.
 * The structure is naturally aligned and is never transmitted directly.
 * @par Usage
 * static const KymoChannel channel = {20, 0, KYMO_FLAG_TIMESTAMP};
 */
typedef struct {
    /**
     * @brief Requested interval in milliseconds, 0..INT32_MAX.
     *
     * @details Zero means every eligible Main call; positive values use unsigned elapsed-time comparison.
     * Backpressure may delay sampling; missed samples are not replayed.
     * @par Usage
     * Use 20 for a nominal 50 Hz channel.
     */
    uint32_t period_ms;
    /**
     * @brief Application channel ID, 0..255.
     *
     * @details Passed to the sample callback and encoded into the frame. Duplicate IDs invalidate Init.
     * @par Usage
     * Map this ID to a sensor or application parameter in the sample callback.
     */
    uint8_t id;
    /**
     * @brief Optional wire fields for this channel.
     *
     * @details Input; only KYMO_SUPPORTED_FLAGS bits may be set in this build.
     * Disabled fields produce KYMO_BAD_CONFIG at Init.
     * @par Usage
     * Use zero for scalar Y, or KYMO_ALLOWED_FLAGS for full XYZ/time.
     */
    uint8_t flags;
} KymoChannel;

/**
 * @brief Coordinates written by the application sample callback.
 *
 * @details Output of KymoSampleFn, input to frame encoding. Main zero-initializes this
 * object for each attempt. There is no channel ID or timestamp here: runtime supplies them.
 * @par Usage
 * sample->value = sensor_value; then return nonzero from the callback.
 */
typedef struct {
    /**
     * @brief Optional X coordinate.
     *
     * @details Callback output; must be finite if the channel enables X. Otherwise ignored.
     * @par Usage
     * Assign position, angle or an independent-variable coordinate.
     */
    float x;
    /**
     * @brief Required scalar/Y measurement.
     *
     * @details Callback output; must be finite for every channel. Unassigned values remain zero.
     * @par Usage
     * Assign the current application measurement.
     */
    float value;
    /**
     * @brief Optional Z coordinate.
     *
     * @details Callback output; must be finite when Z is enabled. Otherwise ignored.
     * @par Usage
     * Assign the third coordinate for XYZ telemetry.
     */
    float z;
} KymoSample;

/** @} */

/** @name Application callbacks
 * @{ */
/**
 * @brief Read a monotonic millisecond counter.
 *
 * @details Input/output context belongs to the application. The counter may wrap modulo 2^32.
 * Init captures an epoch; Main computes unsigned time differences. Use a consistent clock
 * for an instance and schedule Main at least once within INT32_MAX milliseconds.
 * @param[in,out] user Opaque clock_user pointer; may be NULL if unused.
 * @return Current uint32 millisecond tick.
 * @par Usage
 * Configure clock_ms and clock_user; wrap the SDK tick function without exposing SDK types.
 */
typedef uint32_t (*KymoClockFn)(void *user);

/**
 * @brief Obtain a current application measurement for a selected channel.
 *
 * @details Called at most once per Main. The runtime supplies zeroed output storage valid only
 * for this call. Do not retain the sample pointer or call Main recursively.
 * @param[in,out] user Application-owned sample_user context; may be NULL if unused.
 * @param[in] id Configured channel ID selecting the source measurement.
 * @param[out] sample Non-null, zero-initialized destination for the required coordinates.
 * @return Nonzero if a measurement is available; zero to skip until the next scheduled period.
 * @par Usage
 * Switch on id, copy application values into sample, then return one on success.
 */
typedef uint8_t (*KymoSampleFn)(void *user, uint8_t id, KymoSample *sample);

/**
 * @brief Accept a prefix of the pending binary frame.
 *
 * @details Zero applies backpressure; short writes resume at the remaining offset. Never consume
 * bytes that are reported as unaccepted. Synchronous drivers copy/consume before return.
 * Async drivers may retain bytes only while busy reports nonzero; release before reinit.
 * Message transports such as MQTT/CAN-FD accept the entire frame or zero.
 * @param[in,out] user Mutable transport_user driver context; may be NULL if unused.
 * @param[in] bytes Non-null readable span owned by runtime; must not be modified.
 * @param[in] length Requested byte count, 1..21. No access beyond this span.
 * @return Accepted byte count in 0..length. Larger counts latch KYMO_IO_ERROR.
 * @par Usage
 * Forward bytes to a driver and report only bytes actually accepted.
 */
typedef uint8_t (*KymoWriteFn)(void *user, const uint8_t *bytes, uint8_t length);

/**
 * @brief Report whether the transport still owns the transmit buffer.
 *
 * @details Optional for copying drivers; mandatory for drivers borrowing a pointer. Also report
 * busy while disconnected/uninitialized. A false result permits reuse of the byte buffer.
 * @param[in,out] user transport_user driver context; inspect completion/connection state.
 * @return Nonzero while unavailable/in flight; zero when the runtime may access or reuse the buffer.
 * @par Usage
 * Read a UART DMA completion flag or USB transmit state.
 */
typedef uint8_t (*KymoBusyFn)(void *user);

/**
 * @brief Advance application-side transport maintenance once per Main.
 *
 * @details Called before fault/busy handling on every valid initialized Main invocation.
 * Keep execution bounded. This callback must not recursively call Init/Main.
 * @param[in,out] user transport_user context updated by transport polling/connection maintenance.
 * @par Usage
 * Poll a network client or driver completion state, then let Main continue.
 */
typedef void (*KymoServiceFn)(void *user);

/** @} */

/** @name Configuration and runtime storage
 * @{ */
/**
 * @brief Persistent wiring of scheduling, callbacks and caller-owned state.
 *
 * @details Input to Init. This object and its channel table remain immutable and alive while
 * the context uses them. User contexts may be mutable. Each runtime instance needs
 * its own last_sample_ms array; no hardware initialization is performed by Init.
 * @par Usage
 * Define a static const configuration in kymo_config.c/.cpp and pass its address to Init.
 */
typedef struct {
    /**
     * @brief Non-null input array describing all channels.
     *
     * @details Exactly channel_count readable entries; must outlive the runtime and remain unchanged.
     * @par Usage
     * Point to a static const KymoChannel table.
     */
    const KymoChannel *channels;
    /**
     * @brief Non-null mutable scheduling array owned by the application.
     *
     * @details Input/output storage with channel_count entries. Init seeds it for immediate sampling;
     * Main updates the selected channel. Do not modify while the runtime operates.
     * @par Usage
     * Declare static uint32_t last_sample_ms[CHANNEL_COUNT].
     */
    uint32_t *last_sample_ms;
    /**
     * @brief Required millisecond clock callback.
     *
     * @details Input; called by Init and when Main searches for due channels.
     * @par Usage
     * Assign a platform wrapper returning uint32 milliseconds.
     */
    KymoClockFn clock_ms;
    /**
     * @brief Opaque context forwarded to clock_ms.
     *
     * @details Input pointer to application-owned input/output state; NULL is valid if unused.
     * @par Usage
     * Use for a clock instance or leave NULL for a global SDK clock.
     */
    void *clock_user;
    /**
     * @brief Required callback producing channel coordinates.
     *
     * @details Input function pointer; callback writes one KymoSample per selected attempt.
     * @par Usage
     * Assign the application sensor/parameter reader.
     */
    KymoSampleFn sample;
    /**
     * @brief Opaque context forwarded to sample.
     *
     * @details Input pointer to application-owned input/output data; synchronize ISR/task sharing externally.
     * @par Usage
     * Point to the application parameter structure instead of using library globals.
     */
    void *sample_user;
    /**
     * @brief Required binary transport callback.
     *
     * @details Input function pointer; its result controls pending offsets and backpressure.
     * @par Usage
     * Bind UART, USB, TCP or message transport in the application.
     */
    KymoWriteFn write;
    /**
     * @brief Optional readiness/buffer-ownership callback.
     *
     * @details NULL means writes copy/consume before returning. Borrowing drivers must provide it.
     * @par Usage
     * Bind an asynchronous driver completion check.
     */
    KymoBusyFn busy;
    /**
     * @brief Optional once-per-call transport service callback.
     *
     * @details NULL disables maintenance; otherwise called even while busy or faulted.
     * @par Usage
     * Bind a bounded driver poll function if required.
     */
    KymoServiceFn service;
    /**
     * @brief Shared context for write, busy and service.
     *
     * @details Input pointer to application-owned driver state, read or updated by those callbacks.
     * @par Usage
     * Point to a driver handle wrapper; never encode this pointer on the wire.
     */
    void *transport_user;
    /**
     * @brief Number of configured channels, inclusive range 1..256.
     *
     * @details Input; uint16 is required to represent all 256 byte-valued IDs.
     * @par Usage
     * Set to the channel-table element count and size last_sample_ms accordingly.
     */
    uint16_t channel_count;
} KymoConfig;

/**
 * @brief Caller-owned runtime state with a single persistent transmit buffer.
 *
 * @details Input/output of Init/Main. Allocate one instance per independently driven transport.
 * Fields are implementation state, exposed only for static allocation; applications
 * must not edit them. Initialize with Init before use (zero initialization is safe).
 * @par Usage
 * static KymoContext context; then Kymo_Init(&context, &configuration).
 */
typedef struct {
    /**
     * @brief Borrowed configuration pointer, NULL when disabled.
     *
     * @details Written by Init and read by Main; configuration lifetime belongs to the caller.
     * @par Usage
     * Inspect for diagnostics only; bind through Init.
     */
    const KymoConfig *config;
    /**
     * @brief Clock epoch captured by Init.
     *
     * @details Initialized/read only when KYMO_ENABLE_TIMESTAMP is enabled.
     * Otherwise remains zero after Init; retained to keep the public layout stable.
     * @par Usage
     * Inspect for diagnostics; change the epoch only by reinitializing safely.
     */
    uint32_t start_ms;
    /**
     * @brief Round-robin index of the next channel to inspect.
     *
     * @details Updated by Main; valid initialized values are below channel_count.
     * @par Usage
     * Internal fairness state; do not advance it from application code.
     */
    uint16_t next_channel;
    /**
     * @brief Fixed 21-byte frame storage owned by this context.
     *
     * @details Written by Main and passed read-only to write. Must remain alive/unchanged while busy.
     * @par Usage
     * Keep the context in persistent storage, especially with DMA/USB drivers.
     */
    uint8_t tx[KYMO_FRAME_MAX_SIZE];
    /**
     * @brief Encoded frame length, zero or 9..21 bytes.
     *
     * @details Updated by Main after encoding. Only tx[0..tx_length-1] belongs to the frame.
     * @par Usage
     * Use only for diagnostics; do not resize a pending frame.
     */
    uint8_t tx_length;
    /**
     * @brief Count of frame bytes already accepted by the driver.
     *
     * @details Updated after writes; 0 <= tx_offset <= tx_length. Equality does not prove physical completion.
     * @par Usage
     * Use for diagnostics; the busy callback determines buffer release.
     */
    uint8_t tx_offset;
    /**
     * @brief Latched driver-contract failure flag.
     *
     * @details Zero is normal; nonzero suppresses sampling/writing until safe reinitialization.
     * @par Usage
     * Observe status KYMO_IO_ERROR instead of writing this flag.
     */
    uint8_t fault;
} KymoContext;

/** @} */

#if KYMO_ENABLE_RUNTIME || defined(DOXYGEN)
/** @name Lifecycle
 * @{ */
/**
 * @brief Validate static configuration and enable a runtime instance.
 *
 * @details Available with KYMO_ENABLE_RUNTIME=1. Checks required callbacks/storage,
 * channel count, unique IDs, build-supported flags and periods.
 * Valid channels become immediately due. A non-null context is cleared before validation;
 * failure therefore disables it. No heap allocation or peripheral setup occurs.
 * @param[out] context Writable, persistent runtime storage; NULL is rejected.
 * @param[in] config Persistent configuration, or NULL to fail. Its last_sample_ms pointee is output storage.
 * @return KYMO_OK on success, KYMO_BAD_CONFIG on validation failure.
 * @pre context, config and scheduling arrays must not overlap.
 * @pre Stop any driver borrowing context->tx before initialization or reinitialization.
 * @post On success, all config-owned pointers must remain valid for subsequent Main calls.
 * @par Usage
 * Initialize hardware, then check Kymo_Init(&context, &config) before calling Main.
 */
KymoStatus Kymo_Init(KymoContext *context, const KymoConfig *config);

/**
 * @brief Perform one bounded scheduling/transmission step.
 *
 * @details Services the driver, respects a latched fault/busy state, resumes pending bytes or
 * samples one due channel. At most one sample and one write occur, with at most
 * channel_count checks. Delayed samples use current values; no backlog is accumulated.
 * Callback execution time is outside the runtime control. This is not an ISR API.
 * @param[in,out] context Initialized persistent state; NULL or zero-initialized disabled state is rejected.
 * @return KYMO_OK, KYMO_IDLE, KYMO_BUSY, KYMO_SKIPPED, KYMO_BAD_SAMPLE, KYMO_BAD_CONFIG or KYMO_IO_ERROR.
 * @pre Do not call concurrently or recursively, or modify configuration while active.
 * @pre Clock calls must remain consistent; invoke Main at least once within INT32_MAX ms.
 * @par Usage
 * Call from a single cyclic task faster than the sum of channel rates; allow extra calls for short writes.
 */
KymoStatus Kymo_Main(KymoContext *context);
#endif
/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* KYMO_RUNTIME_H */
