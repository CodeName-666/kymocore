/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial */
/** @file kymo_trace.h @brief Fixed-storage recorder hooks and resource bounds.
 * @details Hook calls never allocate or transport. Concurrent callers require paired
 * critical callbacks that serialize all recorder accesses; these callbacks are not
 * declared ISR-safe by this portable library. Persistent recorder storage is caller-owned.
 * @par Usage Configure callbacks, call KymoTrace_Init, then use instrumentation macros.
 */
#ifndef KYMO_TRACE_H
#define KYMO_TRACE_H
#include "kymo_features.h"
#if KYMO_EFFECTIVE_TRACE
#include "kymo_trace_protocol.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Maximum event ring capacity per core (power of two, 16..4096). */
#ifndef KYMO_TRACE_EVENTS_PER_CORE
#define KYMO_TRACE_EVENTS_PER_CORE 64u
#endif
/** @brief Number of recorder cores supported (1 or 2). */
#ifndef KYMO_TRACE_CORE_COUNT
#define KYMO_TRACE_CORE_COUNT 1u
#endif
/** @brief Maximum allocated context records (1..256). */
#ifndef KYMO_TRACE_MAX_CONTEXTS
#define KYMO_TRACE_MAX_CONTEXTS 16u
#endif
/** @brief Maximum stored scope symbols per context (1..255). */
#ifndef KYMO_TRACE_MAX_DEPTH
#define KYMO_TRACE_MAX_DEPTH 32u
#endif
/** @brief Maximum nested ISR activations when ISR tracing is effective (1..16). */
#ifndef KYMO_TRACE_MAX_ISR_DEPTH
#define KYMO_TRACE_MAX_ISR_DEPTH 8u
#endif
#if (KYMO_TRACE_EVENTS_PER_CORE < 16u) || (KYMO_TRACE_EVENTS_PER_CORE > 4096u) || ((KYMO_TRACE_EVENTS_PER_CORE & (KYMO_TRACE_EVENTS_PER_CORE-1u)) != 0u)
#error "KYMO_TRACE_EVENTS_PER_CORE must be a power of two from 16 through 4096"
#endif
#if (KYMO_TRACE_CORE_COUNT < 1u) || (KYMO_TRACE_CORE_COUNT > 2u)
#error "KYMO_TRACE_CORE_COUNT must be 1 or 2"
#endif
#if (KYMO_TRACE_MAX_CONTEXTS < 1u) || (KYMO_TRACE_MAX_CONTEXTS > 256u)
#error "KYMO_TRACE_MAX_CONTEXTS must be from 1 through 256"
#endif
#if (KYMO_TRACE_MAX_DEPTH < 1u) || (KYMO_TRACE_MAX_DEPTH > 255u)
#error "KYMO_TRACE_MAX_DEPTH must be from 1 through 255"
#endif
#if (KYMO_TRACE_MAX_ISR_DEPTH < 1u) || (KYMO_TRACE_MAX_ISR_DEPTH > 16u)
#error "KYMO_TRACE_MAX_ISR_DEPTH must be from 1 through 16"
#endif
#define KYMO_TRACE_FLAG_ROOT_BOUNDARY 0x0001u
#define KYMO_TRACE_FLAG_CONTEXT_INVALID 0x0002u
#define KYMO_TRACE_FLAG_ARG_VALID 0x0004u
#define KYMO_TRACE_FLAG_RECOVERY 0x0008u
/** @brief Recorder clock and synchronization bindings.
 * @details When critical callbacks are absent the application serializes every
 * recorder access. Supplied callbacks are not presumed ISR-safe.
 */
