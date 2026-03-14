/*
 * MIT License
 *
 * Copyright (c) 2026 Davide Girardi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "advanced_timer_face.h"
#include "watch.h"
#include "watch_utility.h"

static const uint32_t _default_timer_values[] = {0x000200, 0x000500, 0x000A00, 0x001400, 0x002D02}; // default timers: 2 min, 5 min, 10 min, 20 min, 2 h 45 min

// sound sequence for a single beeping sequence
static const int8_t _sound_seq_beep[] = {BUZZER_NOTE_C8, 3, BUZZER_NOTE_REST, 3, -2, 2, BUZZER_NOTE_C8, 5, BUZZER_NOTE_REST, 25, 0};
static const int8_t _sound_seq_start[] = {BUZZER_NOTE_C8, 2, 0};

static uint8_t _beeps_to_play;    // temporary counter for ring signals playing

uint8_t last_timer;

static void _beep(void) {
    if (movement_button_should_sound()) watch_buzzer_play_note(BUZZER_NOTE_C7, 20);
}

static void _signal_callback() {
    if (_beeps_to_play) {
        _beeps_to_play--;
        watch_buzzer_play_sequence((int8_t *)_sound_seq_beep, _signal_callback);
    }
}

static void _start(advanced_timer_state_t *state, bool with_beep) {
    if (state->timers[state->current_timer].value == 0) return;
    watch_date_time_t now = watch_rtc_get_date_time();
    state->now_ts = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
    if (state->mode == at_pausing)
        state->target_ts = state->now_ts + state->paused_left;
    else
        state->target_ts = watch_utility_offset_timestamp(state->now_ts, 
                                                          state->timers[state->current_timer].unit.hours, 
                                                          state->timers[state->current_timer].unit.minutes, 
                                                          state->timers[state->current_timer].unit.seconds);
    watch_date_time_t target_dt = watch_utility_date_time_from_unix_time(state->target_ts, movement_get_current_timezone_offset());
    state->mode = at_running;
    movement_schedule_background_task_for_face(state->watch_face_index, target_dt);
    watch_set_indicator(WATCH_INDICATOR_BELL);
}

static void _draw(advanced_timer_state_t *state, uint8_t subsecond) {
    char bottom_time[10];
    char timer_id[3];
    uint32_t delta;
    div_t result;
    uint8_t h, min, sec;

    switch (state->mode) {
        case at_pausing:
            if (state->pausing_seconds % 2)
                watch_clear_indicator(WATCH_INDICATOR_BELL);
            else
                watch_set_indicator(WATCH_INDICATOR_BELL);
            if (state->pausing_seconds != 1)
                // not 1st iteration (or 256th): do not write anything
                return;
            // fall through
        case at_running:
            delta = state->target_ts - state->now_ts;
            result = div(delta, 60);
            sec = result.rem;
            result = div(result.quot, 60);
            min = result.rem;
            h = result.quot;
            sprintf(bottom_time, "%02u%02u%02u", h, min, sec);
            break;
        case at_setting:
            if (state->settings_state == 1) {
                // ask it the current timer shall be erased
                sprintf(bottom_time, "CLEAR%c", state->erase_timer_flag ? 'y' : 'n');
                watch_clear_colon();
            } else if (state->settings_state == 5) {
                sprintf(bottom_time, " LOOP%c", state->timers[state->current_timer].unit.repeat ? 'y' : 'n');
                watch_clear_colon();
            } else {
                sprintf(bottom_time, "%02u%02u%02u", state->timers[state->current_timer].unit.hours,
                        state->timers[state->current_timer].unit.minutes,
                        state->timers[state->current_timer].unit.seconds);
                watch_set_colon();
            }
            break;
        case at_waiting:
            sprintf(bottom_time, "%02u%02u%02u", state->timers[state->current_timer].unit.hours,
                    state->timers[state->current_timer].unit.minutes,
                    state->timers[state->current_timer].unit.seconds);
            watch_set_colon();
            break;
    }

    sprintf(timer_id, "%2u", state->current_timer + 1);
    if (state->mode == at_setting && subsecond % 2) {
        // blink the current settings value
        if (state->settings_state == 0) timer_id[0] = timer_id[1] = ' ';
        else if (state->settings_state == 1 || state->settings_state == 5) bottom_time[5] = ' ';
        else bottom_time[(state->settings_state - 1) * 2 - 2] = bottom_time[(state->settings_state - 1) * 2 - 1] = ' ';
    }
    watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, bottom_time, bottom_time);
    watch_display_text_with_fallback(WATCH_POSITION_TOP_RIGHT, timer_id, timer_id);

    // set lap indicator when we have a looping timer
    if (state->timers[state->current_timer].unit.repeat) watch_set_indicator(WATCH_INDICATOR_LAP);
    else watch_clear_indicator(WATCH_INDICATOR_LAP);
}

static void _reset(advanced_timer_state_t *state) {
    state->mode = at_waiting;
    movement_cancel_background_task_for_face(state->watch_face_index);
    watch_clear_indicator(WATCH_INDICATOR_BELL);
}

static void _set_next_valid_timer(advanced_timer_state_t *state) {
    if ((state->timers[state->current_timer].value & 0xFFFFFF) == 0) {
        uint8_t i = state->current_timer;
        do {
            i = (i + 1) % TIMER_SLOTS;
        } while ((state->timers[i].value & 0xFFFFFF) == 0 && i != state->current_timer);
        state->current_timer = i;
    }
}

static void _resume_setting(advanced_timer_state_t *state) {
    state->settings_state = 0;
    state->mode = at_waiting;
    movement_request_tick_frequency(1);
    _set_next_valid_timer(state);
}

static void _settings_increment(advanced_timer_state_t *state) {
    switch(state->settings_state) {
        case 0:
            state->current_timer = (state->current_timer + 1) % TIMER_SLOTS;
            break;
        case 1:
            state->erase_timer_flag ^= 1;
            break;
        case 2:
            state->timers[state->current_timer].unit.hours = (state->timers[state->current_timer].unit.hours + 1) % 24;
            break;
        case 3:
            state->timers[state->current_timer].unit.minutes = (state->timers[state->current_timer].unit.minutes + 1) % 60;
            break;
        case 4:
            state->timers[state->current_timer].unit.seconds = (state->timers[state->current_timer].unit.seconds + 1) % 60;
            break;
        case 5:
            state->timers[state->current_timer].unit.repeat ^= 1;
            break;
        default:
            // should never happen
            break;
    }
    return;
}

static void _abort_quick_cycle(advanced_timer_state_t *state) {
    if (state->quick_cycle) {
        state->quick_cycle = false;
        movement_request_tick_frequency(4);
    }
}

static inline bool _check_for_signal() {
    if (_beeps_to_play) {
        _beeps_to_play = 0;
        return true;
    }
    return false;
}

void advanced_timer_face_setup(uint8_t watch_face_index, void ** context_ptr) {

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(advanced_timer_state_t));
        advanced_timer_state_t *state = (advanced_timer_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(advanced_timer_state_t));
        state->watch_face_index = watch_face_index;
        for (uint8_t i = 0; i < sizeof(_default_timer_values) / sizeof(uint32_t); i++) {
            state->timers[i].value = _default_timer_values[i];
        }
    }
}

void advanced_timer_face_activate(void *context) {
    advanced_timer_state_t *state = (advanced_timer_state_t *)context;
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "TMR", "TR");
    watch_set_colon();
    if(state->mode == at_running) {
        watch_date_time_t now = watch_rtc_get_date_time();
        state->now_ts = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
        watch_set_indicator(WATCH_INDICATOR_BELL);
    } else {
        state->pausing_seconds = 1;
        _beeps_to_play = 0;
    }
}

bool advanced_timer_face_loop(movement_event_t event, void *context) {
    advanced_timer_state_t *state = (advanced_timer_state_t *)context;
    uint8_t subsecond = event.subsecond;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _draw(state, event.subsecond);
            break;
        case EVENT_TICK:
            if (state->mode == at_running) state->now_ts++;
            else if (state->mode == at_pausing) state->pausing_seconds++;
            else if (state->quick_cycle) {
                if (HAL_GPIO_BTN_ALARM_read()) {
                    _settings_increment(state);
                    subsecond = 0;
                } else _abort_quick_cycle(state);
            }
            _draw(state, subsecond);
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            switch (state->mode) {
                case at_pausing:
                    _reset(state);
                    _beep();
                    break;
                case at_running:
                    movement_illuminate_led();
                    break;
                case at_setting:
                    if (state->erase_timer_flag) {
                        state->timers[state->current_timer].value = 0;
                        state->erase_timer_flag = false;
                    }
                    state->settings_state = (state->settings_state + 1) % 6;
                    if (state->settings_state == 1 && state->timers[state->current_timer].value == 0) state->settings_state = 2;
                    else if (state->settings_state == 5 && (state->timers[state->current_timer].value & 0xFFFFFF) == 0) state->settings_state = 0;
                    break;
                case at_waiting:
                    last_timer = state->current_timer;
                    state->current_timer = (state->current_timer + 1) % TIMER_SLOTS;
                    _set_next_valid_timer(state);
                    break;
                default:
                    break;
            }
            _draw(state, event.subsecond);
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            _abort_quick_cycle(state);
            if (_check_for_signal()) break;;
            switch (state->mode) {
                case at_running:
                    state->mode = at_pausing;
                    state->pausing_seconds = 0;
                    state->paused_left = state->target_ts - state->now_ts;
                    movement_cancel_background_task();
                    _beep();
                    break;
                case at_pausing:
                    _start(state, false);
                    _beep();
                    break;
                case at_setting:
                    _settings_increment(state);
                    subsecond = 0;
                    break;
                case at_waiting:
                    _start(state, true);
                    _beep();
                    break;
            }
            _draw(state, subsecond);
            break;
        case EVENT_LIGHT_LONG_PRESS:
            if (state->mode == at_waiting) {
                state->current_timer = last_timer;
                // initiate settings
                state->mode = at_setting;
                state->settings_state = 0;
                state->erase_timer_flag = false;
                movement_request_tick_frequency(4);
                _beep();
            } else if (state->mode == at_setting) {
                _resume_setting(state);
                _beep();
            }
            _draw(state, event.subsecond);
            break;
        case EVENT_BACKGROUND_TASK:
            // play the alarm
            _beeps_to_play = 4;
            watch_buzzer_play_sequence((int8_t *)_sound_seq_beep, _signal_callback);
            _reset(state);
            if (state->timers[state->current_timer].unit.repeat) _start(state, false);
            break;
        case EVENT_ALARM_LONG_PRESS:
            switch(state->mode) {
                case at_setting:
                    switch (state->settings_state) {
                        case 0:
                            state->current_timer = 0;
                            break;
                        case 2:
                        case 3:
                        case 4:
                            state->quick_cycle = true;
                            movement_request_tick_frequency(8);
                            break;
                        default:
                            break;
                    }
                    break;
                case at_waiting:
                case at_pausing:
                case at_running:
                default:
                    break;
            }
            _draw(state, event.subsecond);
            break;
        case EVENT_ALARM_LONG_UP:
            _abort_quick_cycle(state);
            break;
        // Like a G Shock
        // case EVENT_MODE_BUTTON_DOWN:
        //     printf("Mode down\n");
        //     printf("State %d\n", state->mode);
        //     if (state->mode == at_setting) {

        //             if (state->erase_timer_flag) {
        //                 state->timers[state->current_timer].value = 0;
        //                 state->erase_timer_flag = false;
        //             }
        //             state->settings_state = (state->settings_state + 1) % 6;
        //             if (state->settings_state == 1 && state->timers[state->current_timer].value == 0) state->settings_state = 2;
        //             else if (state->settings_state == 5 && (state->timers[state->current_timer].value & 0xFFFFFF) == 0) state->settings_state = 0;
        //         break;
        //     }
        //     movement_move_to_next_face();
        //     break;
        // case EVENT_MODE_LONG_PRESS:
        //     movement_move_to_face(0);
        //     break;
        case EVENT_TIMEOUT:
            _abort_quick_cycle(state);
            movement_move_to_face(0);
            break;
        default:
            movement_default_loop_handler(event);
            break;
    }

    return true;
}

void advanced_timer_face_resign(void *context) {
    advanced_timer_state_t *state = (advanced_timer_state_t *)context;
    if (state->mode == at_setting) {
        state->settings_state = 0;
        state->mode = at_waiting;
    }
}
