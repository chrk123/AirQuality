#include <Adafruit_EPD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SGP30.h>
#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>
#include <LowPower.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <sps30.h>

#define EPD_CS    10
#define EPD_DC    9
#define SRAM_CS   8
#define EPD_RESET 7 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY  6

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

  void onSleep()
  {
    StopMeasurement();
    // TODO: implement sleep according to datasheet
  }

  void onResume()
  {
    // TOOD: implement wakeup according to datasheet
    StartMeasurement();
  }

  void StartMeasurement()
  {
    sps30_set_fan_auto_cleaning_interval_days(4);
    sps30_start_measurement();
  }

  void StopMeasurement()
  {
    sps30_stop_measurement();
  }

  Data GetMeasurement()
  {
    uint16_t has_data;

    if (sps30_read_data_ready(&has_data) < 0)
    {
      return Data{};
    }

    if (!has_data)
      return Data{};


    struct sps30_measurement m;
    if (sps30_read_measurement(&m) < 0)
    {
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
      return;
    }

    // values based on previous calibration
    if (!m_Sensor.setIAQBaseline(37120, 39100))
    {
    }
  }

  Data GetMeasurement()
  {
    if (!m_Sensor.IAQmeasure())
    {
      return {};
    }

    Data data;

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

  enum class MeasureMode
  {
    SingleShot,
    LowPowerPeriodic,
    NormalPeriodic,
  };

  explicit CO2Sensor(TwoWire&    i2c_bus,
                     MeasureMode measure_mode = MeasureMode::SingleShot)
    : m_Bus{i2c_bus}, m_MeasureMode{measure_mode}
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

  Data GetMeasurement()
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

    Data data;
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
    m_Sensor.setSensorAltitude(520 /* Munich, in meter*/);

    if (m_MeasureMode == MeasureMode::SingleShot)
      return;

    // 0x21ac is low power peridic measurement mode (30s measure interval)
    Wire.beginTransmission(0x62);
    Wire.write(0x21);
    Wire.write(m_MeasureMode == MeasureMode::LowPowerPeriodic ? 0xac : 0xb1);
    Wire.endTransmission();
  }

  void StopMeasurement()
  {
    if (m_MeasureMode != MeasureMode::SingleShot)
      m_Sensor.stopPeriodicMeasurement();
  }

private:
  TwoWire&          m_Bus;
  SensirionI2CScd4x m_Sensor;
  MeasureMode       m_MeasureMode;
};

constexpr uint8_t const GRID_DX     = 88;
constexpr uint8_t const GRID_DY     = 58;
constexpr uint8_t const LINE_HEIGHT = GRID_DY / 2;

class Display
{
public:
  void setup()
  {
    m_Display.begin();
    m_Display.setFont(&FreeSans9pt7b);
  }

  void drawDataCell(char* title, uint16_t value)
  {
    auto const original_cursor_x = m_Display.getCursorX();
    auto const original_cursor_y = m_Display.getCursorY();

    m_Display.setFont(&FreeSansBoldOblique9pt7b);
    m_Display.setCursor(original_cursor_x + 4, original_cursor_y + 4);
    m_Display.print(title);
    m_Display.setFont(&FreeSans9pt7b);
    m_Display.setCursor(original_cursor_x + 10,
                        original_cursor_y + LINE_HEIGHT);
    itoa(m_CO2Data.co2, m_Buffer, 10);
    m_Display.drawRect(original_cursor_x, original_cursor_y - 15, GRID_DX,
                       GRID_DY, EPD_BLACK);
    m_Display.print(m_Buffer);
  }

  void drawDataCell(char* title, float value)
  {
    auto const original_cursor_x = m_Display.getCursorX();
    auto const original_cursor_y = m_Display.getCursorY();

    m_Display.setFont(&FreeSansBoldOblique9pt7b);
    m_Display.setCursor(original_cursor_x + 4, original_cursor_y + 4);
    m_Display.print(title);
    m_Display.setFont(&FreeSans9pt7b);
    m_Display.setCursor(original_cursor_x + 10,
                        original_cursor_y + LINE_HEIGHT);
    dtostrf(value, 4, 2, m_Buffer);
    m_Display.drawRect(original_cursor_x, original_cursor_y - 15, GRID_DX,
                       GRID_DY, EPD_BLACK);
    m_Display.print(m_Buffer);
  }

  void drawStatusCell(bool isAirGood)
  {
    auto const original_cursor_x = m_Display.getCursorX();
    auto const original_cursor_y = m_Display.getCursorY();

    m_Display.drawRect(original_cursor_x, original_cursor_y - 15, GRID_DX,
                       GRID_DY, EPD_BLACK);

    m_Display.setCursor(original_cursor_x + GRID_DX / 4,
                        original_cursor_y + GRID_DY / 3);
    m_Display.print(isAirGood ? "good" : "bad");
  }

  void spinOnce()
  {
    m_Display.clearBuffer();
    m_Display.fillScreen(EPD_WHITE);

    for (uint8_t x = 0; x < 3; ++x)
    {
      for (uint8_t y = 0; y < 3; ++y)
      {
        // add some screen offset so that the first line appears...
        m_Display.setCursor(x * GRID_DX, y * GRID_DY + 15);

        if (x == 0 && y == 0)
        {
          drawDataCell("CO2", m_CO2Data.co2);
        }
        else if (x == 1 && y == 0)
        {
          drawDataCell("Temp", m_CO2Data.temperature);
        }
        else if (x == 2 && y == 0)
        {
          drawDataCell("Humid", m_CO2Data.humidity);
        }
        else if (x == 0 && y == 1)
        {
          drawDataCell("PM1.0", m_SPSData.pm1);
        }
        else if (x == 1 && y == 1)
        {
          drawDataCell("PM2.5", m_SPSData.pm25);
        }
        else if (x == 2 && y == 1)
        {
          drawDataCell("TVOC", m_TVOCData.tvoc);
        }
        else if (x == 0 && y == 2)
        {
          drawDataCell("PM4.0", m_SPSData.pm4);
        }
        else if (x == 1 && y == 2)
        {
          drawDataCell("PM10", m_SPSData.pm10);
        }
        else if (x == 2 && y == 2)
        {
          drawStatusCell(true);
        }
      }
    }
    m_Display.display(true);
  }

  void SetCo2(CO2Sensor::Data const& data)
  {
    m_CO2Data = data;
  }

  void SetSPS(SPSSensor::Data const& data)
  {
    m_SPSData = data;
  }

  void SetTVOC(VOCSensor::Data const& data)
  {
    m_TVOCData = data;
  }

private:
  char            m_Buffer[16];
  CO2Sensor::Data m_CO2Data;
  SPSSensor::Data m_SPSData;
  VOCSensor::Data m_TVOCData;

  Adafruit_IL91874 m_Display{264, 176, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS};
};


SPSSensor sps_sensor;
CO2Sensor co2_sensor{Wire};
VOCSensor voc_sensor{Wire};

Display display;

void setup()
{
  Wire.begin();
  sps_sensor.StartMeasurement();
  voc_sensor.StartMeasurement();
  co2_sensor.StartMeasurement();
  display.setup();
}

constexpr uint64_t const UPDATE_INTERVAL = 5 * 60000;

void loop()
{
  // wait until sensors get ready
  delay(5000);

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

  co2_sensor.onSleep();
  sps_sensor.onSleep();

  for (uint8_t i = 0; i < UPDATE_INTERVAL / 8000; ++i)
    LowPower.powerStandby(SLEEP_8S, ADC_OFF, BOD_OFF);


  sps_sensor.onResume();
  co2_sensor.onResume();
}
