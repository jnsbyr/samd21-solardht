/*****************************************************************************
 *
 * Solar powered humidity and temperature sensor with 433 MHz transmitter
 *
 * file:     SolarDHT.ino
 * encoding: UTF-8
 * created:  23.04.2023
 *
 *****************************************************************************
 *
 * Copyright (C) 2023 Jens B.
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

#include <System.h>
using namespace SAMD21LPE;

#include "SolarDHT.hpp"


SolarDHT& solarDHT = SolarDHT::instance();

void setup()
{
#ifdef DEBUG
  #if __SAMD21__ && F_CPU != 48000000L
    #error "serial debug over USB requires 48 MHz CPU clock"
  #endif

  // init serial for debug output
  delay(5000);
  Serial.begin(9600);
  while(!Serial);
#else
  // disable non essential MCU modules (including USB)
  System::reducePowerConsumption();
#endif

  // reenable IOs
  System::enablePORT();

  // start real time clock counter, enable and configure radio, setup ADC and start periodic RTC timer
  solarDHT.setup();

#ifndef DEBUG
  // select sleep mode STANDBY between ISR calls and enable sleep-on-exit mode
  System::setSleepOnExitISR(true);
  System::setSleepMode(System::STANDBY);
#endif
}

void loop()
{
  // interrupt driven, do nothing here
#ifdef DEBUG
  delay(1);
#else
  __WFI();
#endif
}
