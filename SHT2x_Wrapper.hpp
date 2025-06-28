/*****************************************************************************
 *
 * wrapper class for SHT2x driver to provide normalized sensor API
 *
 * file:     SHT2x_Wrapper.hpp
 * encoding: UTF-8
 * created:  12.01.2024
 *
 *****************************************************************************
 *
 * Copyright (C) 2024 Jens B.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *****************************************************************************/

#pragma once

#include <SHT2x.h>


/**
 * application specific wrapper class for SHT2x driver to provide normalized sensor API
 *
 * wrapper features:
 * - heater support
 * - serial ID support
 * - non-blocking API
 *
 * Si7021 device features:
 * - capacitive hygrometer (dielectric polymer)
 * - factory calibrated
 * - typical humidity accuracy +/- 3 %
 * - typical temperature accuracy +/- 0.4 °C
 * - humidity acquisition time 2.6 .. 12 ms
 * - temperature acquisition time 1.5 .. 10.8 ms
 * - power up time 5 .. 80 ms
 * - measurement ambient temperature range -40 .. 125 °C
 * - min. supply voltage: 1.9
 * - sleep current 0.06 µA
 * - humidity acquisition current 150 µA
 * - temperature acquisition current 90 µA
 * - heater current 3.1 .. 94.2 mA
  */
template<class T> class SHT2x_Wrapper
{
public:
  enum AcquisitionType
  {
    ACQ_TYPE_TEMPERATURE = 1,
    ACQ_TYPE_HUMIDITY    = 2,
    ACQ_TYPE_COMBINED    = 3
  };

public:
  SHT2x_Wrapper() = default;

public:
  bool isConnected()
  {
    return dhtSensor.isConnected();
  }

  bool reset()
  {
    return dhtSensor.reset();
  }

  /**
   * RES     HUM       TEMP
   *  0      12 bit    14  bit
   *  1      08 bit    12  bit
   *  2      10 bit    13  bit
   *  3      11 bit    11  bit
   *  4..255 returns false
   */
  bool setResolution(uint8_t humidityBits, uint8_t temperatureBits)
  {
    bool success = true;
    if (humidityBits == 12 && temperatureBits == 14) dhtSensor.setResolution(0);
    else if (humidityBits == 8 && temperatureBits == 12) dhtSensor.setResolution(1);
    else if (humidityBits == 10 && temperatureBits == 13) dhtSensor.setResolution(2);
    else if (humidityBits == 11 && temperatureBits == 11) dhtSensor.setResolution(3);
    else success = false;
    return success;
  }

  bool setHeaterEnabled(bool enabled)
  {
    if (enabled)
      return dhtSensor.heatOn();
    else
      return dhtSensor.heatOff();
  }

  bool isSupplyVoltageOK()
  {
    return dhtSensor.batteryOK();
  }

  uint32_t readSerialIdLow()
  {
    return dhtSensor.getEIDB();
  }

  uint32_t readSerialIdHigh()
  {
    return dhtSensor.getEIDA();
  }

  bool startAcquisition(AcquisitionType acquisitionType)
  {
    switch (acquisitionType)
    {
      case ACQ_TYPE_TEMPERATURE:
        return dhtSensor.requestTemperature();
      default:
        return dhtSensor.requestTemperature();
    }
  }

  bool isAcquisitionComplete()
  {
    return dhtSensor.requestReady();
  }

  bool isHumidityReady()
  {
    return dhtSensor.reqHumReady();
  }

  bool isTemperatureReady()
  {
    return dhtSensor.reqHumReady();
  }

  bool readHumidity()
  {
    switch (dhtSensor.getRequestType())
    {
      case SHT2x_REQ_HUMIDITY:
        return dhtSensor.readHumidity();
      default:
        return false;
    }
  }

  bool readTemperature()
  {
    switch (dhtSensor.getRequestType())
    {
      case SHT2x_REQ_TEMPERATURE:
        return dhtSensor.readTemperature();
      case SHT2x_REQ_HUMIDITY:
        return dhtSensor.readCachedTemperature();
      default:
        return false;
    }
  }

  float getHumidity()
  {
    return dhtSensor.getHumidity();
  }

  float getTemperature()
  {
    return dhtSensor.getTemperature();
  }

protected:
  T dhtSensor;
};
