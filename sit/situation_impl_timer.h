/***************************************************************************************************
*
*   situation_impl_timer.h - Timer & Oscillator System Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Extracted from situation_impl.h for modularity.
*   This file is included by situation_impl.h.
*
*   Contains:
*     - Oscillator state queries (get, previous, updated, ping)
*     - Oscillator configuration (period, trigger count)
*     - Ping progress tracking
*     - High-resolution time query
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_TIMER_H
#define SITUATION_IMPL_TIMER_H

// --- High-Resolution Time ---
static uint64_t _SituationGetHighResTime(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000) / frequency.QuadPart);
#else
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

// --- Timer System API Implementation ---
/**
 * @brief Gets the current binary state (0 or 1) of a temporal oscillator.
 * @details Each oscillator flips its state between `true` (1) and `false` (0) every time its defined period elapses, creating a perfectly timed square-wave signal. This function returns the state of the oscillator for the *current* frame.
 *
 * @param oscillator_id The ID of the oscillator to query (0-255).
 *
 * @return `true` if the oscillator's current state is 1, `false` if it is 0.
 *
 * @note The state is updated once per frame by `SituationUpdateTimers()`.
 *
 * @see SituationTimerGetPreviousOscillatorState(), SituationTimerHasOscillatorUpdated()
 */
SITAPI bool SituationTimerGetOscillatorState(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized) { _SituationSetErrorFromCode(SITUATION_ERROR_TIMER_SYSTEM, "SituationTimerGetOscillatorState: timer system not initialized"); return false; }
    if (oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationTimerGetOscillatorState: oscillator_id out of range"); return false; }
    SituationTimerSystem* ts = &sit_gs.timer_system_instance;
    int bank = oscillator_id / 64;
    int bit_pos = oscillator_id % 64;
    return (ts->state_current[bank] & ((uint64_t)1 << bit_pos)) != 0;
}

/**
 * @brief Gets the binary state (0 or 1) of a temporal oscillator from the *previous* frame.
 * @details This function is a crucial companion to `SituationTimerGetOscillatorState`. It allows you to detect the exact moment a state change occurred by comparing the previous state with the current one.
 *
 * @param oscillator_id The ID of the oscillator to query (0-255).
 *
 * @return `true` if the oscillator's state was 1 in the previous frame, `false` if it was 0.
 *
 * @see SituationTimerGetOscillatorState(), SituationTimerHasOscillatorUpdated()
 */
SITAPI bool SituationTimerGetPreviousOscillatorState(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized) { _SituationSetErrorFromCode(SITUATION_ERROR_TIMER_SYSTEM, "SituationTimerGetPreviousOscillatorState: timer system not initialized"); return false; }
    if (oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationTimerGetPreviousOscillatorState: oscillator_id out of range"); return false; }
    SituationTimerSystem* ts = &sit_gs.timer_system_instance;
    int bank = oscillator_id / 64;
    int bit_pos = oscillator_id % 64;
    return (ts->state_previous[bank] & ((uint64_t)1 << bit_pos)) != 0;
}

/**
 * @brief Checks if an oscillator's state has changed *this frame*.
 * @details This is the primary function for triggering "on-beat" events. It returns `true` only for the single frame in which an oscillator flips its state (e.g., from 0 to 1, or 1 to 0). This allows you to execute logic precisely in sync with the oscillator's rhythm.
 *
 * @param oscillator_id The ID of the oscillator to query (0-255).
 *
 * @return `true` if the oscillator's state is different from its state in the previous frame, `false` otherwise.
 *
 * @example
 *   // Make an object pulse in time with oscillator 10
 *   if (SituationTimerHasOscillatorUpdated(10)) {
 *       my_object.scale = 1.5f; // Trigger animation on the beat
 *   } else {
 *       my_object.scale = 1.0f; // Return to normal
 *   }
 */
SITAPI bool SituationTimerHasOscillatorUpdated(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized) { _SituationSetErrorFromCode(SITUATION_ERROR_TIMER_SYSTEM, "SituationTimerHasOscillatorUpdated: timer system not initialized"); return false; }
    if (oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationTimerHasOscillatorUpdated: oscillator_id out of range"); return false; }
    // Updated if current state is different from previous state for that oscillator
    return SituationTimerGetOscillatorState(oscillator_id) != SituationTimerGetPreviousOscillatorState(oscillator_id);
}

