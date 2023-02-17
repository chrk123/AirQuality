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

constexpr uint8_t EPD_CS    = 10;
constexpr uint8_t EPD_DC    = 9;
constexpr uint8_t SRAM_CS   = 8;
constexpr uint8_t EPD_RESET = 7;
constexpr uint8_t EPD_BUSY  = 6;

constexpr uint16_t DISPLAY_HEIGHT = 176;
constexpr uint16_t DISPLAY_WIDTH  = 264;
constexpr uint16_t GRID_DX        = DISPLAY_WIDTH / 3;
constexpr uint16_t GRID_DY        = DISPLAY_HEIGHT / 3;
constexpr uint16_t LINE_HEIGHT    = GRID_DY / 2;
constexpr uint16_t Y_OFFSET       = 15;

constexpr uint64_t UPDATE_INTERVAL    = 3 * 60000;
constexpr uint64_t SENSOR_HEATUP_TIME = 10000;
constexpr uint64_t BUFFERSIZE         = GRID_DX * 1;

enum class AirQualityLevel
{
  Good,
  Bad,
};

struct DataBuffer
{
public:
  void append(int data)
  {
    if (m_Size < BUFFERSIZE)
    {
      m_Buffer[m_Size++] = data;
    }
    else
    {
      for (int i = 1; i < BUFFERSIZE; i++)
      {
        m_Buffer[i - 1] = m_Buffer[i];
      }
      m_Buffer[BUFFERSIZE - 1] = data;
    }
  }

  size_t getSize() const
  {
    return m_Size;
  }

  int const* getData() const
  {
    return m_Buffer;
  }

  void getBoundaries(int& vmin, int& vmax) const
  {
    vmin = -2147483648;
    vmax = 2147483647;

    for (int i = 0; i < m_Size; i++)
    {
      if (m_Buffer[i] < vmin)
        vmin = m_Buffer[i];
      else if (m_Buffer[i] > vmax)
        vmax = m_Buffer[i];
    }
  }

private:
  int    m_Buffer[BUFFERSIZE];
  size_t m_Size{0};
};

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
    m_Sensor.setIAQBaseline(37120, 39100);
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

AirQualityLevel JudgeQuality(VOCSensor::Data const& voc,
                             CO2Sensor::Data const& co2,
                             SPSSensor::Data const& sps)
{
  if (co2.co2 > 1000 || co2.humidity > 60)
    return AirQualityLevel::Bad;

  return AirQualityLevel::Good;
}

class Display
{
public:
  void setup()
  {
    m_Display.begin();
    m_Display.setFont(&FreeSans9pt7b);
  }

  void drawDataCell(char const* title, char const* unit, uint16_t const value)
  {
    auto const original_cursor_x = m_Display.getCursorX();
    auto const original_cursor_y = m_Display.getCursorY();

    m_Display.setFont(&FreeSansBoldOblique9pt7b);
    m_Display.setCursor(original_cursor_x + 4, original_cursor_y + 4);
    m_Display.print(title);
    m_Display.setFont(&FreeSans9pt7b);
    m_Display.setCursor(original_cursor_x + 10,
                        original_cursor_y + LINE_HEIGHT);
    itoa(value, m_Buffer, 10);
    m_Display.drawRect(original_cursor_x, original_cursor_y - Y_OFFSET,
                       GRID_DX, GRID_DY, EPD_BLACK);
    m_Display.print(m_Buffer);

    m_Display.setFont(nullptr);
    m_Display.setCursor(original_cursor_x + 50,
                        original_cursor_y + LINE_HEIGHT);
    m_Display.print(unit);
  }

  void drawDataCell(char const* title, char const* unit, float const value)
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

