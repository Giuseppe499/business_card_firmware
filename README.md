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