# Solar DHT Sensor with 433 MHz Transmitter

There are already quite a lot of digital humidity and temperature projects around and some of them are also solar powered. But the functionally more advanced projects tend to use quite a lot of components and typically rather large solar panels. This results in rather bulky devices. 

But I was looking for something that comes closer in size to the solar powered NodOn STPH Enocean sensor. Additionally I wanted to solve the main issue of the Nodon STPH sensor: it does not work solar powered with indirect ambient light.


### Table of contents

[1. Component Selection](#component-selection)  
&nbsp;&nbsp;&nbsp;&nbsp;[1.1 MCU](#mcu)  
&nbsp;&nbsp;&nbsp;&nbsp;[1.2 Humidity and Temperature Sensor](#humidity-and-temperature-sensor)  
&nbsp;&nbsp;&nbsp;&nbsp;[1.3 RF Transmitter](#rf-transmitter)  
&nbsp;&nbsp;&nbsp;&nbsp;[1.4 Solar Cell, Energy Harvesting and Battery](#solar-cell-energy-harvesting-and-battery)  
&nbsp;&nbsp;&nbsp;&nbsp;[1.4 Display](#display)  
[2. Power Consumption](#power-consumption)  
[3. Results](#results)  
[4. Licenses and Credits](#licenses-and-credits)


## Component Selection

To bring "small", "very low powered" and "DIY" together resulted in the following recipe:

* MCU: Seed Studio XIAO SAMD21
* DHT sensor: Silicon Labs Si7021 or Texas Instruments HDC1080 module
* RF transmitter: Silicon Labs Si4432 module
* energy harvesting: Texas Instruments bq25570 module
* solar panel: 5 V 0.3 W 66x37 mm polycrystalline
* buffer battery: 3.7 V 50 mAh LiPo
* display: optional

The solar panel is not as small as desired, but more about this later on.

### MCU

The selection of a suitable MCU for an energy harvesting project was the precursor of this project. 

A short comparison of several popular MCUs and the test results for the SAMD21 can be found in the project [Low Power Stamp Sized Development Boards](https://github.com/jnsbyr/arduino-lowpowertest230203). 

The initial version of the Arduino library [Arduino Core for SAMD21 Low Power Extensions](https://github.com/jnsbyr/arduino-samd21lpe) was created from the test code of this project.

The XIAO board is really small (21x18 mm) and has 14 pins - 3 of them for power supply, the remaining 11 available as GPIOs. But this projects needs 13 GPIOs if the display is used. Luckily the XIAO SAMD21 board has 2 soldering pads on the bottom side for the SWD debug interface. These pins can also be used as GPIOs as long as pin SWCLK is high during boot. To make them available as additional Arduino pins the following lines must be added to the definion of *g_APinDescription* in SDK file *hardware/samd/.../variants/XIAO_m0/variant.cpp*:

```cpp
  // 17..18 - Debug
  // --------------------
  { PORTA, 30, PIO_DIGITAL, PIN_ATTR_DIGITAL, No_ADC_Channel, PWM2_CH2, TCC2_CH2, EXTERNAL_INT_10 }, // TCC1/WO[0] TCC3/WO[4] SWCLK
  { PORTA, 31, PIO_DIGITAL, PIN_ATTR_DIGITAL, No_ADC_Channel, PWM2_CH3, TCC2_CH3, EXTERNAL_INT_11 } // TCC1/WO[1] TCC3/WO[5] SWDIO
```

### Humidity and Temperature Sensor

The Si7021 got primarily selected because it was a leftover from another project. This module already included the 10k pullups for the I2C bus, so it can be used directly without extra components. The device performs a capacitive humidity measurement using a dielectric polymer. 

Missing a reference measurement device for temperature and humidity I had to fall back to a majority vote for judging accuracy, using 4 devices of different manufacture and sensor technology: 2 weather stations, the Si7021 and the HDC1080.

While the temperature difference of all sensors was negligible (less than 0.5 °C at 20 °C), the difference between lowest and highest humidity value was around 8 % after more than 3 hours at more or less constant environmental conditions (e.g. 45 %, 46 %, 51 % and 53 %) - still with no way to tell which of the 4 values was the best. When changing the environmental conditions the difference between the humidity values can temporarily increases to more than 16 %. The humidity value of the Si7021 and the HDC1080 is also affected by the measured temperature as the humidity value is directly related to the chip temperature. Using maximum temperature resolution seems to stabilize temperature and humidity values. Both sensors react within seconds to any minor change in the environmental conditions, but that may be partially due to the fact that the weather stations do not display decimal places for the humidity value.

Before starting the comparison of the 4 sensors the typical difference between the Si7021 and one of the weather stations was around 10 %. Such an offset might be caused by a sensor contamination. The Si7021 datasheet recommends 2 procedures to cope with humidity offset: a) using the internal heater or b) baking and rehydrating the chip. Enabling the internal heater at level 10 caused the sensor to report ~77 °C and the humidity value continued to drop slowly from 7.5 % to 5.5 % within 15 minutes. After cool-down the humidity offset was initially reduced toy ~8 % but reverted to ~10 % within a day. 

Baking the sensor at 125 °C for more than 12 hours would be the next recommended step, but I opted for a repeat at heater level 14, because baking the sensor may change the characteristics permanently and using an oven wastes a lot of energy. Heater level 14 resulted in more than 93 °C and a humidity of ~4 % after a few minutes. By additionally using a hot air gun, I let the sensor temperature reach almost 120 °C. After switching the hot air gun off the humidity was still around 4 %, even after more than 3 hours with the heater enabled all the time. So I decided to dab the white PFTE filter a few times using a cotton swab with isopropanol while still hot, assuming the filter could be contaminated from the outside, e.g. by touching. After cool-down the humidity offset was down to ~4 %. Another 3 hours and it was down to ~ 3 %. Another 3 hours and it was down to ~2 %.

It was not really surprising that the offset slowly increased again, to ~6 % within the next weeks, but that is still significantly less than the initial difference of ~10%.

Recommendation: Do not touch the sensor as it will probably affect the humidity measurement. In case of a significant humidity offset, enable the heater for several hours and consider cleaning the PFTE filter with isopropanol.

From the Arduino libraries available for the Si7021, the best choice for a low power project is the implementation of [Rob Tillaart](https://github.com/RobTillaart/SHT2x) that explicitly supports non-blocking access while e.g. the implementation of [Adafruit](https://github.com/adafruit/Adafruit_Si7021) uses active waiting (delay).

Alternatively the HDC1080 can be used, which has more or less the same sensor capabilities as the Si7021. The HDC1080 is a little faster than the Si7021, with a higher supply current during measurement, but roughly the same energy requirements. One disadvantage of the HDC1080 is the weak heater (24 mW) compared to the the Si7021 (10 .. 311 mW) if you plan to use it. The Si7021 also comes with an additional PFTE filter that covers the sensor, but I cannot say if this is an advantage or a disadvantage. There a quite a few Arduino libraries available that support the HDC1080. But I could not find one that met all requirements of this project, so I created a new [HDC10XX library](https://github.com/jnsbyr/arduino-hdc10xx/") from scratch.

### RF Transmitter

The requirements:

- very low shutdown consumption
- configurable output power
- low consumption in relation to output power
- 433 MHz frequency
- preferably OOK modulation
- optional Manchester encoding by transmitter
- power supply ~ 2.5V
- SPI interface

Looking for a transmitter module with this feature set in early 2023 the market presented a nice selection of candidates (list is certainly incomplete):

- CC1101
- CMT211xA (RFM11xW)
- RFM69CW, RFM75-S
- Si44xx (RFM2xW / DRF4463F / HC-12)
- SX1212 (RFM64W)
- SYN115

After adding availability and low price to the list of requirements the Si4432 came out top, but primarily because of a shutdown consumption of less than 50 nA where e.g. the popular CC1101 and SX1212 needed 200 nA.

Finding a suitable Arduino library for the Si4432 proofed to be a greater problem than initially anticipated. There is the [RadioHead](http://www.airspayce.com/mikem/arduino/RadioHead/index.html) library, but the focus of this library is more on bidirectional packet radio, as the name of the library suggests. Then there is a library from [Ahmet Ipkin](https://github.com/ADiea/si4432), but the project seems to have been abandoned several years ago. This library has a fork from [nopnop2002](https://github.com/nopnop2002/Arduino-SI4432) that was more up to date. After looking at the code it was obvious that the library did not support all of the features required for this project, especially power management, non-blocking transmit, antenna control and Manchester encoding, so I decided to create a new fork.

Antenna control is something that seems to be an (often erroneous) side-note of the RF module specification, if mentioned at all. But if you ever wondered why your transmission is so much worse than the RF chip datasheet or others claim, the missing or wrong antenna control might be the reason. The antenna circuit is not part of the RF chip but something the module designer adds. There are several ways to do this, especially if the chip can both send and receive, and some chips also support antenna diversity. Have a look at annotation AN415 for the Si433x and you will find more background information. If you have a module where the manufacturer does not describe the antenna circuit or where you have doubts that the description is correct, you should try to find out for yourself and add the correct antenna control commands to your firmware. The RF module I selected for this project is named "XL 4332-SMT" and the antenna circuit of this module has a separate transmit and receive receive path wired to the RF chip GPIO0 and GPIO1 respectively. Proper antenna control effectively helps to tune down the transmit power to the required level and this reduces the power requirements. For my use case I was able to select 2nd lowest transmit power setting of the Si4432 (4 dBm).

#### RF protocol

The Si4432 is typically used for long range packet radio applications (some claim for distances up to 1000 km using yagi antennas) at frequencies below 1 GHz with data rates up to 50 kbit/s.

Missing a suitable packet radio receiver but having a 433 MHz SDR with [RTL_433](https://github.com/merbanan/rtl_433) I opted to simulate the THGR810 wireless DHT sensor from [Oregon Scientific](https://www.oregonscientificstore.com/) as the protocol specification of this sensor has been well documented. Having the choice of several protocol variants, protocol version 3.0 is the favorite for a low power project because a single message requires around 100 ms while protocol version 2.1 takes 4 times as long. 

Still, transmitting for 100 ms is a very long time for a message size of 13 bytes with a payload size of 3.5 bytes for temperature, humidity and battery status, but that is what it takes at a data rate of 1 kbit/s. Increasing the data rate to 50 kbit/s the transmission time would decrease to 2 ms, but this would break the protocol specifications and does not work out of the box with the RTL_433 receiver. As the radio transmission consumes more than 80 % of the total energy required the significant reduction of the transmission time must be the next step.

### Solar Cell, Energy Harvesting and Battery

The solar cell is a critical component for an energy harvesting project. It is the power source on which the rest of the device and the firmware features depend on. But the requirements "small" and "should work at low ambient light" seem to be mutually exclusive. To make the requirement "should work at low ambient light" more specific, a low brightness of 50 lux for at least 6 hours per day should be enough to harvest the required energy.

The CJMU-2557 energy harvesting module is based on the Texas Instruments bq25570 boost charger/converter. The specifications provided by the resellers are often incomplete or misleading. Here is my summary based on the Texas instruments datasheet and my test results:

min. input for cold start (VIN_DC): 0.7 V
max. input (VIN_DC):                5.1 V
min. battery for output (VBAT_UV):  2.0 V
max. battery charging (VBAT_OV):    4.07 V
typ. output (VOUT):                 2.57 V
max. output (IOUT):                 0.093 A
V_OK -> H at VBAT_OK + HYST:        3.26 V
V_OK -> L at VBAT_OK:               2.85 V
efficiency:                         > 60 % @ V_IN_DC > 1 V, IOUT > 1 mA

The module needs an energy buffer attached to the BAT terminals and is targeted for a LiPo battery with 3.7 V. Considering that a LiPo battery will degrade to about 80 % of the nominal capacity after around 400 charging cycles shows that this concept is not ideal to operate without maintenance for several years. Due to the continuous charging and discharging there will typically be no full charge cycles and the upper charging voltage of 4.07 V will also extend the lifetime of the LiPo battery somewhat. But the actual lifetime of the LiPo battery is hard to predict and may also dependent on factors like the operating temperature.

Tests with several solar cells below 70x40 mm showed that even with the largest cell it needs at least 200 lux to start the harvesting. As 200 lux do not create a relevant yield and the power conversion efficiency of the module is around 60 %, a significantly higher brightness will be required to generate a slight energy surplus for continuous operation. 

Outdoors you will probably get 4000 lux or more, even on a cloudy day. Depending on the average daylight hours and brightness an even smaller solar cell can be considered.

On an inside windowsill about half of the outdoor brightness remains, but a windowsill is typically not the best place to put a DHT. In a room corner without direct light or in a room with shades typically 50 lux or less remain. Classic indoor light will provide about the same amount. For this situation several small panels would have to be combined or a larger panel needs to be selected. This does not agree with the "small" requirement.

So it is typically not a problem to operate an electronic device with a consumption of around 100 µW by a small solar cell outdoors, but indoors at low ambient light this requires a specifically designed circuit with a consumption well below 1 µW.

There are companies that claim to have a solar powered harvester with a yield of more than 50 µW at 200 lux, but they do not seem to offer to end users.

To use this project under adverse light conditions the solar energy harvester can be replaced with a battery, e.g. a Lithium button cell. This will reduce the design complexity and will allow to build a smaller device.

Under certain light conditions it may also be an option to keep the solar cell, but use a LiPo battery with a higher capacity than 50 mAh to bridge extended times of darkness while recharging during extended times of brightness. Precharging the LiPo battery is advisable in any case.

## Display

Having a local display is optional for a sensor with a RF transmitter. But at least in my perception it adds significantly to the usability because it can show the measurement results as well as additional status information (e.g. low battery state).

Finding a display suitable for this project turned out to be difficult. The dimensions should be around 30x15 mm, providing a display area around 1" and the power consumption should not change to total power budget significantly.

Segmented LCDs should be good choice. They require very little energy to maintain state and are available for transreflective use without backlight. They can be used in direct sunlight and cope acceptably with different temperatures. But it is hard to find a suitable module as they are typically custom build.

Small OLED displays are readily available, but this technology has drawbacks that need to be addressed:

- requires around 3 mW to maintain (e.g. SSD1306)
- brightness of pixels fades noticeably with activity time, especially problematic when continuously displaying similar content

These aspects can easily be resolved by operating the display on demand, e.g. by adding a button, but this reduces the usability slightly.

Dot matrix LCDs and TFT are also available, but they typically require even more power to maintain than an OLED of the same size.

Almost out of options that leaves ePaper displays to consider. Their operation is significantly different than all other display technologies. This comes with several limitations and some advantages:

- high black/white contrast
- works with supply voltage down to 2.3 V
- should not be updated faster than every 3 min
- must be protected from direct sunlight
- full display refresh takes up to 4000 ms, partial refresh up to 1500 ms
- requires around 700 nW to maintain
- requires around 20 mJ per full refresh, 7.5 mJ per partial refresh
- requires around 53 µW when updated every 3 min with 1 full refresh every 6 updates
- display content inverts several times when performing a full refresh

So an ePaper display is suitable for content that does not change at a high rate and where a flickering display update is acceptable. This works for environmental values like temperature and humidity as they typically change slowly. And when used indoors protection from direct sunlight should not be a problem. The power requirements are still high with 56 µW for 20 updates per hour. But with a measurement value change filter the update rate can be lowered further so that the power requirement should drop below 5 µW.

To see how it works out I selected the Dalian 1.02" b/w E Ink display. Originally I wanted to use the Arduino [GxEPD2 library](https://github.com/ZinggJM/GxEPD2/), but my tests showed that while supporting many different display models this library was not designed for low power usage and its GPL3.0 license is not compatible with my project.

Another thing of note is the SPI interface. The Dalian datasheet talks about 3-wire and 4-wire SPI interface mode. This is not referring to the well known SPI lines SCK, MOSI, MISO and SS. The display only supports half-duplex SPI without a MISO line, resulting in 3 lines anyway. Not all SPI libraries and SPI hardware can handle half-duplex mode, so in most cases only the write commands can be used. As the Dalian display breakout board wiring forces the 4-wire SPI mode, the 3-wire mode is not really an option anyway. Using the 3-wire mode would also require a 9 bit SPI word, another thing that may not be available in all SPI implementations. The 9th bit marks the SPI word as command or data. In 4-wire mode this is done with an additional D/C-line and a SPI word size of 8 bits. On top of these 4 lines the display also needs a line for reset to be able to wakeup from deep sleep and a line for the busy signal to check the operation status of the display without needing to rely on SPI half-duplex read operations. But this kind of wiring is not specific for this display and can also be found with other displays.

## Power Consumption

The main focus of this project was low power consumption. Here are some measurement results:

voltage:         3.3 V

#### boot

avg. current:    510 µA \
power:           1.6 mW \
duration:       2000 ms \
energy:          3.4 mJ

#### activity

avg. current:     25 mA (mostly the Si4432 in 4 dBm transmit mode, no display update) \
power:            83 mW \
duration:        130 ms \
energy:           11 mJ \
period:          180 s \
energy per hour: 216 mJ \
power:            60 µW

#### standby

current:           2 µA \
energy per hour:  24 mJ \
power:             7 µW

#### display (estimated, see comments above)

period:          180 s \
energy per hour: 192 mJ \
power:            53 µW  

#### SolarDHT totals

energy per hour: 432 mJ \
power:           120 µW

#### Blink totals (same board, nothing connected)

voltage:           3 V \
current:          12 mA \
energy per hour: 132 J \
power:            37 mW

-> power factor Blink/SolarDHT: 308

#### LiPo battery 50 mAh 3.7 V 

energy of batteries: 666 J

-> SolarDHT runtime: ~100 d without display, >50 d with display

#### Lithium button cell CR2023 200 mAh 3.0 V

energy of batteries: 2160 J

-> SolarDHT runtime: ~400 d without display, >200 d with display

**********

Oregon Scientific THGR122NX with 2x LR3 batteries 850 mAh 1.5 V

energy of batteries: 9000 J \
runtime:         600 d \
energy per hour: 625 mJ \
period:           50 s \
energy per meas:   9 mJ  \
protocol: Oregon Scientific 2.1

The THGR122NX is about 5 times more energy efficient per meas than this project when both are using the Oregon Scientific protocol 2.1. The key design difference is that the Si4432 is not a transmitter but a transceiver that comes with its own build-in CPU causing a significant power offset.

Similar results could be achieved by creating the bit stream with the SAMD21 and using a pure transmitter like the SYN115. 

By selecting the Oregon Scientific protocol 3.0 for this project the difference in energy efficiency becomes negligible.

Significantly better results are possible by increasing the data rate by a factor of 10 or more.


## Results

Parts of my expectations have been met, e.g. an overall power consumption of 120 µW for a wireless sensor with a permanent display. 

But it was also sobering to see that every software component this project is build on had to be improved or rewritten, including the SAM D21 SDK and all device drivers. The use of *delay()* in device drivers and the resulting APIs and programming models disregard the capabilities and power requirements of state of the art MCUs.

During this project I have come to terms with the fact that ambient light energy harvesting at 200 lx or lower is still a topic for research. The conversion efficiency of readily available, inexpensive and compact generators is still too low to be of practical use, even for low power devices. Buf if the average brightness is high enough my prototype operates completely self-sustaining. 

Currently this project is missing a PCB and an enclosure. When not used exclusively at environmental conditions near 20 °C and 50 %RH an enclosure with 2 chambers is recommended, one providing mechanical protection for the sensor and one protecting the circuit from condensation. 

What I am missing most is a way to send information to the sensor, e.g. to configure the transmit period or to provide time synchronization. As the Si4432 is also able to receive, these features could be added without requiring hardware modifications. But there are no protocol standards available for 433 MHz that can be used for this purpose that are supported by typical controllers (RF gateway, smart home, etc.). Improving compatibility in this respect requires choosing a popular wireless technology (WiFi, BLE, EnOcean, ZigBee, etc.) with all its advantages and disadvantages. If a transmit range of significantly more than 20 m is required, 433 MHz remains a very good choice.


## Licenses and Credits

### Documentation and Photos

Copyright (c) 2023 [Jens B.](https://github.com/jnsbyr)

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

### Source Code

Copyright (c) 2023 [Jens B.](https://github.com/jnsbyr/samd21-solardht)

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

The source code was edited and build using the [Arduino IDE](https://www.arduino.cc/en/software/), [Arduino CLI](https://github.com/arduino/arduino-cli) and [Microsoft Visual Studio Code](https://code.visualstudio.com).

The source code depends on:

#### Arduino SDK & Seed Studio SDK for SAMD21 and SAMD51

Copyright (C) 2014 [Arduino LLC](https://github.com/arduino/Arduino)

[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL%202.1%20only-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)

#### Arduino Core for SAMD21 Low Power Extensions (release 1.0.8)

Copyright (c) 2023 [Jens B.](https://github.com/jnsbyr/arduino-samd21lpe)

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

#### CMSIS Atmel

Copyright (C) 2015 [Atmel Corporation](https://github.com/arduino/ArduinoModule-CMSIS-Atmel)

[![License](https://img.shields.io/badge/License-BSD_2--Clause-orange.svg)](https://opensource.org/licenses/BSD-2-Clause)

#### HDC1080 sensor driver (release 1.0.0)

Copyright (C) 2024 [Jens B.](https://github.com/jnsbyr/arduino-hdc10xx)

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

#### Si7021 sensor driver (release 0.4.2)

Copyright (C) 2021 [Rob Tillaart](https://github.com/RobTillaart/SHT2x)

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

#### Si4432 transceiver driver (release 1.1.2)

Copyright (C) 2014 [Ahmet Ipkin](https://github.com/ADiea/si4432)
Copyright (C) 2022 [Aichi Nagoya](https://github.com/nopnop2002/Arduino-SI4432)

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

#### Good Display ePaper driver (release 1.0.0)

Copyright (c) 2024 [Jens B.](https://github.com/jnsbyr/)

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
