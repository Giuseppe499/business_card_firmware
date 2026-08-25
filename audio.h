#pragma once

#include <stdint.h>
#include <array>
#include <cmath>

#if USE_AUDIO_I2S

#include "pico/audio_i2s.h"

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE+1, "I2S LRCK"));
#endif

#elif USE_AUDIO_PWM
#include "pico/audio_pwm.h"
#elif USE_AUDIO_SPDIF
#include "pico/audio_spdif.h"
#endif

#if USE_AUDIO_SPDIF || USE_AUDIO_I2S
    #define AUDIO_SAMPLE_FREQ 44100
#else
    #define AUDIO_SAMPLE_FREQ 24000
#endif

struct audio_buffer_pool *init_audio();

#define SAMPLES_PER_BUFFER 256