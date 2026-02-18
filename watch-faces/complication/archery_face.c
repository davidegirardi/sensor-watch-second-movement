/*
 * MIT License
 *
 * Copyright (c) 2026 <#author_name#>
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
#include "watch_utility.h"
#include "archery_face.h"

#define WAIT_TIME_SECONDS 10
#define INDOOR_RUN_MINUTES 2
#define OUTDOOR_RUN_MINUTES 4

static const int8_t _sound_seq_prepare[] = {BUZZER_NOTE_C6, 40, BUZZER_NOTE_REST, 40, -2, 1, 0};
static const int8_t _sound_seq_start[] = {BUZZER_NOTE_C7, 50, 0};
static const int8_t _sound_seq_30s[] = {BUZZER_NOTE_C6, 1, BUZZER_NOTE_REST, 2, -2, 3, 0};
static const int8_t _sound_seq_end[] = {BUZZER_NOTE_C7, 40, BUZZER_NOTE_REST, 40, -2, 2, 0};

static inline void load_countdown(archery_state_t *state) {
    /* Load set countdown time */
    state->minutes = state->set_minutes;
    state->seconds = state->set_seconds;
}

static inline void store_countdown(archery_state_t *state) {
    /* Store set countdown time */
    state->set_minutes = state->minutes;
    state->set_seconds = state->seconds;
}

static inline void button_beep() {
    // play a beep as confirmation for a button press (if applicable)
    if (movement_button_should_sound()) watch_buzzer_play_note_with_volume(BUZZER_NOTE_C8, 20, movement_button_volume());
}

static void schedule_countdown(archery_state_t *state) {
    // Calculate the new state->now_ts but don't update it until we've updated the target
    // avoid possible race where the old target is compared to the new time and immediately triggers
    //
    uint32_t new_now = watch_utility_date_time_to_unix_time(movement_get_utc_date_time(), movement_get_current_timezone_offset());
    state->target_ts = watch_utility_offset_timestamp(new_now, 0, state->minutes, state->seconds);
    state->now_ts = new_now;
    watch_date_time_t target_dt = watch_utility_date_time_from_unix_time(state->target_ts, movement_get_current_timezone_offset());
    movement_schedule_background_task_for_face(state->watch_face_index, target_dt);
}

static void start(archery_state_t *state) {
    schedule_countdown(state);
}

static void draw(archery_state_t *state, uint8_t subsecond) {
    char buf[16];
    uint32_t delta;
    div_t result;
    switch (state->mode) {
        case archery_prepare:
        case archery_running:
            if (state->target_ts <= state->now_ts)
                delta = 0;
            else
                delta = state->target_ts - state->now_ts;
            result = div(delta, 60);
            state->seconds = result.rem;
            result = div(result.quot, 60);
            state->minutes = result.rem;
            sprintf(buf, "  %02d%02d", state->minutes, state->seconds);
            break;
        case archery_reset:
        case archery_paused:
            watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
            sprintf(buf, "  %02d%02d", state->minutes, state->seconds);
            break;
        case archery_setting:
            sprintf(buf, "  %02d%02d", state->minutes, state->seconds);
            if (subsecond % 2) {
                switch(state->selection) {
                    case 0:
                        buf[0] = buf[1] = ' ';
                        break;
                    case 1:
                        buf[2] = buf[3] = ' ';
                        break;
                    case 2:
                        buf[4] = buf[5] = ' ';
                        break;
                    default:
                        break;
                }
            }
            break;
    }
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
}

static void pause(archery_state_t *state) {
    state->prev_mode = state->mode;
    state->mode = archery_paused;
    movement_cancel_background_task_for_face(state->watch_face_index);
    watch_clear_indicator(WATCH_INDICATOR_SIGNAL);
}

static void reset(archery_state_t *state) {
    state->mode = archery_reset;
    movement_cancel_background_task_for_face(state->watch_face_index);
    load_countdown(state);
}

