# Arduino multi sensor

Three independently scheduled Y, XY and XYZ waveform channels through the
C Init/Main API, 115200 baud/8N1. The sketch selects the multidimensional
configuration in `../../common/arduino_serial_config.h`; replace synthetic
sampling there for real sensors. PlatformIO enables X, Z and timestamps.

From the KymoCore root: `pio run -d examples/arduino/multi_sensor_example -e uno`.
Other environments: `nano`, `mega`. See [build instructions](../../README.md).
