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

#pragma once

#include "movement.h"

/*
 * WORLD ARCHERY TARGET ROUNDS FACE
 *
 * Timing for world archery target rounds with 2 auditory signals, 10 seconds
 * preparation and then a countdown for the round itself.
 *
 * Long press light button to switch the configuration between 2 minutes for
 * indoor, reflected by "in" in the top right corner, and 4 minutes for
 * outdoors, indicated by "ou" insted.
 *
 * Start the countdown by pressing the alarm button. The watch will mimic the
 * behavior you would expect in a competition, with the extra possibility of
 * pausing whenever you want. Stages:
 * - 2 audible signals will ring at the start of the 10 seconds preparation
 *   time (the watch will show "rdy")
 * - 1 signal will indicate that round start
 * - 3 final signals indicate a timeout
 *
 * Pressing the alarm button while the timer is running will pause the
 * countdown (during the prepration too). You can reset the timer by pressing
 * the light button.
 */

typedef enum {
    wa_indoor,
    wa_outdoor,
} archery_round_t;

typedef enum {
    archery_paused,
    archery_prepare,
    archery_running,
    archery_reset,
} archery_mode_t;

typedef struct {
    uint32_t target_ts;
    uint32_t now_ts;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t set_minutes;
    uint8_t set_seconds;
    archery_mode_t mode;
    archery_mode_t before_pause_mode;
    archery_round_t round;
    uint8_t watch_face_index;
} archery_state_t;

void archery_face_setup(uint8_t watch_face_index, void ** context_ptr);
void archery_face_activate(void *context);
bool archery_face_loop(movement_event_t event, void *context);
void archery_face_resign(void *context);

#define archery_face ((const watch_face_t){ \
    archery_face_setup, \
    archery_face_activate, \
    archery_face_loop, \
    archery_face_resign, \
    NULL, \
})
