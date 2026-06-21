/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <array>
#include "pico/stdlib.h"
#include "audio.h"

#if PICO_ON_DEVICE

#include "hardware/gpio.h"
constexpr uint BUTTON_PINS[] = {0,1,2};
constexpr float FREQS[] = {261.63, 293.66, 329.63};
consteval std::array<uint32_t, sizeof(FREQS)/sizeof(FREQS[0])> populate_steps() {
    std::array<uint32_t, sizeof(FREQS)/sizeof(FREQS[0])> steps{};
    for (int i = 0; i < steps.size(); i++) {
        steps[i] = step_size_for_freq(FREQS[i]);
    }
    return steps;
}
constexpr auto STEPS = populate_steps();

#endif

int main() {
    #if PICO_ON_DEVICE
    // Setup GPIO for buttons
    for (uint gpio_pin : BUTTON_PINS){
        gpio_init(gpio_pin);
        gpio_set_dir(gpio_pin, GPIO_IN);
    }
    uint32_t positions[sizeof(BUTTON_PINS)/sizeof(BUTTON_PINS[0])] = {0};
    #endif

    uint32_t pos_max = STEP_MULTIPLIER * SINE_WAVE_TABLE_LEN;
    uint vol = 64;

    stdio_init_all();

    struct audio_buffer_pool *ap = init_audio();

    while (true) {
#if USE_AUDIO_PWM
        enum audio_correction_mode m = audio_pwm_get_correction_mode();
#endif
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == '-' && vol) vol -= 4;
            if ((c == '=' || c == '+') && vol < 255) vol += 4;
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
            printf("vol = %d, mode = %d      \r", vol, m);
#else
            printf("vol = %d,      \r", vol);
#endif
        }
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = 0;
        }
        int j = 0;
        for(uint gpio_pin : BUTTON_PINS) {
            if (gpio_get(gpio_pin)) {
                for (uint i = 0; i < buffer->max_sample_count; i++) {
                    samples[i] += (vol * sine_wave_table[positions[j] / STEP_MULTIPLIER]) >> 8u;
                    positions[j] += STEPS[j];
                    if (positions[j] >= pos_max) positions[j] -= pos_max;
                }
            }
            else {
                positions[j] = 0;
            }
            j++;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    puts("\n");
    return 0;
}