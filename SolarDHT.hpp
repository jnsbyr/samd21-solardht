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

#include <GD_EPDisplay.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <si4432.h>

#include "Measurement.h"
#include "OregonScientific.h"

//#define DEBUG
#define SERIAL_SPEED 115200

#define PIN_UNUSED      0 // TBD

#define PIN_EPD_RST     1 // out
#define PIN_EPD_DC      2 // out
#define PIN_EPD_CS      3 // out
#define PIN_EPD_BUSY    6 // in

#define PIN_RADIO_NIRQ  7 // in
#define PIN_RADIO_NSDN 18 // out
#define PIN_RADIO_CS   17 // out

#define RADIO_TX_POWER 1 // 0..7

#define TEMP_OFFSET 1.3 // [°C] SAMD21 internal temperature immediately after standby is too low

#define HAS_RADIO      1
#define HAS_DISPLAY    1
#define HAS_DHT_SENSOR 0 // NONE: 0 - Si7021: 1 - HDC1080: 2

#define EXECUTION_TIMEOUT 200 // [ms] max. duration from wakeup to end of transmission

#define MIN_DISPLAY_UPDATE_PERIOD 180000 // [ms] 180 s

#define SUPPLY_VOLTAGE_LOW  2.55 // [V] harvester default seems to be around 2.6 V
#define SUPPLY_VOLTAGE_HIGH 3.40 // [V]

#if HAS_DHT_SENSOR == 1
  #include <SHT2x.h>
#endif

