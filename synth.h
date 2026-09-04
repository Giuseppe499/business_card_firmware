#pragma once

#include <stdint.h>
#include "audio.h"
#include <tuple>
#include <array>
#include <vector>
#include <fpm/fixed.hpp>

// #define USE_FIXED_POINT // On the rp2040, fixed point math is faster than float
#ifdef USE_FIXED_POINT
    using amplitude_t = fpm::fixed<int32_t, int32_t, 16, false>;
    using position_t = fpm::fixed<uint32_t, uint64_t, 19, false>;
#else
    using amplitude_t = float;
    using position_t = float;
#endif

#define SINE_WAVE_TABLE_LEN 0x1000
constexpr auto sine_wave_table = [](){
    std::array<amplitude_t, SINE_WAVE_TABLE_LEN> table{};
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        table[i] = static_cast<amplitude_t>(std::cos(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN)));
    }
    return table;
}();

constexpr position_t step_size_for_freq(float freq) {
    return (position_t)(freq / AUDIO_SAMPLE_FREQ);
}

constexpr float freq_for_step_size(position_t step) {
    return (float)step * AUDIO_SAMPLE_FREQ;
}

class SineOscillator {
public:
    SineOscillator() : step_size(0), position(0) {}

    void set_frequency(float freq) {
        step_size = step_size_for_freq(freq);
    }

    void set_step_size(position_t step) {
        step_size = step;
    }

    float get_frequency() const {
        return freq_for_step_size(step_size);
    }

    position_t get_step_size() const {
        return step_size;
    }

    position_t get_position() const {
        return position;
    }

    void set_position(position_t pos) {
        while (pos >= position_t(1)){
            pos -= 1;
        }
        position = pos;
    }

    void continuous_update_step_size(position_t target_step_size, position_t acceleration) {
        if (step_size < target_step_size) {
            step_size += acceleration;
            if (step_size > target_step_size) {
                step_size = target_step_size;
            }
        } else if (step_size > target_step_size) {
            step_size -= acceleration;
            if (step_size < target_step_size) {
                step_size = target_step_size;
            }
        }
    }

    amplitude_t next_sample(position_t step_multiplier = position_t(1)) {
        size_t idx = static_cast<size_t>(position * static_cast<position_t>(SINE_WAVE_TABLE_LEN));
        amplitude_t sample = sine_wave_table[idx];
        position += step_size * step_multiplier;
        while (position >= position_t(1)){
            position -= 1;
        }
        return sample;
    }

    void reset() {
        position = position_t(0);
    }

private:
    position_t step_size;
    position_t position;
};

constexpr float base_freq = 261.63; // C4
constexpr int lowest_octave = -2;
constexpr int lowest_note = 12*lowest_octave;
constexpr int highest_octave = 2;
constexpr int highest_note = 12*highest_octave;
constexpr int base_freq_idx = -lowest_note;
constexpr int lowest_tonewheel_note = lowest_note - 12;
constexpr int highest_tonewheel_note = highest_note + 12 + 6;
constexpr int num_tonewheel_notes = highest_tonewheel_note - lowest_tonewheel_note + 1;
constexpr std::array<position_t, num_tonewheel_notes> organ_step_sizes = []() {
    std::array<position_t, num_tonewheel_notes> step_sizes{};
    int idx = 0;
    for (int note = lowest_tonewheel_note; note <= highest_tonewheel_note; note++) {
        float freq = base_freq * std::pow(2.0f, note / 12.0f);
        step_sizes[idx++] = step_size_for_freq(freq);
    }
    return step_sizes;
}();
constexpr int harmonics[] = {-12, 0, 7, 12, 19, 24, 28, 31, 36}; // frequencies multiples {.5, 1, 1.5, 2, 3, 4, 5, 6, 8};
constexpr float amplitudes_unnormalized[] = {0.5, 1, 0.5, .3, .2, .1, .1, .1, .1};
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
    position_t LESLIE_CUTOFF_STEP_SIZE = step_size_for_freq(LESLIE_CUTOFF_FREQ);
    int j = 0;
    for (auto step_size : organ_step_sizes) {
        if (step_size >= LESLIE_CUTOFF_STEP_SIZE) {
            return j;
        }
        j++;
    }
    return j;
}();
constexpr auto LESLIE_TREMOLO_HF_STEP_SIZE = step_size_for_freq(6.7f); // frequency of the leslie effect in Hz
constexpr auto LESLIE_TREMOLO_LF_STEP_SIZE = step_size_for_freq(5.8f); // frequency of the leslie effect in Hz
constexpr auto LESLIE_CHORALE_HF_STEP_SIZE = step_size_for_freq(0.8f); // frequency of the leslie effect in Hz
constexpr auto LESLIE_CHORALE_LF_STEP_SIZE = step_size_for_freq(0.7f); // frequency of the leslie effect in Hz

