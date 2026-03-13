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
#include "simple_tally_face.h"

static bool _init_val;
#define SIMPLE_TALLY_FACE_MAX 9999
#define SIMPLE_TALLY_FACE_MIN 0

static void draw(simple_tally_state_t *state) {
    char buf[5];
    sprintf(buf, "%4d", state->simple_tally_counter);
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
    return;
}

static inline void button_beep(simple_tally_state_t *state) {
    watch_buzzer_note_t note;
    if (state->simple_tally_counter == SIMPLE_TALLY_FACE_MIN) {
        note = BUZZER_NOTE_C6;
    } else {
        note = BUZZER_NOTE_C7;
    }
    // play a beep as confirmation for a button press (if applicable)
    if (movement_button_should_sound()) {
        watch_buzzer_play_note_with_volume(note, 30, movement_button_volume());
    }
}

void simple_tally_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(simple_tally_state_t));
        memset(*context_ptr, 0, sizeof(simple_tally_state_t));
        // Do any one-time tasks in here; the inside of this conditional happens only at boot.
        _init_val = true;
    }
    // Do any pin or peripheral setup here; this will be called whenever the watch wakes from deep sleep.
}

void simple_tally_face_activate(void *context) {
    simple_tally_state_t *state = (simple_tally_state_t *)context;
    // Handle any tasks related to your watch face coming on screen.
    watch_display_text_with_fallback(WATCH_POSITION_TOP, "TALLY", "TA");
}

static void simple_tally_face_increment(simple_tally_state_t *state) {
    uint8_t increment = 1;
    // This will trigger on a long press so the tally will already have
    // incremented by 1, we add 9 more to get to +10
    if (state->big_increment) {
        increment = 9;
        state->big_increment = false;
    }
    // Do not overflow nor loop back to zero
    if (state->simple_tally_counter < SIMPLE_TALLY_FACE_MAX){
        state->simple_tally_counter += increment;
    }
    _init_val = false;
    state->big_increment = false;
    draw(state);
}

static void simple_tally_face_decrement(simple_tally_state_t *state) {
        // Do not overflow nor loop back to zero
        if (state->simple_tally_counter > SIMPLE_TALLY_FACE_MIN){
            state->simple_tally_counter--;
            _init_val = false;
        } else {
            state->simple_tally_counter = SIMPLE_TALLY_FACE_MIN;
            _init_val = true;
        }
        draw(state);
}

bool simple_tally_face_loop(movement_event_t event, void *context) {
    simple_tally_state_t *state = (simple_tally_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            // Show your initial UI here.
            draw(state);
            break;
        case EVENT_TICK:
            // If needed, update your display here.
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            // You can use the Light button for your own purposes. Note that by default, Movement will also
            // illuminate the LED in response to EVENT_LIGHT_BUTTON_DOWN; to suppress that behavior, add an
            // empty case for EVENT_LIGHT_BUTTON_DOWN.
            movement_illuminate_led();
            break;
        case EVENT_LIGHT_LONG_PRESS:
            simple_tally_face_decrement(state);
            button_beep(state);
            draw(state);
            break;
        case EVENT_LIGHT_REALLY_LONG_PRESS:
            if (!_init_val) {
                state->simple_tally_counter = SIMPLE_TALLY_FACE_MIN;
                _init_val = true;
            }
            button_beep(state);
            draw(state);
            break;
        case EVENT_ALARM_LONG_PRESS:
            state->big_increment = true;
        case EVENT_ALARM_BUTTON_DOWN:
            simple_tally_face_increment(state);
            button_beep(state);
            draw(state);
            break;
        case EVENT_TIMEOUT:
            // Your watch face will receive this event after a period of inactivity. If it makes sense to resign,
            // you may uncomment this line to move back to the first watch face in the list:
            movement_move_to_face(0);
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

void simple_tally_face_resign(void *context) {
    (void) context;

    // handle any cleanup before your watch face goes off-screen.
}

