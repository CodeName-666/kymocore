# Arduino

Use `simple_analog_example` for two waveform channels, or
`multi_sensor_example` for Y/XY/XYZ. Both use `Kymo_Init` and `Kymo_Main`.
Change channel periods, flags and the sample callback in
`../common/arduino_serial_config.h`. Replace waveform generation with
`analogRead(A0)` for actual ADC measurements. Select Uno, Nano or Mega in the
project's platformio.ini. Serial connection: 115200, 8N1, KymoStudio binary v6.
See [example build instructions](../README.md).

These small examples use only the C Init/Main API. The larger C++ sender,
network, performance and trace demonstrations live in KymoProbe.