#ifdef DEBUG
#  define TRANSMIT_PERIOD 10*1000 // [ms] 10 s test period
#else
#  define TRANSMIT_PERIOD 3UL*60*1000 // [ms] 3 min wakeup period
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
    rtc(RealTimeClock::instance()),
    display(GDEW0102T4(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)),
    hasDisplay(HAS_DISPLAY),
    hasRadio(HAS_RADIO),
    hasSensor(HAS_DHT_SENSOR > 0)
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
    if (hasRadio)
    {
      radio.setModulationType(Si4432::OOK);
      radio.setManchesterEncoding(true, true); // inverted
      radio.setPacketHandling(false, true);    // LSB
      radio.setSendBlocking(false);

      radio.setConfigCallback([]{
        Si4432& radio = SolarDHT::instance().radio;

        radio.setTransmitPower(RADIO_TX_POWER, false);
        radio.setFrequency(433.92);
        radio.setBaudRate(1.4); // OregonScientific::BIT_RATE/1000.0, RTL_433 max. 1400 bits/s

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
      Serial.print("initializing Si4432 with SPI baud rate:");
      Serial.println(baud);
    #endif
      bool radioInitialized = radio.init(&SPI, baud);

      // turn off radio (to save power)
      radioState = RADIO_OFF;
      radio.turnOff();

      if (radioInitialized)
      {
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
      else
      {
      #ifdef DEBUG
        Serial.println("initializing Si4432 failed");
      #endif
        // 2 yellow blinks on radio init error
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);

        radio.turnOff();
        hasRadio = false;
      }
    }
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
    // enable I2C
    if (hasSensor)
    {
#if HAS_DHT_SENSOR > 0
      System::enableClock(GCM_SERCOM0_CORE + PERIPH_WIRE.getSercomIndex(), GCLK_CLKCTRL_GEN_GCLK0_Val);

    #ifdef DEBUG
      Serial.println("initializing Si7021");
    #endif

      // init wire, reset sensor and lower resolution for faster measurement
      bool sensorInitialized = false;
      Wire.begin();
      Wire.setTimeout(200); // [ms]
      if  (sensor.isConnected())
      {
        sensor.reset();
        delay(6); // ~5 ms for soft reset to complete
        sensorInitialized = sensor.isConnected() && sensor.setResolution(3); // 3: 11 bits / ~18 ms
      }

      if (!sensorInitialized)
      {
      #ifdef DEBUG
        Serial.println("initializing Si7021 failed");
      #endif
        // 3 yellow blinks on sensor init error
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);
        delay(200);
        digitalWrite(PIN_LED, LOW);
        delay(100);
        digitalWrite(PIN_LED, HIGH);

        Wire.end();
        hasSensor = false;
      }
#endif
    }
  }

  void setupTimer()
  {
    // configure timer counter to run at 1 kHz
    //timeout.enable(4, GCLKGEN_ID_1K, 1024, TimerCounter::DIV1, TimerCounter::RES16);
    timeout.enable(4, GCLK_CLKCTRL_GEN_GCLK0_Val, SystemCoreClock, TimerCounter::DIV1024, TimerCounter::RES16); // max. 1398 ms
  }

  void setupDisplay()
  {
    // init display (pins, SPI, initial reset)
    if (hasDisplay)
    {
      // configure display
      display.init();
      display.setRotation(1); // 1=landscape
      display.setTextColor(GD_EPDisplay::COLOR_BLACK);
    }
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

    // setup display
    setupDisplay();

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
    Serial.print("WE@"); // watchdog timer enabled
    Serial.println(millis() - wakeupTime); // 0 ms
  #endif

    if (hasRadio)
    {
      // wakeup radio (takes ~17 ms until radio is ready)
      radioState = RADIO_ENABLED;
      radio.turnOn();

    #ifdef DEBUG
      Serial.print("RE@"); // radio enabled
      Serial.println(millis() - wakeupTime); // 1 ms, delta 1 ms
    #endif
    }

    // read supply voltage
    readSupplyVoltage();

    if (hasSensor)
    {
#if HAS_DHT_SENSOR > 0
      // async request humidity (takes ~18 ms with 11 bits resolution)
      Wire.begin();
      if (sensor.isConnected())
      {
        sensor.requestHumidity();
      #ifdef DEBUG
        Serial.print("SR@"); // sensor data requested
        Serial.println(millis() - wakeupTime); // 2 ms, delta 1 ms (OK)
      #endif
      }
      #ifdef DEBUG
      else
      {
        Serial.println("SR!"); // sensor data request error
      }
      #endif
#endif
    }

    if (!hasRadio)
    {
      // no radio: blocking read sensor and display
      readSensor();
      updateDisplay();
    }

#ifdef DEBUG
    Serial.print("IC@"); // init completed
    Serial.println(millis() - wakeupTime); // 2 ms, delta 1 ms (OK)
#endif

    if (hasRadio)
    {
      // do not shutdown completely to keep timer running
      // @todo and because of long XOSC32K/DFLL48M startup time?
      //if (SystemCoreClock > 8000000)
      //{
      System::setSleepMode(System::IDLE2);
      //System::setSleepMode(System::IDLE0);
      //}
    }
    else
    {
      // no radio, shutdown
      shutdown();

      // cancel timeout handler
      timeout.cancel();
    }
  }

  void readSupplyVoltage()
  {
    supplyVoltage = adc.read(ADC_INPUTCTRL_MUXPOS_SCALEDIOVCC_Val);
  }

  void readSensor()
  {
    if (hasSensor)
    {
#if HAS_DHT_SENSOR > 0
      // when used with radio no waiting should be necessary
      int available = 20; // ~15 ms for 11 bit humidity request
      while (!sensor.reqHumReady() && (available-- > 0))
      {
        delay(1);
      }

      bool humidityUpdated = false;
      bool temperatureUpdated = false;
      if (available)
      {
      #ifdef DEBUG
        Serial.print("RHA@");
        Serial.println(millis() - wakeupTime);  // 35 ms, delta 33 ms (OK)
      #endif
        if (sensor.readHumidity())
        {
          // update humidity
          float currentHumidity = sensor.getHumidity();
          humidities.add(currentHumidity);
          humidity = humidities.getAverage();
          humidityUpdated = true;
        #ifdef DEBUG
          Serial.print("RH@");
          Serial.println(millis() - wakeupTime);  // 35 ms, delta 33 ms (OK)
        #endif
          if (sensor.readCachedTemperature())
          {
            // update temperature
            float currentTemp = sensor.getTemperature();
            temperatures.add(currentTemp);
            temperature = temperatures.getAverage();
            temperatureUpdated = true;
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

      // remove oldest sample if not updated to keep average moving
      if (!humidityUpdated)
      {
        humidities.removeOldest();
        humidity = humidities.getAverage();
      }
      if (!temperatureUpdated)
      {
        temperatures.removeOldest();
        temperature = temperatures.getAverage();
      }
#endif
    }
    else
    {
      // no sensor, read SAMD21 temperature and update temperature average
      float currentTemp = adc.read(ADC_INPUTCTRL_MUXPOS_TEMP_Val) + TEMP_OFFSET;
      temperatures.add(currentTemp);
      temperature = temperatures.getAverage();

      // use tens and hundreds of millivolts of Vcc as pseudo humidity
      humidity = round((supplyVoltage*10 - floor(supplyVoltage*10))*100);
    }
  }

  void transmitSensorData()
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
    readSensor();

    // encode and transmit temperature in Oregon Scientific 3.0 format (takes ~108 ms), encode fractional part of supply voltage as humidity
    bool lowBattery = supplyVoltage >= SUPPLY_VOLTAGE_LOW && supplyVoltage < SUPPLY_VOLTAGE_HIGH;
    byte txLen = oregon.encodeTH(0xF824, 1, 0x12, lowBattery, temperature, (byte)round(humidity));
    byte* txBuf = oregon.getMessage();
    radio.setIdleMode(Si4432::SleepMode);
    radio.sendPacket(txLen, txBuf);
    radioState = RADIO_TX;

  #ifdef DEBUG
    Serial.print("TS@");
    Serial.println(millis() - wakeupTime);   // 22 ms, delta 0 ms (OK)
  #endif

    // update display while transmit is in progress (~ 25 ms)
    updateDisplay();
  }

  void displaySensorData()
  {
    const int MARGIN = 10;      // distance from border and distance between words
    const int RIGHT_ALIGN = 72; // right position of number

    char text[8];
    int16_t tbx, tby; uint16_t tbw, tbh;

    display.newScreen();

    display.setFont(&FreeSans18pt7b);
    sprintf(text, "%.1f", temperature);
    display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(RIGHT_ALIGN - tbw, display.height()/2 - MARGIN);
    display.print(text);

    display.setCursor(RIGHT_ALIGN + MARGIN + 13, display.height()/2 - MARGIN);
    display.print("C");

    sprintf(text, "%.0f", humidity);
    display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(RIGHT_ALIGN - tbw, display.height() - MARGIN);
    display.print(text);

    display.setCursor(RIGHT_ALIGN + MARGIN, display.height() - MARGIN);
    display.print("%");

    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(RIGHT_ALIGN + MARGIN, display.height()/2 - MARGIN - 15);
    display.print("o"); // no degree letter available in font, use lower case o

  #ifdef DEBUG
    Serial.print("UD@"); // updating display
    Serial.println(millis() - wakeupTime);
  #endif

    display.updateScreen(true); // reset display, send page image to display, refresh display and power down
  }

  void updateDisplay()
  {
    if (hasDisplay)
    {
      // @TODO update at least once per day?
      // @TODO display sensor data tendency
      // @TODO display sensor error
      // @TODO display transmitter error

      // update display on significant change but not more frequently than every 180 s
      uint32_t now = rtc.getElapsed();
      if ((abs(temperature - displayTemperature) >= 0.5
        || abs(humidity - displayHumidity) >= 3)
        && now >= displayUpdated
        && (now - displayUpdated) >= MIN_DISPLAY_UPDATE_PERIOD)
      {
        // full refresh (~4000 ms) every 6th refresh, otherwise partial refresh (~1500 ms)
        display.setPartialRefresh(displayUpdateCount % 6 != 0);

        // update display content ~25 ms
        displaySensorData();

        displayTemperature = temperature;
        displayHumidity = humidity;
        displayUpdated = now;
        displayUpdateCount++;
      }

    #ifdef DEBUG
      Serial.print("now:");
      Serial.println(now);
      Serial.print("displayUpdated:");
      Serial.println(displayUpdated);
      Serial.print("period:");
      Serial.println(MIN_DISPLAY_UPDATE_PERIOD);
      Serial.print("delta:");
      Serial.println(now - displayUpdated);
    #endif
    }

  #ifdef DEBUG
    Serial.print("T:");
    Serial.println(temperature);
    Serial.print("H:");
    Serial.println(humidity);
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
            transmitSensorData();
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
    if (hasRadio)
    {
      radio.turnOff();
    }

    // send display to deep sleep if unexepectly active
    // notes:
    // - display will stay in deep sleep until an update is performed
    // - display will be automatically send to deep sleep after an update
    if (hasDisplay && !display.isSleeping())
    {
    #ifdef DEBUG
      Serial.print("SD@"); // shutdown display
      Serial.println(millis() - wakeupTime);
    #endif
      display.sleep();
    }

    // turn I2C (SERCOM) off
    if (hasSensor)
    {
      Wire.end();
    }

    // disable LEDs
    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_LED3, HIGH);

  #ifndef DEBUG
    // disable SysTick before entering STANDBY
    System::disableSysTick();

    // select MCU sleep mode STANDBY until next RTC wakeup
    System::setSleepMode(System::STANDBY);
  #else
    Serial.print("SC@"); // shutdown completed
    Serial.println(millis() - wakeupTime); // 131 ms, delta 109 ms (OK)
  #endif
  }

  /**
   * TC ISR (prio 0)
   */
  void timeoutInterrupt()
  {
    // flash LED for 50 ms
  #ifdef DEBUG
    Serial.println("timeout, shutting down");
  #endif
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
#if HAS_DHT_SENSOR == 1
  Si7021 sensor;
#endif
  RadioState radioState;
  RealTimeClock& rtc;
  TimerCounter timeout;
  GD_EPDisplay display;
  Measurement humidities;
  Measurement temperatures;
  float supplyVoltage = 0;
  float temperature = 0;
  float displayTemperature = -999;
  float humidity = 0;
  float displayHumidity = 0;
  uint32_t wakeupTime = 0;
  uint32_t displayUpdated = MIN_DISPLAY_UPDATE_PERIOD/3; // [ms] -> will delay 1st update
  uint16_t displayUpdateCount = 0;
  bool hasDisplay;
  bool hasRadio;
  bool hasSensor;
};