    m_Display.setFont(nullptr);
    m_Display.setCursor(original_cursor_x + 5,
                        original_cursor_y + LINE_HEIGHT);
    m_Display.print(unit);
  }

  void drawStatusCell(AirQualityLevel level)
  {
    auto const original_cursor_x = m_Display.getCursorX();
    auto const original_cursor_y = m_Display.getCursorY();

    m_Display.setFont(&FreeSans9pt7b);
    m_Display.drawRect(original_cursor_x, original_cursor_y - 15, GRID_DX,
                       GRID_DY, EPD_BLACK);

    m_Display.setCursor(original_cursor_x + GRID_DX / 4,
                        original_cursor_y + GRID_DY / 3);

    if (level == AirQualityLevel::Good)
    {
      m_Display.setTextColor(EPD_BLACK);
      m_Display.print("good");
    }
    else
    {
      m_Display.setTextColor(EPD_RED);
      m_Display.print("bad");
    }
  }

  void plot2d(int const* values, size_t count, int y_min, int y_max,
              size_t x_span, size_t y_span)
  {
    if (count == 1)
      return;

    auto const origin_x = m_Display.getCursorX();
    auto const origin_y = m_Display.getCursorY();

    auto const margin_top    = 6.;
    auto const margin_right  = 20.;
    auto const margin_bottom = 6;

    m_Display.setFont(nullptr);
    auto const font_offset    = -3;
    auto const legend_padding = 3;

    auto const plot_width  = (x_span * GRID_DX) - margin_right;
    auto const plot_height = (y_span * GRID_DY) - margin_top - margin_bottom;

    auto const dy = plot_height / (y_max - y_min);
    auto const dx = plot_width / (count - 1);

    for (int i = 0; i < count - 1; i++)
    {
      auto const x_coord_start = i * dx;
      auto const y_coord_start = plot_height - ((values[i] - y_min) * dy);
      auto const x_coord_end   = (i + 1) * dx;
      auto const y_coord_end   = plot_height - ((values[i + 1] - y_min) * dy);

      m_Display.drawLine(origin_x + x_coord_start,
                         origin_y + y_coord_start + margin_top,
                         origin_x + x_coord_end,
                         origin_y + y_coord_end + margin_top, EPD_BLACK);
    }
    m_Display.drawFastHLine(origin_x, origin_y + margin_top, plot_width,
                            EPD_BLACK);
    m_Display.setCursor(origin_x + plot_width + legend_padding,
                        origin_y + margin_top + font_offset);
    m_Display.print(y_max);

    m_Display.setCursor(origin_x + plot_width + legend_padding,
                        origin_y + margin_top + plot_height / 2 + font_offset);

    m_Display.print((y_max - y_min) / 2);

    m_Display.drawFastHLine(origin_x, origin_y + margin_top + plot_height,
                            plot_width, EPD_BLACK);
    m_Display.setCursor(origin_x + plot_width + legend_padding,
                        origin_y + margin_top + plot_height + font_offset);

    m_Display.print(y_min);
  }


  void drawPlotCells(char const* title, char const* unit,
                     char const* current_value, DataBuffer const& history,
                     int v_min, int v_max)
  {
    auto const original_cursor_x = m_Display.getCursorX();
    auto const original_cursor_y = m_Display.getCursorY();

    m_Display.setFont(&FreeSansBoldOblique9pt7b);
    m_Display.setCursor(original_cursor_x + 4, original_cursor_y + 4);
    m_Display.print(title);
    m_Display.setFont(&FreeSans9pt7b);
    m_Display.setCursor(original_cursor_x + 10,
                        original_cursor_y + LINE_HEIGHT);
    m_Display.print(current_value);

    m_Display.setFont(nullptr);
    m_Display.setCursor(original_cursor_x + 55,
                        original_cursor_y + LINE_HEIGHT);
    m_Display.print(unit);

    m_Display.setCursor(original_cursor_x + 1 * GRID_DX - 15,
                        original_cursor_y - 15);
    int t_min, t_max;
    history.getBoundaries(t_min, t_max);

    t_min = min(v_min, t_min);
    t_max = max(v_max, t_max);
    plot2d(history.getData(), history.getSize(), t_min, t_max, 1, 1);

    m_Display.drawRect(original_cursor_x, original_cursor_y - Y_OFFSET,
                       2 * GRID_DX, GRID_DY, EPD_BLACK);
  }

  void drawHeatUpScreen()
  {
    m_Display.clearBuffer();
    m_Display.fillScreen(EPD_WHITE);

    m_Display.setCursor(m_Display.width() / 5, m_Display.height() / 2);
    m_Display.setFont(&FreeSansBoldOblique9pt7b);
    m_Display.print("Preparing sensors...");
    m_Display.display(true);
  }

  void spinOnce()
  {
    m_Display.clearBuffer();
    m_Display.fillScreen(EPD_WHITE);


    m_Display.setCursor(0 * GRID_DX, 0 * GRID_DY + Y_OFFSET);
    itoa(m_CO2Data.co2, m_Buffer, 10);
    drawPlotCells("CO2", "ppm", m_Buffer, m_CO2History, 400, 1700);

    m_Display.setCursor(2 * GRID_DX, 0 * GRID_DY + Y_OFFSET);
    drawDataCell("PM10", "mug/m3", m_SPSData.pm10);

    m_Display.setCursor(0 * GRID_DX, 1 * GRID_DY + Y_OFFSET);
    dtostrf(m_CO2Data.temperature, 4, 2, m_Buffer);
    drawPlotCells("Temp", "C", m_Buffer, m_TemperatureHistory, 10, 40);

    m_Display.setCursor(2 * GRID_DX, 1 * GRID_DY + Y_OFFSET);
    drawDataCell("TVOC", "ppb", m_TVOCData.tvoc);

    m_Display.setCursor(0 * GRID_DX, 2 * GRID_DY + Y_OFFSET);
    dtostrf(m_CO2Data.humidity, 4, 2, m_Buffer);
    drawPlotCells("Humid", "%", m_Buffer, m_HumidityHistory, 0, 100);

    m_Display.setCursor(2 * GRID_DX, 2 * GRID_DY + Y_OFFSET);
    drawStatusCell(JudgeQuality(m_TVOCData, m_CO2Data, m_SPSData));

    m_Display.display(true);
  }

  void SetCo2(CO2Sensor::Data const& data)
  {
    m_CO2Data = data;
    m_TemperatureHistory.append(data.temperature);
    m_HumidityHistory.append(data.humidity);
    m_CO2History.append(data.co2);
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

  DataBuffer m_TemperatureHistory;
  DataBuffer m_CO2History;
  DataBuffer m_HumidityHistory;

  Adafruit_IL91874 m_Display{DISPLAY_WIDTH, DISPLAY_HEIGHT, EPD_DC,
                             EPD_RESET,     EPD_CS,         SRAM_CS};
};


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

  SleepFor(UPDATE_INTERVAL - SENSOR_HEATUP_TIME);

  sps_sensor.onResume();
  //  co2_sensor.onResume();
}
