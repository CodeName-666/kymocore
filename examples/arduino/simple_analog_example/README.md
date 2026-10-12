# Arduino simple analog

Two synthetic waveform channels through the C Init/Main API, 115200 baud/8N1.
Replace sampling in `../../common/arduino_serial_config.h` with `analogRead(A0)`
for real ADC input. Keep the shared header with the sketch in the Arduino IDE.

From the KymoCore root: `pio run -d examples/arduino/simple_analog_example -e uno`.
Other environments: `nano`, `mega`. The `.ino` is also the PlatformIO entrypoint.
See [build and feature instructions](../../README.md).
