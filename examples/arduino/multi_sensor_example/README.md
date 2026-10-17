# Arduino multi sensor

Three independently scheduled Y, XY and XYZ waveform channels through the
C Init/Main API, 115200 baud/8N1. Everything lives in
`multi_sensor_example.ino`; replace synthetic sampling in `readSample()` for
real sensors. PlatformIO enables X, Z and timestamps.

From the KymoCore root: `pio run -d examples/arduino/multi_sensor_example -e uno`.
Other environments: `nano`, `mega`. See [build instructions](../../README.md).
