# hello_sensors

On-hardware smoke test for the four I2C1 sensor drivers. The LCD shows device
presence and live readings; RTT carries the complete scaled diagnostics.

    fw build hello_sensors
    fw flash hello_sensors
    fw rtt

Expected behavior:

- SHT40 reports plausible room temperature and humidity.
- OPT4001 responds to covering and illumination.
- BMI323 shows roughly 1000 milli-g on the down axis and reacts to motion.
- BMM350 reports a changing Earth-field magnitude near metal or magnets.