/**
 * @brief Checks if an oscillator's period has elapsed since the last time this specific function was called for this oscillator.
 * @details This function provides a "polling" or "cooldown" mechanism. Unlike `SituationTimerHasOscillatorUpdated`, which is tied to the oscillator's fixed global clock, `SituationTimerPingOscillator` maintains its own internal timestamp.
 *          It returns `true` once the period has passed since its last successful ping, and then immediately resets its internal timer.
 *          This is ideal for scenarios like checking if an enemy can fire a weapon again, or triggering an event periodically that is not strictly aligned with the global beat.
 *
 * @param oscillator_id The ID of the oscillator whose period should be used for the ping interval.
 *
 * @return `true` if the period has elapsed since the last successful ping for this oscillator, `false` otherwise.
 */
SITAPI bool SituationTimerPingOscillator(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized) { _SituationSetErrorFromCode(SITUATION_ERROR_TIMER_SYSTEM, "SituationTimerPingOscillator: timer system not initialized"); return false; }
    if (oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationTimerPingOscillator: oscillator_id out of range"); return false; }
    SituationTimerSystem* ts = &sit_gs.timer_system_instance;

    bool ping_triggered = (ts->current_system_time_seconds >= ts->last_ping_time_seconds[oscillator_id] + ts->period_seconds[oscillator_id]);
    if (ping_triggered) {
        // If multiple periods passed, advance last_ping_time by multiples of period until it's in the current "frame"
        while(ts->last_ping_time_seconds[oscillator_id] + ts->period_seconds[oscillator_id] <= ts->current_system_time_seconds) {
            ts->last_ping_time_seconds[oscillator_id] += ts->period_seconds[oscillator_id];
        }
    }
    return ping_triggered;
}

/**
 * @brief Gets the total number of times an oscillator has flipped its state since the application started.
 * @details This function returns a continuously increasing count of state changes for a given oscillator. This can be used as a simple, rhythmic counter for procedural generation or sequencing complex event patterns.
 *
 * @param oscillator_id The ID of the oscillator to query (0-255).
 *
 * @return A `uint64_t` representing the total number of triggers.
 */
SITAPI uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized) { _SituationSetErrorFromCode(SITUATION_ERROR_TIMER_SYSTEM, "SituationTimerGetOscillatorTriggerCount: timer system not initialized"); return 0; }
    if (oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationTimerGetOscillatorTriggerCount: oscillator_id out of range"); return 0; }
    return sit_gs.timer_system_instance.trigger_count[oscillator_id];
}

/**
 * @brief Gets the current period of an oscillator in seconds.
 * @details The "period" is the fundamental property of an oscillator. It defines the exact duration of one half of its cycle.
 *          An oscillator's state flips between `false` (0) and `true` (1). The period is the amount of time the oscillator will spend in the `false` state before flipping to `true`, and also the amount of time it will spend in the `true` state before flipping back to `false`.
 *
 * For example, an oscillator with a period of `0.5` seconds will:
 * - Spend 0.5 seconds in state 0.
 * - Flip to state 1 and spend 0.5 seconds in state 1.
 * - Flip back to state 0 and spend 0.5 seconds in state 0.
 * - ...and so on.
 *
 * The total duration of one full cycle (0 -> 1 -> 0) is therefore twice the period.
 *
 * @param oscillator_id The ID of the oscillator to query (0-255).
 *
 * @return The oscillator's period in seconds as a high-precision `double`.
 *
 * @see SituationSetTimerOscillatorPeriod()
 */
SITAPI double SituationTimerGetOscillatorPeriod(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized || oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) return 0.0;
    return sit_gs.timer_system_instance.period_seconds[oscillator_id];
}

/**
 * @brief Sets a new period for an oscillator at runtime, changing its frequency.
 * @details This function allows you to dynamically change the rhythm of any oscillator. The change takes effect immediately, and the oscillator's next state-flip is rescheduled based on the current time and the new period.
 *
 * @par What is a Period?
 *   The "period" is the fundamental property of an oscillator. It defines the exact duration in seconds for **one half of its cycle**.
 *
 *   An oscillator's state flips between `false` (0) and `true` (1). The period is the amount of time the oscillator will spend in the `false` state before flipping to `true`, and also the amount of time it will spend in the `true` state before flipping back to `false`.
 *
 *   For example, setting a period of `0.25` seconds will cause the oscillator to:
 *   - Spend 0.25 seconds in state 0.
 *   - Flip to state 1 and spend 0.25 seconds in state 1.
 *   - Flip back to state 0 and spend 0.25 seconds in state 0.
 *   - ...and so on.
 *
 *   The total duration of one full cycle (0 -> 1 -> 0) is therefore **twice the period**. A shorter period means a faster oscillator (higher frequency).
 *
 * @param oscillator_id The ID of the oscillator to modify (0-255).
 * @param period_seconds The new period in seconds. This value must be positive.
 *
 * @return `SITUATION_SUCCESS` on successful update.
 * @return `SITUATION_ERROR_NOT_INITIALIZED` if the timer system is not active.
 * @return `SITUATION_ERROR_INVALID_PARAM` if the `oscillator_id` is out of range or if `period_seconds` is zero or negative.
 *
 * @see SituationTimerGetOscillatorPeriod()
 */
