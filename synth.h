#pragma once

#include <stdint.h>
#include "audio.h"
#include <tuple>
#include <array>
#include <vector>
#include <fpm/fixed.hpp>

#define USE_FIXED_POINT
#ifdef USE_FIXED_POINT
    using amplitude_t = fpm::fixed<int32_t, int32_t, 16, false>;
#else
    using amplitude_t = float;
#endif

#define SINE_WAVE_TABLE_LEN 2048
#define STEP_MULTIPLIER 0x10000
#define MAX_POSITION (STEP_MULTIPLIER * SINE_WAVE_TABLE_LEN)

constexpr float sine_freq = AUDIO_SAMPLE_FREQ / (float)SINE_WAVE_TABLE_LEN;

constexpr uint32_t step_size_for_freq(float freq) {
    return (uint32_t)(freq * STEP_MULTIPLIER / sine_freq);
}

constexpr float freq_for_step_size(uint32_t step) {
    return (float)step / STEP_MULTIPLIER * sine_freq;
}

constexpr auto sine_wave_table = [](){
    std::array<amplitude_t, SINE_WAVE_TABLE_LEN> table{};
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        table[i] = static_cast<amplitude_t>(std::cos(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN)));
    }
    return table;
}();

class SineOscillator {
public:
    SineOscillator() : frequency(0), step_size(0), position(0) {}
    SineOscillator(float freq) : frequency(freq), step_size(step_size_for_freq(freq)), position(0) {}
    SineOscillator(uint32_t step) : frequency(freq_for_step_size(step)), step_size(step), position(0) {}

    void set_frequency(float freq) {
        frequency = freq;
        step_size = step_size_for_freq(freq);
    }

    amplitude_t next_sample(amplitude_t step_multiplier = 1) {
        amplitude_t sample = sine_wave_table[position / STEP_MULTIPLIER];
        position = (position + static_cast<uint32_t>(step_size * step_multiplier)) % MAX_POSITION;
        return sample;
    }

    void reset() {
        position = 0;
    }

private:
    float frequency;
    uint32_t step_size;
    uint32_t position;
};

constexpr float base_freq = 261.63; // C4
constexpr int lowest_octave = -2; // C1
constexpr int lowest_note = 12*lowest_octave; // C1
constexpr int highest_octave = 2; // C6
constexpr int highest_note = 12*(highest_octave+1); // C6
constexpr int base_freq_idx = -lowest_note;
constexpr int lowest_tonewheel_note = lowest_note - 12*2; // C-1
constexpr int highest_tonewheel_note = highest_note + 12*3;
constexpr int num_tonewheel_notes = highest_tonewheel_note - lowest_tonewheel_note + 1;
constexpr std::array<uint32_t, num_tonewheel_notes> organ_step_sizes = []() {
    std::array<uint32_t, num_tonewheel_notes> step_sizes{};
    int idx = 0;
    for (int note = lowest_tonewheel_note; note < highest_tonewheel_note; note++) {
        float freq = base_freq * std::pow(2.0f, note / 12.0f);
        step_sizes[idx++] = step_size_for_freq(freq);
    }
    return step_sizes;
}();
constexpr int harmonics[] = {-12*2, -12, 0, 7, 12, 19, 24, 28, 31, 36}; // frequencies multiples {.25, .5, 1, 1.5, 2, 3, 4, 5, 6, 8};
constexpr float amplitudes_unnormalized[] = {0.6, 0.3, 1, 0.5, .3, .2, .1, .1, .1, .1};
constexpr int num_harmonics = sizeof(harmonics) / sizeof(harmonics[0]);
constexpr std::array<amplitude_t, num_harmonics> amplitudes = []() {
    std::array<float, num_harmonics> amps{};
    float sum_amplitudes = 0;
    for (float amp : amplitudes_unnormalized) {
        sum_amplitudes += amp;
    }
    sum_amplitudes *= 1; // Slightly increase the sum to avoid clipping
    for (size_t i = 0; i < num_harmonics; i++) {
        amps[i] = amplitudes_unnormalized[i] / sum_amplitudes;
    }
    std::array<amplitude_t, num_harmonics> amps_int{};
    for (size_t i = 0; i < num_harmonics; i++) {
        amps_int[i] = static_cast<amplitude_t>(amps[i]);
    }
    return amps_int;
}();

constexpr auto LESLIE_CUTOFF_IDX = []() {
    float LESLIE_CUTOFF_FREQ = 800.0f; // cross-over frequency for the Leslie effect
    uint32_t LESLIE_CUTOFF_STEP_SIZE = step_size_for_freq(LESLIE_CUTOFF_FREQ);
    int j = 0;
    for (auto step_size : organ_step_sizes) {
        if (step_size >= LESLIE_CUTOFF_STEP_SIZE) {
            return j;
        }
        j++;
    }
    return j;
}();

class OrganSynth {
    public:
        OrganSynth(){
            for (int i = 0; i < num_tonewheel_notes; i++) {
                tonewheels[i] = SineOscillator(organ_step_sizes[i]);
            }
            leslie = SineOscillator(6.8f); // frequency of the leslie effect in Hz
        }
    template <uint32_t buffer_size>
    std::array<amplitude_t, buffer_size> next_samples(std::vector<int> &notes_idxs) {
        std::array<amplitude_t, num_tonewheel_notes> tonewheel_amplitudes = {amplitude_t(0)};
        std::array<uint8_t, num_tonewheel_notes> active_idx {};
        int active_count = 0;
        for (int i = 0; i < notes_idxs.size(); i++) {
            if (notes_idxs[i] < lowest_tonewheel_note || notes_idxs[i] > highest_tonewheel_note) {
                continue;
            }
            int idx = notes_idxs[i] - lowest_tonewheel_note;
            for (int h = 0; h < num_harmonics; h++) {
                int harmonic_idx = idx + harmonics[h];
                if (tonewheel_amplitudes[harmonic_idx] <= amplitude_t(0)){
                    active_idx[active_count++] = harmonic_idx;
                }
                tonewheel_amplitudes[harmonic_idx] += amplitudes[h];
            }
        }
        std::array<amplitude_t, buffer_size> samples{};
        int current_idx = 0;
        for (int j = 0; j < buffer_size; j++) {
            amplitude_t leslie_sample = leslie.next_sample();
            int leslie_modulation_HF = amplitude_t(.9) + amplitude_t(.1) * leslie_sample;
            int leslie_modulation_LF = amplitude_t(.9) + amplitude_t(.1) * leslie_sample;
            amplitude_t leslie_amp_modulation_HF = amplitude_t(.9) + amplitude_t(.1) * leslie_sample;
            amplitude_t leslie_amp_modulation_LF = amplitude_t(.9) + amplitude_t(.1) * leslie_sample;
            for (int i = 0; i < active_count; i++) {
                current_idx = active_idx[i];
                if (current_idx < LESLIE_CUTOFF_IDX) {
                    samples[j] += tonewheels[current_idx].next_sample(leslie_modulation_LF) * tonewheel_amplitudes[current_idx] * leslie_amp_modulation_LF;
                } else {
                    samples[j] += tonewheels[current_idx].next_sample(leslie_modulation_HF) * tonewheel_amplitudes[current_idx] * leslie_amp_modulation_HF;
                }
            }
        }
        return samples;
    }
    private:
        std::array<SineOscillator, num_tonewheel_notes> tonewheels;
        SineOscillator leslie;
};