#pragma once

#include <sps30.h>

struct SPSData : public Printable
{
  explicit SPSData() = default;
  explicit SPSData(sps30_measurement& m)
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

class SPSSensor
{
public:
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

  SPSData GetMeasurement()
  {
    uint16_t has_data;

    if (sps30_read_data_ready(&has_data) < 0)
    {
      return SPSData{};
    }

    if (!has_data)
      return SPSData{};


    struct sps30_measurement m;
    if (sps30_read_measurement(&m) < 0)
    {
      return SPSData{};
    }

    return SPSData{m};
  }
};