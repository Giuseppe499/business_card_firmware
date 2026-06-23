#pragma once

#include <stdint.h>
#include "audio.h"
#include <tuple>

class Synth {
    public:
        Synth() : frequency(0) {}
        Synth(float freq) : frequency(freq) {}
        virtual int16_t next_sample() {
            return 0;
        };
        virtual void set_frequency(float freq) {
            frequency = freq;
        }
        virtual void reset() {};
    protected:
        float frequency;
};

class SineOscillator : public Synth {
public:
    SineOscillator() : Synth(0), step_size(0), position(0) {}
    SineOscillator(float freq) : Synth(freq), step_size(step_size_for_freq(freq)), position(0) {}

    void set_frequency(float freq) override {
        frequency = freq;
        step_size = step_size_for_freq(freq);
    }

    int16_t next_sample() override {
        int16_t sample = sine_wave_table[position >> 16];
        position = (position + step_size) % MAX_POSITION;
        return sample;
    }

    void reset() override {
        position = 0;
    }

private:
    float frequency;
    uint32_t step_size;
    uint32_t position;
};


class AdditiveSynth : public Synth {
private:
    size_t num_harmonics;
    float* harmonics;
    float* amplitudes;
    SineOscillator* oscillators;
protected:
    void initialize_harmonics(size_t num_harmonics, float* harmonics, float* amplitudes) {
        this->num_harmonics = num_harmonics;
        this->harmonics = new float[num_harmonics];
        this->amplitudes = new float[num_harmonics];
        this->oscillators = new SineOscillator[num_harmonics];
        for (size_t i = 0; i < num_harmonics; i++) {
            this->harmonics[i] = harmonics[i];
            this->amplitudes[i] = amplitudes[i];
            this->oscillators[i] = SineOscillator(this->frequency * harmonics[i]);
        }
    }
public:
    AdditiveSynth() : Synth(0), num_harmonics(0), harmonics(nullptr), amplitudes(nullptr), oscillators(nullptr) {};
    AdditiveSynth(float freq) : Synth(freq), num_harmonics(0), harmonics(nullptr), amplitudes(nullptr), oscillators(nullptr) {};
    AdditiveSynth(float freq, size_t num_harmonics, float* harmonics, float* amplitudes)
        : Synth(freq) {
        initialize_harmonics(num_harmonics, harmonics, amplitudes);
    }

    ~AdditiveSynth() {
        delete[] harmonics;
        delete[] amplitudes;
        delete[] oscillators;
    }

    int16_t next_sample() override {
        int16_t sample = 0;
        for (size_t i = 0; i < num_harmonics; i++) {
            sample += amplitudes[i] * oscillators[i].next_sample();
        }
        return sample;
    }

    void reset() override {
        for (size_t i = 0; i < num_harmonics; i++) {
            oscillators[i].reset();
        }
    }
};

class OrganSynth : public AdditiveSynth {
public:
    OrganSynth() : AdditiveSynth(0) {};
    OrganSynth(float freq) : AdditiveSynth(freq) {
        float harmonics[] = {1, 1.5, 2, 3, 4, 5, 6, 8};
        float amplitudes[] = {1, 0.5, 0.3, 0.2, 0.1, 0.1, 0.1, 0.1};
        float sum_amplitudes = 0;
        for (float amp : amplitudes) {
            sum_amplitudes += amp;
        }
        for (float& amp : amplitudes) {
            amp /= sum_amplitudes;
        }
        size_t num_harmonics = sizeof(harmonics) / sizeof(harmonics[0]);
        // for (size_t i = 0; i < num_harmonics; i++) {
        //     harmonics[i] *= freq;
        // }
        this->initialize_harmonics(num_harmonics, harmonics, amplitudes);
    };
};