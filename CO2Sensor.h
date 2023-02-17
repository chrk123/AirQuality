#pragma once

#include <SensirionI2CScd4x.h>

struct CO2Data : public Printable
{
  uint16_t co2         = 0;
  float    temperature = 0.f;
  float    humidity    = 0.f;

  // indicate if the measurement is valid
  bool valid = false;

  size_t printTo(Print& p) const override
  {
    return p.print("Co2: ") + p.print(co2) + p.print("ppm\t")
           + p.print("Temperature: ") + p.print(temperature) + p.print("°C\t")
           + p.print("Humidity: ") + p.print(humidity) + p.print("%");
  }
};

class CO2Sensor
{
public:
  enum class MeasureMode
  {
    SingleShot,
    LowPowerPeriodic,
    NormalPeriodic,
  };

  struct EnvironmentSettings
  {
    explicit EnvironmentSettings(uint16_t altitude, float temp_offset)
      : altitude{altitude}, temperature_offset{temp_offset}
    {
    }

    // altitude in m over sea level
    uint16_t altitude{0};

    // offset in deg C
    float temperature_offset{0.0f};
  };

  explicit CO2Sensor(TwoWire& i2c_bus, EnvironmentSettings settings,
                     MeasureMode measure_mode = MeasureMode::SingleShot)
    : m_Bus{i2c_bus}, m_Settings{settings}, m_MeasureMode{measure_mode}
  {
  }

  CO2Sensor(CO2Sensor const&)            = delete;
  CO2Sensor(CO2Sensor&&)                 = delete;
  CO2Sensor& operator=(CO2Sensor const&) = delete;
  CO2Sensor& operator=(CO2Sensor&&)      = delete;

  ~CO2Sensor()
  {
    StopMeasurement();
  }

  void onSleep()
  {
    StopMeasurement();
    m_Sensor.powerDown();
  }

  void onResume()
  {
    m_Sensor.wakeUp();
    StartMeasurement();
  }

  CO2Data GetMeasurement()
  {
    bool has_data = false;

    if (m_MeasureMode == MeasureMode::SingleShot)
    {
      m_Sensor.measureSingleShot();
      delay(500);
    }

    if (auto const error = m_Sensor.getDataReadyFlag(has_data); error)
    {
      return {};
    }

    if (!has_data)
      return {};

    CO2Data data;
    if (auto const error = m_Sensor.readMeasurement(data.co2, data.temperature,
                                                    data.humidity);
        !error && data.co2 != 0)
    {
      data.valid = true;
    }

    return data;
  }

  void StartMeasurement()
  {
    // sensor needs >1000ms to be ready
    delay(1000);

    m_Sensor.begin(m_Bus);

    // stop any previous ongoing measurement
    StopMeasurement();

    m_Sensor.setSensorAltitude(m_Settings.altitude);
    m_Sensor.setTemperatureOffset(m_Settings.temperature_offset);

    if (m_MeasureMode == MeasureMode::SingleShot)
      return;

    if (m_MeasureMode == MeasureMode::LowPowerPeriodic)
      m_Sensor.startLowPowerPeriodicMeasurement();
    else
      m_Sensor.startPeriodicMeasurement();
  }

  void StopMeasurement()
  {
    m_Sensor.stopPeriodicMeasurement();
  }

private:
  TwoWire&            m_Bus;
  SensirionI2CScd4x   m_Sensor;
  MeasureMode         m_MeasureMode;
  EnvironmentSettings m_Settings;
};