#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>

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
      return p.print("Co2:") + p.print(co2) + p.print("\t")
             + p.print("Temperature:") + p.print(temperature) + p.print("\t")
             + p.print("Humidity:") + p.print(humidity);
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

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  Wire.begin();
  co2_sensor.StartMeasurement();
}

void loop()
{
  delay(1000);

  // Read Measurement
  auto const data = co2_sensor.GetMeasurement();
  if (data.valid)
  {
    Serial.println(data);
  }
}