struct PreparedNotes {
    std::array<amplitude_t, num_tonewheel_notes> tonewheel_amplitudes{};
    std::array<uint8_t, num_tonewheel_notes> active_idx{};
    uint8_t active_count = 0;
};

class OrganSynth {
    public:
    position_t leslie_target_step_size_HF;
    position_t leslie_target_step_size_LF;
    OrganSynth(){
        for (int i = 0; i < num_tonewheel_notes; i++) {
            tonewheels[i] = SineOscillator();
            tonewheels[i].set_step_size(organ_step_sizes[i]);
        }
        leslie_am_HF = SineOscillator();
        leslie_fm_HF = SineOscillator();
        leslie_fm_HF.set_position(static_cast<position_t>(.25)); // phase shift the FM oscillator by 90 degrees
        leslie_target_step_size_HF = position_t(0);
        leslie_am_LF = SineOscillator();
        leslie_fm_LF = SineOscillator();
        leslie_fm_LF.set_position(static_cast<position_t>(.25)); // phase shift the FM oscillator by 90 degrees
        leslie_target_step_size_LF = position_t(0);
    }

    static PreparedNotes prepare_notes(const std::vector<int> &notes_idxs) {
        PreparedNotes notes;
        for (int note_idx : notes_idxs) {
            int idx = note_idx - lowest_tonewheel_note;
            for (int h = 0; h < num_harmonics; h++) {
                int harmonic_idx = idx + harmonics[h];
                while (harmonic_idx >= num_tonewheel_notes) {
                    harmonic_idx -= 12; // wrap around to the previous octave
                }
                while (harmonic_idx < 0) {
                    harmonic_idx += 12; // wrap around to the next octave
                }
                if (notes.tonewheel_amplitudes[harmonic_idx] <= amplitude_t(0)) {
                    notes.active_idx[notes.active_count++] = harmonic_idx;
                }
                notes.tonewheel_amplitudes[harmonic_idx] += amplitudes[h];
            }
        }
        return notes;
    }

    template <size_t buffer_size>
    std::array<amplitude_t, buffer_size> next_samples(const std::vector<int> &notes_idxs) {
        return render_samples<buffer_size>(prepare_notes(notes_idxs));
    }

