#include <Adafruit_SGP30.h>
#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>

class VOCSensor
{
public:
  struct Data : public Printable
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

  explicit VOCSensor(TwoWire& i2c_bus) : m_Bus{i2c_bus}
  {
  }

  void StartMeasurement()
  {
    if (!m_Sensor.begin(&m_Bus))
    {
      Serial.println("Could not start VOC measurement!");
      return;
    }

    // values based on previous calibration
    if (!m_Sensor.setIAQBaseline(37120, 39100))
    {
      Serial.println("Setting baseline failed");
    }
  }

  Data GetMeasurement()
  {
    if (!m_Sensor.IAQmeasure())
    {
      Serial.println("Could not measure VOC!");
      return {};
    }

    Data data;
    if (!m_Sensor.getIAQBaseline(&data.eco2_baseline, &data.tvoc_baseline))
    {
      Serial.println("Could not determine baselines!");
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

class CO2Sensor
{
public:
  struct Data : public Printable
  {
    uint16_t co2         = 0;
    float    temperature = 0.f;
    float    humidity    = 0.f;

    // indicate if the measurement is valid
    bool valid = false;

    size_t printTo(Print& p) const override
    {
      return p.print("Co2: ") + p.print(co2) + p.print("ppm\t")
             + p.print("Temperature: ") + p.print(temperature)
             + p.print("°C\t") + p.print("Humidity: ") + p.print(humidity)
             + p.print("%");
    }
  };

  explicit CO2Sensor(TwoWire& i2c_bus) : m_Bus{i2c_bus}
  {
  }

  Data GetMeasurement()
  {
    bool has_data = false;

    if (auto const error = m_Sensor.getDataReadyFlag(has_data); error)
    {
      Serial.print("Could not obtain data ready flag: ");
      PrintErrorMessage(error);
      return {};
    }

    if (!has_data)
      return {};

    Data data;
    if (auto const error = m_Sensor.readMeasurement(data.co2, data.temperature,
                                                    data.humidity);
        error)
    {
      Serial.print("Could not read the measurement: ");
      PrintErrorMessage(error);
    }
    else if (data.co2 != 0 /*according to example, thats an invalid sample*/)
    {
      data.valid = true;
    }

    return data;
  }

  void StartMeasurement()
  {
    m_Sensor.begin(m_Bus);
    if (auto const error = m_Sensor.startPeriodicMeasurement(); error)
    {
      Serial.print("Could not start the measurement: ");
      PrintErrorMessage(error);
    }
  }

  void StopMeasurement()
  {
    if (auto const error = m_Sensor.stopPeriodicMeasurement(); error)
    {
      Serial.print("Could not stop the measurement: ");
      PrintErrorMessage(error);
    }
  }

  ~CO2Sensor()
  {
    StopMeasurement();
  }

private:
  void PrintErrorMessage(uint16_t error)
  {
    char error_message[256];

    errorToString(error, error_message, 256);
    Serial.println(error_message);
  }

  TwoWire&          m_Bus;
  SensirionI2CScd4x m_Sensor;
};

auto co2_sensor = CO2Sensor{Wire};
auto voc_sensor = VOCSensor{Wire};

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  Wire.begin();
  co2_sensor.StartMeasurement();
  voc_sensor.StartMeasurement();
}

void loop()
{
  delay(1000);

  auto const co2_data = co2_sensor.GetMeasurement();
  if (co2_data.valid)
  {
    Serial.println(co2_data);
  }

  auto const tvoc_data = voc_sensor.GetMeasurement();
  if (tvoc_data.valid)
  {
    Serial.println(tvoc_data);
  }
}
