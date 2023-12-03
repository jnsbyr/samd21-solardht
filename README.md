# Solar DHT Sensor with 433 MHz Transmitter

There are already quite a lot of digital humidity and temperature projects around and some of them are also solar powered. But the functionally more advanced projects tend to use quite a lot of components and typically rather large solar panels. This results in rather bulky devices. 

But I was looking for something that comes closer in size to the solar powered NodOn STPH Enocean sensor. Additionally I wanted to solve the main issue of the Nodon STPH sensor: it does not work solar powered with indirect ambient light.


## Component Selection

To bring "small", "very low powered" and "DIY" together resulted in the following recipe:

* MCU: Seed Studio XIAO SAMD21
* DHT sensor: Silicon Labs Si7021 module
* RF transmitter: Silicon Labs Si4432 module
* energy harvesting: Texas Instruments bq25570 module
* solar panel: 5 V 0.3 W 66x37 mm polycrystalline
* buffer battery: 3.7 V 50 mAh LiPo
* display: optional

The solar panel is not as small as desired, but more about this later on.

### Si7021 Humidity and Temperature Sensor

The Si7021 got primarily selected because it was a leftover from another project. The module already included the 10k pullups for the I2C bus, so it can be used without extra components.

This sensor is not known to have a high accuracy or a low humidity hysteresis. But several the comparisons of low cost sensors agree that there is no clear winner anyway, if you factor in extreme environmental conditions and ageing. Each sensor will show individual advantages and disadvantages, so the Si7021 is as good as any. 

While the temperature offset of my specimen was negligible (less than 0.5 °C), it showed a significant humidity offset of ~10 %. Such an offset might be caused by a sensor contamination, so enabling the build-in heater was the next step. After reaching ~77 °C at heater level 10 the humidity value continued to drop slowly from 7.5 % to 5.5 % within 15 minutes. After cool-down the humidity offset was initially reduced by ~2 % but reverted to ~10 % within a day.

Baking the sensor at 125 °C for more than 12 hours would be the next recommended step but I opted for a repeat at heater level 14, because baking the sensor may change the characteristics permanently and using an oven wastes a lot of energy.

Heater level 14 resulted in more than 93 °C and around 4 % after a few minutes. By additionally using a hot air gun, I let the sensor temperature reach almost 120 °C. After switching the hot air gun off the humidity was still around 4 %, even after more than 3 hours with the heater enabled all the time. So I decided to dab the white PFTE filter a few times using a cotton swab with isopropanol while still hot, assuming the filter could be contaminated from the outside, e.g. by touching. After cool-down the humidity offset was down to ~4 %. Another 3 hours and it was down to ~ 3 %. Another 3 hours and it was down to ~2% .

Recommendation: Do not touch the sensor as it will degrade the accuracy of the humidity measurement. In case of a significant humidity offset, enable the heater for several hours and consider cleaning the PFTE filter with isopropanol.

The Si7021 is a popular sensor and there are several compatible libraries available for the Arduino platform. The best choice for a low power project is the implementation of [Rob Tillaart](https://github.com/RobTillaart/SHT2x) that explicitly supports non-blocking access while e.g. the implementation of [Adafruit](https://github.com/adafruit/Adafruit_Si7021) uses active waiting (delay).

### Si4432 ISM Transceiver

The requirements:

- very low shutdown consumption
- configurable output power
- low consumption in relation to output power
- 433 MHz frequency
- preferably OOK modulation
- optional Manchester encoding by transmitter
- power supply ~ 2.5V
- SPI interface

Looking for a transmitter module with this feature set in early 2023 the market preseted a nice selection of candidates (list is certainly incomplete):

- CC1101
- CMT211xA (RFM11xW)
- RFM69CW, RFM75-S
- Si44xx (RFM2xW / DRF4463F / HC-12)
- SX1212 (RFM64W)
- SYN115

After adding availability and low price to the list of requirements the Si4432 came out top, but primarily because of a shutdown consumption of less than 50 nA where e.g. the popular CC1101 and SX1212 needed 200 nA.

Finding a suitable Arduino library for the Si4432 proofed to be a greater problem than initially anticipated. There is the [RadioHead](http://www.airspayce.com/mikem/arduino/RadioHead/index.html) library, but the focus of this library is more on bidirectional packet radio, as the name of the library suggests. Then there is a library from [Ahmet Ipkin](https://github.com/ADiea/si4432), but the project seems to have been abandoned several years ago. This library has a fork from [nopnop2002](https://github.com/nopnop2002/Arduino-SI4432) that was more up to date. After looking at the code it was obvious that the library did not support all of the features required for this project, especially power management, non-blocking transmit, antenna control and Manchester encoding, so I decided to create a new fork.

Antenna control is something that seems to be an (often erroneous) side-note of the module specification, if at all. But if you ever wondered why your transmission is much worse than others claim, the missing or wrong antenna control might be the reason. The antenna circuit is not part of the chip but something the module designer adds and there are several ways to do this, especially if the chip can both send and receive. Some chips also support antenna diversity. Have a look at annotation AN415 for the Si433x and you will find more background information. If you have a module where the manufacturer does not describe the antenna circuit or where you have doubts that the description is correct, you should try to find out for yourself and add the correct antenna control commands to your firmware. The module I selected for this project is named "XL 4332-SMT" and the antenna circuit of this module has a separate transmit path (activated by GPIO0) and receive receive path (activated by GPIO1). Proper antenna control effectively helps to tune down the transmit power to the required level and this reduces the power requirements. For my use case I was able to select the lowest transmit power setting of the Si4432.



# To be continued ...


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

#### Arduino Core for SAMD21 Low Power Extensions

Copyright (c) 2023 [Jens B.](https://github.com/jnsbyr/arduino-samd21lpe)

[![License: Apache 2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

#### CMSIS Atmel

Copyright (C) 2015 [Atmel Corporation](https://github.com/arduino/ArduinoModule-CMSIS-Atmel)

[![License](https://img.shields.io/badge/License-BSD_2--Clause-orange.svg)](https://opensource.org/licenses/BSD-2-Clause)

#### Si7021 driver (release 0.4.2)

Copyright (C) 2021 [Rob Tillaart](https://github.com/RobTillaart/SHT2x)

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

#### Si4432 driver (release 1.0.0 with project specfic extensions)

Copyright (C) 2014 [Ahmet Ipkin](https://github.com/ADiea/si4432)
Copyright (C) 2022 [Aichi Nagoya](https://github.com/nopnop2002/Arduino-SI4432)

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
