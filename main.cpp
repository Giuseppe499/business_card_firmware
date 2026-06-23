/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <array>
#include "pico/stdlib.h"
#include "audio.h"
#include "synth.h"

#if PICO_ON_DEVICE

#include "hardware/gpio.h"
constexpr uint BUTTON_PINS[] = {0,1,2};
constexpr float FREQS[] = {261.63, 293.66, 329.63};

#endif

int main() {
    stdio_init_all();

    #if PICO_ON_DEVICE
    // Setup GPIO for buttons
    for (uint gpio_pin : BUTTON_PINS){
        gpio_init(gpio_pin);
        gpio_set_dir(gpio_pin, GPIO_IN);
    }
    #endif

    uint vol = 64;

    std::array<Synth*, sizeof(BUTTON_PINS)/sizeof(BUTTON_PINS[0])> oscillators;
    for (size_t i = 0; i < oscillators.size(); i++) {
        oscillators[i] = new OrganSynth(FREQS[i]);
    }

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
                    samples[i] += (oscillators[j]->next_sample() >> 8);
                }
            }
            j++;
        }
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] *= vol;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }

    // Cleanup
    for (size_t i = 0; i < oscillators.size(); i++) {
        delete oscillators[i];
    }

    return 0;
}