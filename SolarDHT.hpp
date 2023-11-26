/*****************************************************************************
 *
 * Solar powered humidity and temperature sensor with 433 MHz transmitter
 *
 * file:     SolarDHT.hpp
 * encoding: UTF-8
 * created:  29.04.2023
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

#pragma once

#include <Analog2DigitalConverter.h>
#include <RealTimeClock.h>
#include <System.h>
#include <TimerCounter.h>
using namespace SAMD21LPE;

#include "Measurement.h"
#include "OregonScientific.h"
#include "si4432.h"
#include "SHT2x.h"

//#define DEBUG

#define PIN_RADIO_NSDN 2
#define PIN_RADIO_CS   6
#define PIN_RADIO_NIRQ 7

#define RADIO_TX_POWER 0 // 0..7

#define TEMP_OFFSET 1.3 // [°C] SAMD21 temperature immediately after standby is too low
#define HAS_DHT_SENSOR

#define EXECUTION_TIMEOUT 200 // [ms] max. duration from wakeup to end of transmission


#ifdef DEBUG
  #define TRANSMIT_PERIOD 10*1000 // [ms]
#else
  #define TRANSMIT_PERIOD 3UL*60*1000 // [ms] wakeup period
#endif


class SolarDHT
{
public:
  enum RadioState
  {
    RADIO_OFF,     // shutdown
    RADIO_ENABLED, // not shutdown
    RADIO_ON,      // clock running
    RADIO_READY,   // configured
    RADIO_TX       // transmitting
  };

public:
  const byte GCLKGEN_ID_1K = 6;

private:
  SolarDHT() :
    adc(Analog2DigitalConverter::instance()),
    radio(PIN_RADIO_CS, PIN_RADIO_NSDN, PIN_RADIO_NIRQ),
    radioState(RADIO_OFF),
    rtc(RealTimeClock::instance())
  {};

public:
  static SolarDHT& instance()
  {
    static SolarDHT sdht;
    return sdht;
  }

public:
  void setupADC()
  {
    // enable ADC (48 MHz / 512 -> 93.75 kHz, 8MHz / 64 -> 125 kHz)
  #if F_CPU == 8000000
    adc.enable(GCLK_CLKCTRL_GEN_GCLK0_Val, SystemCoreClock, Analog2DigitalConverter::DIV64);
  #else
    adc.enable(GCLK_CLKCTRL_GEN_GCLK0_Val, SystemCoreClock, Analog2DigitalConverter::DIV512);
  #endif

    // configure hardware averaging (2^3 = 8)
    adc.setSampling(0, 3);

    // disable ADC until start of conversion (to save power)
    adc.disable();
  }

  void setupRadio()
  {
    // define radio configuration
    radio.setModulationType(Si4432::OOK);
    radio.setManchesterEncoding(true, true); // inverted
    radio.setPacketHandling(false, true);    // LSB
    radio.setSendBlocking(false);

    radio.setConfigCallback([]{
      Si4432& radio = SolarDHT::instance().radio;

      radio.setTransmitPower(RADIO_TX_POWER, false);
      radio.setFrequency(433.92);
      radio.setBaudRate(1.024);

      // antenna rx/tx switch control, GPIO0 = TX, GPIO1 = RX, GPIO2 = unused (e.g. board XL4432-SMT)
      radio.ChangeRegister(Si4432::REG_GPIO0_CONF, Si4432::GPIO_TX_STATE_OUTPUT); // Tx state output
      radio.ChangeRegister(Si4432::REG_GPIO1_CONF, Si4432::GPIO_RX_STATE_OUTPUT); // Rx state output

      // prevent excessive power consumption of the SAMD21 MCU by floating inputs when radio is in shutdown
      PORT->Group[g_APinDescription[PIN_SPI_MISO].ulPort].PINCFG[g_APinDescription[PIN_SPI_MISO].ulPin].reg |= PORT_PINCFG_PULLEN;
    });

    // enable SPI
    System::enableClock(GCM_SERCOM0_CORE + PERIPH_SPI.getSercomIndex(), GCLK_CLKCTRL_GEN_GCLK0_Val);
    System::enableClock(GCM_EIC, GCLK_CLKCTRL_GEN_GCLK0_Val); // @todo Why is this needed here? EIC will be enabled a little later anyway.

    // enable radio (mainly for verification)
    uint32_t baud = 4000000; // baud*SERCOM_SPI_FREQ_REF/F_CPU;
  #ifdef DEBUG
    Serial.print("initializing SI4432 for SPI baud rate:");
    Serial.println(baud);
  #endif
    bool radioInitialized = radio.init(&SPI, baud);

    // turn off radio (to save power)
    radioState = RADIO_OFF;
    radio.turnOff();

    // halt on radio init fail
    if (!radioInitialized)
    {
      digitalWrite(PIN_LED, LOW);
      delay(100);
      digitalWrite(PIN_LED, HIGH);
      delay(200);
      digitalWrite(PIN_LED, LOW);
      delay(100);
      digitalWrite(PIN_LED, HIGH);
      System::setSleepMode(System::STANDBY);
      while(1) System::sleep();
    }

    // enable radio interrupt handling, change EIC GCLKGEN (to save power) and lower priority (to enable SysTick)
    noInterrupts();
    pinMode(radio.getIntPin(), INPUT_PULLUP);
    attachInterrupt(radio.getIntPin(), []{ SolarDHT::instance().radioInterrupt(); }, LOW);
    //System::enableClock(GCM_EIC, GCLKGEN_ID_1K);
    NVIC_DisableIRQ(EIC_IRQn);
    NVIC_SetPriority(EIC_IRQn, 3);
    NVIC_EnableIRQ(EIC_IRQn);
    interrupts();
  }

  void setupRTC()
  {
    // configure low power clock generator to run at 1 kHz
    System::setupClockGenOSCULP32K(GCLKGEN_ID_1K, 4); // 2^(4+1) = 32 -> 1 kHz

    // enable RTC timer (1 kHz tick, 1 ms duration resolution)
    rtc.enable(GCLKGEN_ID_1K, 1024, 1);
  }

  void setupSensor()
  {
    #ifdef HAS_DHT_SENSOR
      // enable I2C
      System::enableClock(GCM_SERCOM0_CORE + PERIPH_WIRE.getSercomIndex(), GCLK_CLKCTRL_GEN_GCLK0_Val);

      // init wire, reset sensor and lower resolution for faster measurement
      bool sensorInitialized = false;
      Wire.begin();
      if  (sensor.isConnected())
      {
        sensor.reset();
        delay(6); // ~5 ms for soft reset to complete
        sensorInitialized = sensor.isConnected() && sensor.setResolution(3); // 3: 11 bits / ~18 ms
      }

      // halt on sensor init fail
      if (!sensorInitialized)
      {
        Wire.end();
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);
        System::setSleepMode(System::STANDBY);
        while(1) System::sleep();
      }
    #endif
  }

  void setupTimer()
  {
    // configure timer counter to run at 1 kHz
    //timeout.enable(4, GCLKGEN_ID_1K, 1024, TimerCounter::DIV1, TimerCounter::RES16);
    timeout.enable(4, GCLK_CLKCTRL_GEN_GCLK0_Val, SystemCoreClock, TimerCounter::DIV1024, TimerCounter::RES16); // max. 1398 ms
  }

  /**
   * setup MCU features for periodic temperature/humidity measurement and transmission
   * and start inital measurement and transmission
   */
  void setup()
  {
    // start RTC counter
    setupRTC();

    // enable and configure radio
    setupRadio();

    // setup ADC and timeout timer
    setupADC();
    setupTimer();

    // setup temperature and humidity sensor
    setupSensor();

    // start RTC timer for periodic wakeup
    rtc.start(TRANSMIT_PERIOD, true, []{ SolarDHT::instance().wakeupInterrupt(); });

    // perform initial measurement and transmission
    wakeupInterrupt();
  }

  /**
   * RTC ISR (prio 3)
   */
  void wakeupInterrupt()
  {
  #ifndef DEBUG
    // reenable SysTick after wakeup from STANDBY
    System::enableSysTick();
  #endif

    wakeupTime = millis();

    digitalWrite(PIN_LED3, LOW);
    digitalWrite(PIN_LED, HIGH);

    // start watchdog timer
    timeout.start(EXECUTION_TIMEOUT, false, []{ SolarDHT::instance().timeoutInterrupt(); });

  #ifdef DEBUG
    Serial.print("TE@");
    Serial.println(millis() - wakeupTime); // 0 ms
  #endif

    // wakeup radio (takes ~17 ms until radio is ready)
    radioState = RADIO_ENABLED;
    radio.turnOn();

  #ifdef DEBUG
    Serial.print("RE@");
    Serial.println(millis() - wakeupTime); // 1 ms, delta 1 ms
  #endif

    // read supply voltage
    readSupplyVoltage();

  #ifdef HAS_DHT_SENSOR
    // request humidity (takes ~18 ms with 11 bits resolution)
    Wire.begin();
    if (sensor.isConnected())
    {
      sensor.requestHumidity();
    }
    else
    {
    #ifdef DEBUG
      Serial.println("CHF");
    #endif
    }
  #else
    // read chip temperature
    readTemperature();
  #endif

  #ifdef DEBUG
    Serial.print("TR@");
    Serial.println(millis() - wakeupTime); // 2 ms, delta 1 ms (OK)
  #endif

    // do not shutdown completely to keep timer running
    // @todo and because of long XOSC32K/DFLL48M startup time?
    //if (SystemCoreClock > 8000000)
    //{
    System::setSleepMode(System::IDLE2);
    //System::setSleepMode(System::IDLE0);
    //}
  }

  void readSupplyVoltage()
  {
    supplyVoltage = adc.read(ADC_INPUTCTRL_MUXPOS_SCALEDIOVCC_Val);
  }

  void readTemperature()
  {
  #ifdef HAS_DHT_SENSOR
    // no waiting should be necessary
    int available = 20; // ~15 ms for 11 bit humidity request
    while (!sensor.reqHumReady() && (available-- > 0))
    {
      delay(1);
    }
    if (available)
    {
    #ifdef DEBUG
      Serial.print("RHA@");
      Serial.println(millis() - wakeupTime);  // 35 ms, delta 33 ms (OK)
    #endif
      if (sensor.readHumidity())
      {
        // update humidity
        humidity = sensor.getHumidity();
      #ifdef DEBUG
        Serial.print("RH@");
        Serial.println(millis() - wakeupTime);  // 35 ms, delta 33 ms (OK)
      #endif
        if (sensor.readTemperatureForHumidity())
        {
          // update temperature
          temperature = sensor.getTemperature();
        #ifdef DEBUG
          Serial.print("RT@");
          Serial.println(millis() - wakeupTime);  // 35 ms, delta 33 ms (OK)
        #endif
        }
      }
      else
      {
      #ifdef DEBUG
        Serial.println("RHF");
      #endif
      }
    #ifdef DEBUG
      Serial.print("RHT@");
      Serial.println(millis() - wakeupTime);  // 35 ms, delta 33 ms (OK)
    #endif
    }
    else
    {
    #ifdef DEBUG
      Serial.print("RTTO@");
      Serial.println(millis() - wakeupTime);  // 20 ms, delta 18 ms (OK)
    #endif
    }
  #else
    // update temperature average
    float currentTemp = adc.read(ADC_INPUTCTRL_MUXPOS_TEMP_Val) + TEMP_OFFSET;
    temperatures.add(currentTemp);
    temperature = temperatures.getAverage();

    // use tens and hundreds of millivolts of Vcc as pseudo humidity
    humidity = round((supplyVoltage*10 - floor(supplyVoltage*10))*100);
  #endif
  }

  void transmitTemperature()
  {
  #ifdef DEBUG
    Serial.print("RO@");
    Serial.println(millis() - wakeupTime);  // 20 ms, delta 18 ms (OK)
  #endif
    //digitalWrite(PIN_LED3, LOW);

    // config radio
    radio.setIdleMode(Si4432::Ready);
    radio.boot();
    radioState = RADIO_READY;

  #ifdef DEBUG
    Serial.print("RC@");
    Serial.println(millis() - wakeupTime);   // 22 ms, delta 2 ms
  #endif

    // get temperature
  #ifdef HAS_DHT_SENSOR
    readTemperature();
  #endif

    // encode and transmit temperature in Oregon Scientific 3.0 format (takes ~108 ms), encode fractional part of supply voltage as humidity
    bool lowBattery = supplyVoltage >= 2.55 && supplyVoltage < 2.65; // default seems to be around 2.6 V
    byte txLen = oregon.encodeTH(0xF824, 1, 0x12, lowBattery, temperature, (byte)round(humidity));
    byte* txBuf = oregon.getMessage();
    radio.setIdleMode(Si4432::SleepMode);
    radio.sendPacket(txLen, txBuf);
    radioState = RADIO_TX;

  #ifdef DEBUG
    Serial.print("TS@");
    Serial.println(millis() - wakeupTime);   // 22 ms, delta 0 ms (OK)
  #endif
  }

  /**
   * EIC ISR (prio 3)
   */
  void radioInterrupt()
  {
    // ISR is sometimes called when interrupt pin is not stable, so check again
    bool interrupt = !digitalRead(radio.getIntPin());

  #ifdef DEBUG
    Serial.print("RI:");
    Serial.println(interrupt);
  #endif

    if (interrupt)
    {
      uint16_t intStatus = radio.getIntStatus();

      switch (radioState)
      {
        case RADIO_ENABLED:
          if (intStatus & Si4432::INT_CHIPRDY)
          {
            // radio on, transmit temperature
            radioState = RADIO_ON;
            transmitTemperature();
          }
          break;

        case RADIO_TX:
          if (intStatus & Si4432::INT_PKSENT)
          {
            // transmit completed, turn radio off and shut down
            radioState = RADIO_READY;
            shutdown();

            // cancel timeout handler
            timeout.cancel();
          }
          break;

        default:
          // ignore
          break;
      }
    }
  }

  /**
   * shutdown peripherals before standby
   */
  void shutdown()
  {
    // turn off radio
    radio.turnOff();

    // turn I2C (SERCOM) off
    Wire.end();

    // disable LEDs
    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_LED3, HIGH);

  #ifndef DEBUG
    // disable SysTick before entering STANDBY
    System::disableSysTick();

    // select MCU sleep mode STANDBY until next RTC wakeup
    System::setSleepMode(System::STANDBY);
  #else
    Serial.print("DR@");
    Serial.println(millis() - wakeupTime); // 131 ms, delta 109 ms (OK)
  #endif
  }

  /**
   * TC ISR (prio 0)
   */
  void timeoutInterrupt()
  {
    // flash LED
    digitalWrite(PIN_LED, LOW);
    uint32_t start = rtc.getElapsed();
    while (rtc.getElapsed() - start < 50);

    // timeout, abort all operations by shutting down
    shutdown();
  }

public:
  Analog2DigitalConverter& adc;
  OregonScientific oregon;
  Si4432 radio;
  Si7021  sensor;
  RadioState radioState;
  RealTimeClock& rtc;
  TimerCounter timeout;
  Measurement temperatures;
  float supplyVoltage = 0;
  float temperature = 0;
  float humidity = 0;
  uint32_t wakeupTime = 0;
};
