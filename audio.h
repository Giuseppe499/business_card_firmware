#include <stdint.h>

#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

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


consteval int16_t[] populate_sine_wave_table() {
    int16_t table[SINE_WAVE_TABLE_LEN];
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }
    return table;
}

constexpr int16_t sine_wave_table[SINE_WAVE_TABLE_LEN] = populate_sine_wave_table();