SITAPI SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds) {
    if (!sit_gs.timer_system_instance.is_initialized) return SITUATION_ERROR_NOT_INITIALIZED;
    if (oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS || period_seconds <= 0.0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    SituationTimerSystem* ts = &sit_gs.timer_system_instance;
    ts->period_seconds[oscillator_id] = period_seconds;
    // Restart from current time with new period (drift-free anchor)
    ts->anchor_time_seconds[oscillator_id] = ts->current_system_time_seconds;
    ts->trigger_count[oscillator_id] = 0;
    ts->next_trigger_time_seconds[oscillator_id] = ts->current_system_time_seconds + period_seconds;
    // Reset ping timer as well
    ts->last_ping_time_seconds[oscillator_id] = ts->current_system_time_seconds;
    return SITUATION_SUCCESS;
}

/**
 * @brief Gets the progress of a timed interval since the last successful "ping".
 * @details This function provides a normalized value from 0.0 to 1.0 (or higher) representing the progress towards completing a timed interval. The duration of this interval is taken from the specified oscillator's `period`.
 *          This is extremely useful for driving progress bars, cooldown timers, or animating values over a specific duration.
 *
 * @par Relationship to Ping and Oscillators
 *   - The "ping" timer is separate from the oscillator's main state-flipping clock.
 *   - `SituationTimerPingOscillator` checks if the `period` has elapsed since its last successful call. If so, it returns `true` and resets its internal timestamp.
 *   - This function, `SituationTimerGetPingProgress`, simply measures the time elapsed since that last reset and divides it by the oscillator's `period` to get a normalized progress value.
 *
 * @par Return Value
 *   - **`0.0`** means the timer has just been successfully pinged (reset).
 *   - **`0.5`** means half of the interval duration has elapsed.
 *   - **`1.0`** means the full interval duration has elapsed, and a call to `SituationTimerPingOscillator` would now return `true`.
 *   - **`> 1.0`** means more than one full interval has elapsed since the last ping.
 *
 * @param oscillator_id The ID of the oscillator (0-255) whose `period` will be used as the duration for the interval.
 *
 * @return A `double` representing the normalized progress [0.0 - N.N].
 *
 * @note This function *reads* the state of the ping timer but does not reset it. Only `SituationTimerPingOscillator` can reset the timer.
 *
 * @see SituationTimerPingOscillator()
 */
SITAPI double SituationTimerGetPingProgress(int oscillator_id) {
    if (!sit_gs.timer_system_instance.is_initialized || oscillator_id < 0 || oscillator_id >= SITUATION_MAX_OSCILLATORS) {
        return 0.0;
    }

    SituationTimerSystem* ts = &sit_gs.timer_system_instance;
    double period = ts->period_seconds[oscillator_id];

    // Avoid division by zero for invalid periods
    if (period <= 0.0) {
        return 1.0; // Or 0.0, depending on desired behavior for a zero-period timer
    }

    // Calculate the time that has passed since the last ping
    double elapsed_since_ping = ts->current_system_time_seconds - ts->last_ping_time_seconds[oscillator_id];

    // Return the normalized progress
    return elapsed_since_ping / period;
}

/**
 * @brief Gets the total time elapsed since the library was initialized.
 * @details This function returns the master application time, updated once per frame by `SituationUpdateTimers()`. It serves as the high-resolution monotonic clock for the entire application and is the basis for all other timing functions, including the Temporal Oscillator system.
 *
 * @return The total elapsed time in seconds as a high-precision `double`.
 *
 * @see SituationGetFrameTime()
 */
SITAPI double SituationTimerGetTime(void) {
    // If timer system is initialized, return its cached current time. Otherwise, direct GLFW call.
    if (sit_gs.timer_system_instance.is_initialized) {
        return sit_gs.timer_system_instance.current_system_time_seconds;
    }
    return glfwGetTime(); // Fallback if timer system not ready or SituationUpdate not called yet
}

#endif // SITUATION_IMPL_TIMER_H
