/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <array>
#include <vector>
#include "pico/stdlib.h"
#include "audio.h"
#include "synth.h"

#if PICO_ON_DEVICE

#include "hardware/gpio.h"
constexpr uint KEYBOARD_PINS[] = {0,1,2,3,4,5,7,8,9,10,11,12,13};
constexpr int N_KEYBOARD = sizeof(KEYBOARD_PINS) / sizeof(KEYBOARD_PINS[0]);
constexpr uint FUNCTION_PINS[] = {16,17,18,19};
constexpr int N_FUNCTION = sizeof(FUNCTION_PINS) / sizeof(FUNCTION_PINS[0]);
bool FUNCTION_PINS_STATE[N_FUNCTION] = {false};
constexpr uint VOL_UP_FUNC_PIN_IDX = 2;
constexpr uint VOL_DOWN_FUNC_PIN_IDX = 3;
constexpr uint OCTAVE_UP_FUNC_PIN_IDX = 0;
constexpr uint OCTAVE_DOWN_FUNC_PIN_IDX = 1;

#endif

void change_volume(uint &vol, int delta) {
    int new_vol = static_cast<int>(vol) + delta;
    if (new_vol < 0) new_vol = 0;
    if (new_vol > 255) new_vol = 255;
    vol = static_cast<uint>(new_vol);
}

void change_octave(int &octave_shift, int delta) {
    int new_octave = octave_shift + delta;
    if (new_octave < lowest_octave) new_octave = lowest_octave;
    if (new_octave > highest_octave) new_octave = highest_octave;
    octave_shift = new_octave;
}

bool get_function_pressed(uint func_pin_idx) {
    auto current_state = gpio_get(FUNCTION_PINS[func_pin_idx]);
    auto pressed = current_state && !FUNCTION_PINS_STATE[func_pin_idx];
    FUNCTION_PINS_STATE[func_pin_idx] = current_state;
    return pressed;
}

int main() {
    stdio_init_all();

    #if PICO_ON_DEVICE
    // Turn on the built-in LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    // Setup GPIO for volume and octave buttons
    for (uint gpio_pin : FUNCTION_PINS){
        gpio_init(gpio_pin);
        gpio_set_dir(gpio_pin, GPIO_IN);
        gpio_set_pulls(gpio_pin, false, true);
    }
    // Setup GPIO for keyboard buttons
    for (uint gpio_pin : KEYBOARD_PINS){
        gpio_init(gpio_pin);
        gpio_set_dir(gpio_pin, GPIO_IN);
        gpio_set_pulls(gpio_pin, false, false);
    }
    #endif

    uint vol = 64;
    int octave_shift = 0;

    OrganSynth organ_synth = OrganSynth();

    struct audio_buffer_pool *ap = init_audio();

    while (true) {
#if USE_AUDIO_PWM
        enum audio_correction_mode m = audio_pwm_get_correction_mode();
#endif
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == '-' && vol) change_volume(vol, -4);
            if ((c == '=' || c == '+') && vol < 255) change_volume(vol, 4);
            if (c == 'w') {
                change_octave(octave_shift, 1);
            }
            if (c == 's') {
                change_octave(octave_shift, -1);
            }
            if (c == 'q') break;
#if USE_AUDIO_PWM
            if (c == 'c') {
                bool done = false;
                while (!done) {
                    if (m == none) m = fixed_dither;
                    else if (m == fixed_dither) m = dither;
                    else if (m == dither) m = noise_shaped_dither;
                    else if (m == noise_shaped_dither) m = none;
                    done = audio_pwm_set_correction_mode(m);
                }
            }
            printf("vol = %d, octave_shift = %d, mode = %d\r", vol, octave_shift, m);
#else
            printf("vol = %d, octave_shift = %d\r", vol, octave_shift);
#endif
        }
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;

        // Handle volume and octave buttons
        if (get_function_pressed(VOL_UP_FUNC_PIN_IDX)) change_volume(vol, 4);
        if (get_function_pressed(VOL_DOWN_FUNC_PIN_IDX)) change_volume(vol, -4);
        if (get_function_pressed(OCTAVE_UP_FUNC_PIN_IDX)) change_octave(octave_shift, 1);
        if (get_function_pressed(OCTAVE_DOWN_FUNC_PIN_IDX)) change_octave(octave_shift, -1);

        // Read the state of the keyboard buttons and determine which notes are currently pressed
        std::vector<int> notes_idxs;
        int j = octave_shift*12; // Start from the lowest note of the current octave shift
        for(uint gpio_pin : KEYBOARD_PINS) {
            if (gpio_get(gpio_pin)) {
                notes_idxs.push_back(j);
            }
            j++;
        }

        // Generate audio samples for the current notes and fill the audio buffer
        std::array<amplitude_t, SAMPLES_PER_BUFFER> organ_samples = organ_synth.next_samples<SAMPLES_PER_BUFFER>(notes_idxs);
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = static_cast<int16_t>(organ_samples[i] / 255 * vol * 32767);
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }

    return 0;
}