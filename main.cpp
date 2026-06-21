/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#if PICO_ON_DEVICE

#include "hardware/gpio.h"
const uint BUTTON_PINS[] = {0,1,2};

#endif

#include "pico/stdlib.h"
#include "audio.h"

int main() {
    #if PICO_ON_DEVICE
    // Setup GPIO for buttons
    for (uint gpio_pin : BUTTON_PINS){
        gpio_init(gpio_pin);
        gpio_set_dir(gpio_pin, GPIO_IN);
    }
    #endif

    stdio_init_all();

    struct audio_buffer_pool *ap = init_audio();
    // Compute the step size for a 440Hz tone
    uint32_t step = step_size_for_freq(440);
    uint32_t pos = 0;
    uint32_t pos_max = STEP_MULTIPLIER * SINE_WAVE_TABLE_LEN;
    uint vol = 128;
    while (true) {
#if USE_AUDIO_PWM
        enum audio_correction_mode m = audio_pwm_get_correction_mode();
#endif
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == '-' && vol) vol -= 4;
            if ((c == '=' || c == '+') && vol < 255) vol += 4;
            if (c == '[' && step > STEP_MULTIPLIER) step -= STEP_MULTIPLIER;
            if (c == ']' && step < (SINE_WAVE_TABLE_LEN / 8) * STEP_MULTIPLIER) step += STEP_MULTIPLIER;
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
            printf("vol = %d, freq = %f, step = %d mode = %d      \r", vol, freq_for_step_size(step), step / STEP_MULTIPLIER, m);
#else
            printf("vol = %d, freq = %f, step = %d      \r", vol, freq_for_step_size(step), step / STEP_MULTIPLIER);
#endif
        }
        for(uint gpio_pin : BUTTON_PINS) {
            if (gpio_get(gpio_pin)) {
                printf("Button %d pressed\n", gpio_pin);
            }
        }
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = (vol * sine_wave_table[pos / STEP_MULTIPLIER]) >> 8u;
            pos += step;
            if (pos >= pos_max) pos -= pos_max;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    puts("\n");
    return 0;
}