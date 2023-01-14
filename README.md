# AirQuality

An arduino project that aims to construct an air quality monitoring system based on three different hardware sensors.

## Sensirion SCD41

This one provides readings for CO2, temperature and humidity.

## Sensirion SPS30

This one provides readings for PM1.0, PM2.5, PM4 and PM10.

## Sensirion SGP30

This one is used to measure TVOC.

## Dependencies

The following libraries are required:
- Adafruit BusIO
- Adafruit SGP30 Sensor
- arduino-sps
- Sensirion Core
- Sensirion I2C SCD4x

## Future ideas

In the future, this project will either connect a display (LCD or OLED) for visualization purposes, or provide the data via bluetooth, in which case an app will be used for visualization.