typedef struct KymoTraceConfig {
    /** @brief Read monotonically increasing hardware ticks. */
    uint64_t (*clock_ticks)(void *user);
    /** @brief Return a core ID in 0..KYMO_TRACE_CORE_COUNT-1. */
    uint8_t (*core_id)(void *user);
    /** @brief Return the currently active context ID, or zero if unknown. */
    uint32_t (*current_context)(void *user);
    /** @brief Enter the application's recorder-wide critical section. */
    void (*critical_enter)(void *user);
    /** @brief Exit the application's recorder-wide critical section. */
    void (*critical_exit)(void *user);
    /** @brief User pointer passed to all callbacks. */
    void *user;
    /** @brief Session identifier copied into mux envelope configuration. */
    uint64_t session_id;
    /** @brief Clock-domain identifier serialized in event batches. */
    uint16_t clock_domain_id;
} KymoTraceConfig;
/** @brief Shared scope state keyed by application context ID. */
typedef struct KymoTraceContextState {
    uint32_t id; uint32_t sequence; uint32_t depth; uint8_t allocated; uint8_t invalid;
    uint8_t depth_overflow;
    uint16_t symbols[KYMO_TRACE_MAX_DEPTH];
} KymoTraceContextState;
/** @brief Fixed-storage event recorder; the application owns this object. */
typedef struct KymoTraceRecorder {
    KymoTraceConfig config;
    KymoTraceEvent rings[KYMO_TRACE_CORE_COUNT][KYMO_TRACE_EVENTS_PER_CORE];
    KymoTraceContextState contexts[KYMO_TRACE_MAX_CONTEXTS];
    uint32_t record_seq[KYMO_TRACE_CORE_COUNT]; uint32_t drops[KYMO_TRACE_CORE_COUNT];
    uint32_t errors[KYMO_TRACE_CORE_COUNT]; uint32_t producer_next[KYMO_TRACE_CORE_COUNT];
    uint32_t settled_through[KYMO_TRACE_CORE_COUNT];
    uint16_t high_water[KYMO_TRACE_CORE_COUNT];
    uint16_t heads[KYMO_TRACE_CORE_COUNT]; uint16_t counts[KYMO_TRACE_CORE_COUNT];
#if KYMO_EFFECTIVE_TRACE_ISR
    uint16_t isr_depth[KYMO_TRACE_CORE_COUNT];
    uint32_t interrupted_contexts[KYMO_TRACE_CORE_COUNT][KYMO_TRACE_MAX_ISR_DEPTH];
    uint32_t irq_contexts[KYMO_TRACE_CORE_COUNT][KYMO_TRACE_MAX_ISR_DEPTH];
#endif
    uint8_t initialized; uint8_t pop_core;
} KymoTraceRecorder;
/** @brief Initialize persistent recorder storage.
 * @param[out] recorder Caller-owned object to initialize.
 * @param[in] config Required time and context callbacks and synchronization contract.
 * @return 1 on success; 0 on invalid pointers or callback pairing.
 */
uint8_t KymoTrace_Init(KymoTraceRecorder *recorder, const KymoTraceConfig *config);
/** @brief Record a function or scope entry; bounded and allocation-free.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Active application context ID.
 * @param[in] symbol_id Registered symbol ID; zero is invalid.
 */
void KymoTrace_Enter(KymoTraceRecorder *recorder, uint32_t context_id, uint16_t symbol_id);
/** @brief Record scope exit and validate it against the context stack.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Active application context ID.
 * @param[in] symbol_id Symbol ID that must match the top scope.
 */
void KymoTrace_Exit(KymoTraceRecorder *recorder, uint32_t context_id, uint16_t symbol_id);
/** @brief Record a marker and its value.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Active application context ID.
 * @param[in] marker_id Registered marker symbol ID.
 * @param[in] value Application-defined marker value.
 */
void KymoTrace_Mark(KymoTraceRecorder *recorder, uint32_t context_id, uint16_t marker_id, uint32_t value);
#if KYMO_EFFECTIVE_TRACE_ISR
/** @brief Record ISR entry and save interrupted-context identity.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] irq_context Registered ISR context ID.
 */
void KymoTrace_IsrEnter(KymoTraceRecorder *recorder, uint32_t irq_context);
/** @brief Record ISR exit and restore the interrupted-context identity.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] irq_context Must match the current ISR context ID.
 */
void KymoTrace_IsrExit(KymoTraceRecorder *recorder, uint32_t irq_context);
#endif
#if KYMO_EFFECTIVE_TRACE_SCHEDULER
/** @brief Record an explicit task switch; no scheduler hook is installed.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Outgoing context ID, or zero if unknown/idle.
 * @param[in] task_id Incoming context ID.
 */
