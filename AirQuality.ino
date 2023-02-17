#include <Arduino.h>
#include <LowPower.h>
#include <Wire.h>

#include "CO2Sensor.h"
#include "Display.h"
#include "SPSSensor.h"
#include "VOCSensor.h"

constexpr uint64_t DISPLAY_UPDATE_INTERVAL = 3 * 60000;
constexpr uint64_t SENSOR_HEATUP_TIME      = 35000;

SPSSensor sps_sensor;
CO2Sensor co2_sensor{
        Wire, CO2Sensor::EnvironmentSettings{520u, 4.0f},
        CO2Sensor::MeasureMode::LowPowerPeriodic
};
VOCSensor voc_sensor{Wire};

Display display;

void SleepFor(unsigned long const ms)
{
  for (uint8_t i = 0; i < ceil(ms / 8000.); ++i)
    LowPower.powerStandby(SLEEP_8S, ADC_OFF, BOD_OFF);
}

void setup()
{
  Wire.begin();
  sps_sensor.StartMeasurement();
  voc_sensor.StartMeasurement();
  co2_sensor.StartMeasurement();
  display.setup();
  display.drawHeatUpScreen();
}

void loop()
{
  SleepFor(SENSOR_HEATUP_TIME);
  if (auto const co2_data = co2_sensor.GetMeasurement(); co2_data.valid)
  {
    display.SetCo2(co2_data);
  }

  if (auto const tvoc_data = voc_sensor.GetMeasurement(); tvoc_data.valid)
  {
    display.SetTVOC(tvoc_data);
  }

  if (auto const sps_data = sps_sensor.GetMeasurement(); sps_data.valid)
  {
    display.SetSPS(sps_data);
  }

  display.spinOnce();

  // co2_sensor.onSleep();
  sps_sensor.onSleep();

  SleepFor(DISPLAY_UPDATE_INTERVAL - SENSOR_HEATUP_TIME);

  sps_sensor.onResume();
  //  co2_sensor.onResume();
}
