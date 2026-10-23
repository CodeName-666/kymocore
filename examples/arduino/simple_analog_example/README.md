# Arduino simple analog

Two synthetic waveform channels through the C Init/Main API, 115200 baud/8N1.
Everything lives in `simple_analog_example.ino`; replace the waveform in
`readSample()` with `analogRead(A0)` for real ADC input.

From the KymoCore root: `pio run -d examples/arduino/simple_analog_example -e uno`.
Other environments: `nano`, `mega`. The `.ino` is also the PlatformIO entrypoint.
See [build and feature instructions](../../README.md).
