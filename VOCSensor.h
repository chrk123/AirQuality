#pragma once

#include <Adafruit_SGP30.h>

struct VOCData : public Printable
{
  uint16_t tvoc          = 0;
  uint16_t tvoc_baseline = 0;

  uint16_t eco2          = 0;
  uint16_t eco2_baseline = 0;

  bool valid = false;

  size_t printTo(Print& p) const override
  {
    return p.print("TVOC: ") + p.print(tvoc) + p.print("ppb\t")
           + p.print("TVOC baseline: ") + p.print(tvoc_baseline)
           + p.println("\t") + p.print("eCO2: ") + p.print(eco2)
           + p.print("ppm\t") + p.print("eCO2 baseline: ")
           + p.print(eco2_baseline) + p.print("\t");
  }
};

class VOCSensor
{
public:
  explicit VOCSensor(TwoWire& i2c_bus) : m_Bus{i2c_bus}
  {
  }

  VOCSensor(VOCSensor const&)            = delete;
  VOCSensor(VOCSensor&&)                 = delete;
  VOCSensor& operator=(VOCSensor const&) = delete;
  VOCSensor& operator=(VOCSensor&&)      = delete;
  ~VOCSensor()                           = default;

  void StartMeasurement()
  {
    if (!m_Sensor.begin(&m_Bus))
    {
      return;
    }

    // values based on previous calibration
    m_Sensor.setIAQBaseline(37120, 39100);
  }

  VOCData GetMeasurement()
  {
    if (!m_Sensor.IAQmeasure())
    {
      return {};
    }

    VOCData data;

    if (!m_Sensor.getIAQBaseline(&data.eco2_baseline, &data.tvoc_baseline))
    {
      return data;
    }

    data.tvoc  = m_Sensor.TVOC;
    data.eco2  = m_Sensor.eCO2;
    data.valid = true;

    return data;
  }

private:
  TwoWire&       m_Bus;
  Adafruit_SGP30 m_Sensor;
};