static void manage_stages(archery_state_t *state) {
    if (state->mode == archery_prepare) {
        state->minutes = INDOOR_RUN_MINUTES;
        state->seconds = 0;
        watch_buzzer_play_sequence((int8_t *)_sound_seq_start, NULL);
        state->mode = archery_running;
        schedule_countdown(state);
    } else {
        watch_buzzer_play_sequence((int8_t *)_sound_seq_end, NULL);
        reset(state);
    }
}

void archery_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(archery_state_t));
        archery_state_t *state = (archery_state_t *)*context_ptr;
        memset(*context_ptr, 0, sizeof(archery_state_t));
        state->minutes = INDOOR_RUN_MINUTES;
        state->mode = archery_reset;
        state->watch_face_index = watch_face_index;
        store_countdown(state);
    }
}

void archery_face_activate(void *context) {
    archery_state_t *state = (archery_state_t *)context;
    if(state->mode == archery_running) {
        watch_date_time_t now = movement_get_utc_date_time();
        state->now_ts = watch_utility_date_time_to_unix_time(now, movement_get_current_timezone_offset());
        watch_set_indicator(WATCH_INDICATOR_SIGNAL);
    }
    movement_request_tick_frequency(1);
}

bool archery_face_loop(movement_event_t event, void *context) {
    archery_state_t *state = (archery_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            if (watch_sleep_animation_is_running()) watch_stop_sleep_animation();
            watch_display_text(WATCH_POSITION_TOP, "WA");
            draw(state, event.subsecond);
            break;
        case EVENT_TICK:
            if (state->mode == archery_running || state->mode == archery_prepare) {
                state->now_ts++;
            }
            draw(state, event.subsecond);
            break;
        case EVENT_LIGHT_BUTTON_UP:
            // You can use the Light button for your own purposes. Note that by default, Movement will also
            // illuminate the LED in response to EVENT_LIGHT_BUTTON_DOWN; to suppress that behavior, add an
            // empty case for EVENT_LIGHT_BUTTON_DOWN.
            if (state->mode == archery_paused) {
                reset(state);
                button_beep();
            }
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            switch (state->mode) {
                case archery_prepare:
                case archery_running:
                    pause(state);
                    button_beep();
                    break;
                case archery_reset:
                    state->minutes = 0;
                    state->seconds = 10;
                    watch_buzzer_play_sequence((int8_t *)_sound_seq_prepare, NULL);
                    state->mode = archery_prepare;
                    start(state);
                    watch_set_indicator(WATCH_INDICATOR_SIGNAL);
                    break;
                case archery_paused:
                    state->mode = state->prev_mode;
                    start(state);
                    button_beep();
                    watch_set_indicator(WATCH_INDICATOR_SIGNAL);
                    break;
                case archery_setting:
                    //settings_increment(state);
                    break;
            }
            draw(state, event.subsecond);
            break;
        case EVENT_ALARM_LONG_PRESS:
            // Just in case you have need for another button.
            break;
        case EVENT_BACKGROUND_TASK:
            manage_stages(state);
            break;
        case EVENT_TIMEOUT:
            // Your watch face will receive this event after a period of inactivity. If it makes sense to resign,
            // you may uncomment this line to move back to the first watch face in the list:
            // movement_move_to_face(0);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
            // If you did not resign in EVENT_TIMEOUT, you can use this event to update the display once a minute.
            // Avoid displaying fast-updating values like seconds, since the display won't update again for 60 seconds.
            // You should also consider starting the tick animation, to show the wearer that this is sleep mode:
            // watch_start_sleep_animation(500);
            break;
        default:
            // Movement's default loop handler will step in for any cases you don't handle above:
            // * EVENT_LIGHT_BUTTON_DOWN lights the LED
            // * EVENT_MODE_BUTTON_UP moves to the next watch face in the list
            // * EVENT_MODE_LONG_PRESS returns to the first watch face (or skips to the secondary watch face, if configured)
            // You can override any of these behaviors by adding a case for these events to this switch statement.
            return movement_default_loop_handler(event);
    }

    // return true if the watch can enter standby mode. Generally speaking, you should always return true.
    return true;
}

void archery_face_resign(void *context) {
    (void) context;

    // handle any cleanup before your watch face goes off-screen.
}

