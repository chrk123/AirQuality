#pragma once

#include "CO2Sensor.h"
#include "SPSSensor.h"
#include "VOCSensor.h"

enum class AirQualityLevel
{
  Good,
  Bad,
};

AirQualityLevel JudgeQuality(VOCData const& voc, CO2Data const& co2,
                             SPSData const& sps)
{
  if (co2.co2 > 1000 || co2.humidity > 60)
    return AirQualityLevel::Bad;

  return AirQualityLevel::Good;
}