    template <size_t buffer_size>
    std::array<amplitude_t, buffer_size> render_samples(const PreparedNotes &notes) {
        std::array<amplitude_t, buffer_size> samples{};
        int current_idx = 0;
        for (int j = 0; j < buffer_size; j++) {
            // Accelerate or decelerate the leslie effect to reach the target step size
            leslie_am_HF.continuous_update_step_size(leslie_target_step_size_HF, static_cast<position_t>(2e-9));
            leslie_fm_HF.set_step_size(leslie_am_HF.get_step_size());
            leslie_am_LF.continuous_update_step_size(leslie_target_step_size_LF, static_cast<position_t>(7e-10));
            leslie_fm_LF.set_step_size(leslie_am_LF.get_step_size());
            // Generate the next sample for the leslie effect
            amplitude_t leslie_am__HF_sample = leslie_am_HF.next_sample();
            amplitude_t leslie_fm_HF_sample = leslie_fm_HF.next_sample();
            position_t leslie_freq_modulation_HF = static_cast<position_t>(amplitude_t(1) + amplitude_t(.0036) * amplitude_t(leslie_am__HF_sample));
            amplitude_t leslie_amp_modulation_HF = amplitude_t(.75) + amplitude_t(.25) * amplitude_t(leslie_fm_HF_sample);
            amplitude_t leslie_am_LF_sample = leslie_am_LF.next_sample();
            amplitude_t leslie_fm_LF_sample = leslie_fm_LF.next_sample();
            position_t leslie_freq_modulation_LF = static_cast<position_t>(amplitude_t(1) + amplitude_t(.0026) * amplitude_t(leslie_am_LF_sample));
            amplitude_t leslie_amp_modulation_LF = amplitude_t(.85) + amplitude_t(.15) * amplitude_t(leslie_fm_LF_sample);
            for (int i = 0; i < notes.active_count; i++) {
                current_idx = notes.active_idx[i];
                if (current_idx < LESLIE_CUTOFF_IDX) {
                    samples[j] += tonewheels[current_idx].next_sample(leslie_freq_modulation_LF) * notes.tonewheel_amplitudes[current_idx] * leslie_amp_modulation_LF;
                } else {
                    samples[j] += tonewheels[current_idx].next_sample(leslie_freq_modulation_HF) * notes.tonewheel_amplitudes[current_idx] * leslie_amp_modulation_HF;
                }
            }
        }
        return samples;
    }

    void shift_leslie_phase_HF(position_t phase_shift) {
        leslie_am_HF.set_position(leslie_am_HF.get_position() + phase_shift);
        leslie_fm_HF.set_position(leslie_fm_HF.get_position() + phase_shift);
    }

    void shift_leslie_phase_LF(position_t phase_shift) {
        leslie_am_LF.set_position(leslie_am_LF.get_position() + phase_shift);
        leslie_fm_LF.set_position(leslie_fm_LF.get_position() + phase_shift);
    }

    private:
        std::array<SineOscillator, num_tonewheel_notes> tonewheels;
        SineOscillator leslie_am_HF;
        SineOscillator leslie_fm_HF;
        SineOscillator leslie_am_LF;
        SineOscillator leslie_fm_LF;
};

template <size_t buffer_size>
struct StereoSamples {
    std::array<amplitude_t, buffer_size> left;
    std::array<amplitude_t, buffer_size> right;
};

class StereoOrganSynth {
public:
    StereoOrganSynth() {
        right.shift_leslie_phase_HF(static_cast<position_t>(.5)); // phase shift the right channel by 180 degrees
        right.shift_leslie_phase_LF(static_cast<position_t>(.125)); // phase shift the right channel by 45 degrees
    }

    template <size_t buffer_size>
    StereoSamples<buffer_size> next_samples(const std::vector<int> &notes_idxs) {
        PreparedNotes notes = OrganSynth::prepare_notes(notes_idxs);
        return {
            left.render_samples<buffer_size>(notes),
            right.render_samples<buffer_size>(notes)
        };
    }

    void set_leslie_target_step_sizes(position_t target_step_size_HF, position_t target_step_size_LF) {
        left.leslie_target_step_size_HF = target_step_size_HF;
        left.leslie_target_step_size_LF = target_step_size_LF;
        right.leslie_target_step_size_HF = target_step_size_HF;
        right.leslie_target_step_size_LF = target_step_size_LF;
    }

private:
    OrganSynth left;
    OrganSynth right;
};

inline amplitude_t soft_clip(amplitude_t x){
    if (x > amplitude_t(1)) {
        return amplitude_t(1);
    } else if (x < amplitude_t(-1)) {
        return amplitude_t(-1);
    } else {
        return x * (.5 * x * x - 1.5);
    }
}