void KymoTrace_Scheduler(KymoTraceRecorder *recorder, uint32_t context_id, uint32_t task_id);
/** @brief Record task creation.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id New task context ID.
 * @param[in] generation Task generation stored in the event argument.
 */
void KymoTrace_TaskCreate(KymoTraceRecorder *recorder, uint32_t context_id, uint32_t generation);
/** @brief Record task termination.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Terminated task context ID.
 * @param[in] generation Task generation stored in the event argument.
 */
void KymoTrace_TaskEnd(KymoTraceRecorder *recorder, uint32_t context_id, uint32_t generation);
/** @brief Record a per-core running-context checkpoint.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Running context ID, or zero when unknown/idle.
 * @param[in] state 0 unknown, 1 running task, or 2 idle.
 * @param[in] recovery Nonzero to mark a trusted recovery checkpoint.
 */
void KymoTrace_CoreState(KymoTraceRecorder *recorder, uint32_t context_id, uint8_t state, uint8_t recovery);
#endif
#if KYMO_EFFECTIVE_TRACE_FLOW
/** @brief Record a flow correlation send.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Sending context ID.
 * @param[in] symbol_id Optional registered operation symbol ID (zero if absent).
 * @param[in] correlation_id Application-defined correlation value.
 */
void KymoTrace_FlowSend(KymoTraceRecorder *recorder, uint32_t context_id, uint16_t symbol_id, uint32_t correlation_id);
/** @brief Record a flow correlation receive.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Receiving context ID.
 * @param[in] symbol_id Optional registered operation symbol ID (zero if absent).
 * @param[in] correlation_id Application-defined correlation value.
 */
void KymoTrace_FlowReceive(KymoTraceRecorder *recorder, uint32_t context_id, uint16_t symbol_id, uint32_t correlation_id);
#endif
/** @brief Emit a context recovery event when the caller confirms depth is zero.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] context_id Context to recover.
 * @param[in] generation New application-supplied context generation.
 * @return 1 if reset was recorded; 0 if the context is active or invalid.
 */
uint8_t KymoTrace_ContextReset(KymoTraceRecorder *recorder, uint32_t context_id, uint32_t generation);
/** @brief Pop one event in round-robin core order.
 * @param[in,out] recorder Initialized recorder.
 * @param[out] event Receives the next event if available.
 * @return 1 if copied; 0 if no event is available or arguments are invalid.
 */
uint8_t KymoTrace_Pop(KymoTraceRecorder *recorder, KymoTraceEvent *event);
/** @brief Read cumulative dropped events for one core under recorder locking.
 * @param[in,out] recorder Initialized recorder.
 * @param[in] core_id Core index.
 * @return Cumulative overflow drops, or zero for an invalid core/recorder.
 */
uint32_t KymoTrace_DropCount(KymoTraceRecorder *recorder, uint8_t core_id);
#ifdef __cplusplus
}
/** @brief C++ RAII scope that emits Enter on construction and Exit on destruction.
 * @details Copy and move operations are disabled to prevent duplicate exits.
 */
class KymoTraceScope {
public:
    /** @brief Begin a measured scope.
     * @param[in,out] recorder Initialized recorder.
     * @param[in] context Context ID to associate with the scope.
     * @param[in] symbol Registered symbol ID.
     */
    KymoTraceScope(KymoTraceRecorder *recorder, uint32_t context, uint16_t symbol)
        : m_recorder(recorder), m_context(context), m_symbol(symbol) {
        KymoTrace_Enter(m_recorder, m_context, m_symbol);
    }
    /** @brief End the measured scope. */
    ~KymoTraceScope() {
        KymoTrace_Exit(m_recorder, m_context, m_symbol);
    }
    KymoTraceScope(const KymoTraceScope &) = delete;
    KymoTraceScope &operator=(const KymoTraceScope &) = delete;
private:
    KymoTraceRecorder *m_recorder;
    uint32_t m_context;
    uint16_t m_symbol;
};
#endif
#endif
#endif
