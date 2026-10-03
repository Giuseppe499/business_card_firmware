# Organ synth business card: firmware

This repository contains the C++ firmware for my organ synth business card.
The firmware is designed to run on the rp2354 microcontroller with a PCM5102A DAC, and a custom resistive touch keyboard.

The complete repository (including the KiCad hardware files) can be found [here](https://github.com/Giuseppe499/business_card).

# What it does

Each key sums a harmonic series read from a single sine wavetable generated at
compile time with `constexpr`.
A two-rotor Leslie simulation applies doppler-based amplitude and frequency modulation. A `tanh` soft clipper is applied to the stereo output.

The [main repository](https://github.com/Giuseppe499/business_card) describes
the synthesis in more detail.

The target board is `pico2` (RP2350). RP2040 is no longer supported: the synth
moved from fixed-point arithmetic to the RP2350 floating-point unit.

# Fedora dependencies

To install the required dependencies on Fedora, run the following command:

```bash
sudo dnf install gcc-arm-linux-gnu \
 arm-none-eabi-gcc-cs-c++ \
 arm-none-eabi-gcc-cs \
 arm-none-eabi-binutils \
 arm-none-eabi-newlib
```

# pico-sdk submodules initialization

To compile the project, you might need to initialize the submodules of the pico-sdk. To do so, move to the `pico-sdk` directory

```bash
cd pico-sdk
```

and run the following command:

```bash
git submodule update --init
```

# Build

Run `cmake` to generate the build files in the `build` directory:

```bash
cmake -B./build -S.
```

Then, run `make` to build the project:

```bash
make -C ./build
```

# Flash
To flash the firmware you can simply copy the `main.uf2` file from the `build` directory to the `pico` drive.
To do so, press the `BOOTSEL` button on the Pico while plugging it into your computer, then copy the `main.uf2` file to the `pico` drive that appears.

As an alternative, you can use the `picotool` command line utility to flash the firmware. First, make sure you have `picotool` installed on your system. Then, run the following command:

```bash
picotool load -f ./build/main.uf2
```

The pi pico should automatically reset, start in bootloader mode, and flash the firmware.
If picotool fails to detect the device, or to reset it, the firmware might have crashed.
In this case, you can manually reset the device (e.g., by unplugging and plugging it back in) while holding the `BOOTSEL` button.
At this point, you can run the `picotool` command to flash the firmware.

# Audio backend

The audio layer supports I2S, PWM and S/PDIF. I2S is the default, selected in
`CMakeLists.txt` with `USE_AUDIO_I2S=1`. The other two back ends compile but are
untested on this board.

# License

[GPL-3.0](LICENSE).
