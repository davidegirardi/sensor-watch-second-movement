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
#include "button_names_face.h"

void button_names_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(button_names_state_t));
        memset(*context_ptr, 0, sizeof(button_names_state_t));
        // Do any one-time tasks in here; the inside of this conditional happens only at boot.
    }
    // Do any pin or peripheral setup here; this will be called whenever the watch wakes from deep sleep.
}

void button_names_face_activate(void *context) {
    button_names_state_t *state = (button_names_state_t *)context;

    // Handle any tasks related to your watch face coming on screen.
}

bool button_names_face_loop(movement_event_t event, void *context) {
    button_names_state_t *state = (button_names_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            gshock_display_current_time_top_right();
            watch_display_text(WATCH_POSITION_TOP, "btn ");
            break;
        case EVENT_TICK:
            // If needed, update your display here.
            break;
        case EVENT_MODE_BUTTON_DOWN:
            watch_display_text(WATCH_POSITION_BOTTOM, "nnode ");
            break;
        case EVENT_ALARM_BUTTON_DOWN:
            watch_display_text(WATCH_POSITION_BOTTOM, "  ALA  ");
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            watch_display_text(WATCH_POSITION_BOTTOM, "Light ");
           break;
        case EVENT_START_BUTTON_DOWN:
            watch_display_text(WATCH_POSITION_BOTTOM, "StArt ");
            break;
        case EVENT_MODE_LONG_PRESS:
            movement_move_to_next_face();
            break;
        case EVENT_START_BUTTON_UP:
        case EVENT_MODE_BUTTON_UP:
        case EVENT_TIMEOUT:
            // Your watch face will receive this event after a period of inactivity. If it makes sense to resign,
            // you may uncomment this line to move back to the first watch face in the list:
            // movement_move_to_face(0);
            break;
#ifdef FORCE_GSHOCK_LCD_TYPE
        case EVENT_MINUTE:
            gshock_display_current_time_top_right();
            break;
#endif
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

void button_names_face_resign(void *context) {
    (void) context;

    // handle any cleanup before your watch face goes off-screen.
}

