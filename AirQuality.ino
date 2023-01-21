#include <Adafruit_SGP30.h>
#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <sps30.h>

class SPSSensor
{
public:
  struct Data : public Printable
  {
    explicit Data() = default;
    explicit Data(sps30_measurement& m)
      : pm1{m.mc_1p0}
      , pm25{m.mc_2p5}
      , pm4{m.mc_4p0}
      , pm10{m.mc_10p0}
      , valid{true}
    {
    }

    float pm1  = 0.;
    float pm25 = 0.;
    float pm4  = 0.;
    float pm10 = 0.;

    // indicate if the measurement is valid
    bool valid = false;

    size_t printTo(Print& p) const override
    {
      return p.print("PM1: ") + p.print(pm1) + p.print("μg/m3\t")
             + p.print("PM2.5: ") + p.print(pm25) + p.print("μg/m3\t")
             + p.print("PM4.0: ") + p.print(pm4) + p.print("μg/m3\t")
             + p.print("PM10.0: ") + p.print(pm10) + p.print("μg/m3\t");
    }
  };

  SPSSensor()                            = default;
  SPSSensor(SPSSensor const&)            = delete;
  SPSSensor(SPSSensor&&)                 = delete;
  SPSSensor& operator=(SPSSensor const&) = delete;
  SPSSensor& operator=(SPSSensor&&)      = delete;
  ~SPSSensor()
  {
    StopMeasurement();
  }

  void StartMeasurement()
  {
    while (sps30_probe() != 0)
    {
      Serial.println("SPS sensor probing failed");
      delay(500);
    }

    if (auto const ret = sps30_set_fan_auto_cleaning_interval_days(4); ret)
    {
      Serial.print("Could not set auto clean interval for SPS sensor");
    }

    if (sps30_start_measurement() < 0)
    {
      Serial.println("Could not start SPS measurement");
    }
  }

  void StopMeasurement()
  {
    if (sps30_stop_measurement() < 0)
    {
      Serial.println("Could not stop SPS measurement");
    }
  }

  Data GetMeasurement()
  {
    uint16_t has_data;

    if (sps30_read_data_ready(&has_data) < 0)
    {
      Serial.println("Could not read data ready for SPS");
      return Data{};
    }

    if (!has_data)
      return Data{};


    struct sps30_measurement m;
    if (sps30_read_measurement(&m) < 0)
    {
      Serial.println("Could not read data measurement for SPS");
      return Data{};
    }

    return Data{m};
  }
};

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

  VOCSensor(VOCSensor const&)            = delete;
  VOCSensor(VOCSensor&&)                 = delete;
  VOCSensor& operator=(VOCSensor const&) = delete;
  VOCSensor& operator=(VOCSensor&&)      = delete;
  ~VOCSensor()                           = default;

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

  CO2Sensor(CO2Sensor const&)            = delete;
  CO2Sensor(CO2Sensor&&)                 = delete;
  CO2Sensor& operator=(CO2Sensor const&) = delete;
  CO2Sensor& operator=(CO2Sensor&&)      = delete;

  ~CO2Sensor()
  {
    StopMeasurement();
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

  void StartMeasurement(bool low_power = true)
  {
    // sensor needs >1000ms to be ready
    delay(1000);

    m_Sensor.begin(m_Bus);

    // 0x21ac is low power peridic measurement mode (30s measure interval)
    Wire.beginTransmission(0x62);
    Wire.write(0x21);
    Wire.write(low_power ? 0xac : 0xb1);
    Wire.endTransmission();
  }

  void StopMeasurement()
  {
    if (auto const error = m_Sensor.stopPeriodicMeasurement(); error)
    {
      Serial.print("Could not stop the measurement: ");
      PrintErrorMessage(error);
    }
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

CO2Sensor co2_sensor{Wire};
VOCSensor voc_sensor{Wire};
SPSSensor sps_sensor;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(100);
  }

  Wire.begin();
  voc_sensor.StartMeasurement();
  co2_sensor.StartMeasurement();
  sps_sensor.StartMeasurement();
}

void loop()
{
  delay(10000);

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

  auto const sps_data = sps_sensor.GetMeasurement();
  if (sps_data.valid)
  {
    Serial.println(sps_data);
  }
}
