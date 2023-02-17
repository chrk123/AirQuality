#pragma once
#include <Adafruit_EPD.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBoldOblique9pt7b.h>

#include "CO2Sensor.h"
#include "DataUtils.h"
#include "SPSSensor.h"
#include "VOCSensor.h"

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

constexpr uint64_t BUFFERSIZE = GRID_DX;

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
    if (m_Size == 0)
    {
      vmin = INT16_MIN;
      vmax = INT16_MAX;
      return;
    }

    vmin = m_Buffer[0];
    vmax = m_Buffer[0];

    for (int i = 1; i < m_Size; i++)
    {
      if (m_Buffer[i] < vmin)
        vmin = m_Buffer[i];

      if (m_Buffer[i] > vmax)
        vmax = m_Buffer[i];
    }
  }

private:
  int    m_Buffer[BUFFERSIZE];
  size_t m_Size{0};
};


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
    m_Display.print(m_Buffer);

    m_Display.setFont(nullptr);
    m_Display.setCursor(original_cursor_x + 50,
                        original_cursor_y + LINE_HEIGHT + 3);
    m_Display.print(unit);

    m_Display.drawRect(original_cursor_x, original_cursor_y - Y_OFFSET,
                       GRID_DX, GRID_DY, EPD_BLACK);
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
    m_Display.print(m_Buffer);

    m_Display.setFont(nullptr);
    m_Display.setCursor(original_cursor_x + 50,
                        original_cursor_y + LINE_HEIGHT + 3);
    m_Display.print(unit);

    m_Display.drawRect(original_cursor_x, original_cursor_y - 15, GRID_DX,
                       GRID_DY, EPD_BLACK);
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

    m_Display.print(level == AirQualityLevel::Good ? "good" : "bad");
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

    m_Display.print((y_max - y_min) / 2 + y_min);

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
    drawPlotCells("CO2", "ppm", m_Buffer, m_CO2History, 400, 1500);

    m_Display.setCursor(2 * GRID_DX, 0 * GRID_DY + Y_OFFSET);
    drawDataCell("PM10", "ug/m3", m_SPSData.pm10);

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

  void SetCo2(CO2Data const& data)
  {
    m_CO2Data = data;
    m_TemperatureHistory.append(data.temperature);
    m_HumidityHistory.append(data.humidity);
    m_CO2History.append(data.co2);
  }

  void SetSPS(SPSData const& data)
  {
    m_SPSData = data;
  }

  void SetTVOC(VOCData const& data)
  {
    m_TVOCData = data;
  }

private:
  char    m_Buffer[16];
  CO2Data m_CO2Data;
  SPSData m_SPSData;
  VOCData m_TVOCData;

  DataBuffer m_TemperatureHistory;
  DataBuffer m_CO2History;
  DataBuffer m_HumidityHistory;

  Adafruit_IL91874 m_Display{DISPLAY_WIDTH, DISPLAY_HEIGHT, EPD_DC,
                             EPD_RESET,     EPD_CS,         SRAM_CS};
};
