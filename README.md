# Solar DHT Sensor with 433 MHz Transmitter

There are already quite a lot of digital humidity and temperature projects around and some of them are also solar powered. But the functionally more advanced projects tend to use quite a lot of components and typically rather large solar panels. This results in rather bulky devices. 

But I was looking for something that comes closer in size to the solar powered NodOn STPH Enocean sensor. Additionally I wanted to solve the main issue of the Nodon STPH sensor: it does not work solar powered with indirect ambient light.


## Component Selection

To bring "small", "very low powered" and "DIY" together resulted in the following recipe:

* MCU: Seed Studio XIAO SAMD21
* DHT sensor: Silicon Labs Si7021 module
* RF transmitter: Silicon Labs Si4332 module
* energy harvesting: Texas Instruments bq25570 module
* solar panel: 5 V 0.3 W 66x37 mm polycrystalline
* buffer battery: 3.7 V 50 mAh LiPo
* display: optional

The solar panel is not as small as desired, but more about this later on.

### Si7021 Humidity and Temperature Sensor

The Si7021 got selected just because it was already there from another project. 

This sensor is not known to have a high accuracy or a low hysteresis. But all serious comparisons of low cost sensor agree that there is no clear winner anyway, if you factor in extreme environmental conditions and ageing. Each sensor will show individual advantages and disadvantages, so the Si7021 is as good as any. 

My specimen showed a higher humidity by ~10 %. Such a humidity offset might be caused by a reversible sensor contamination, so enabling the build-in heater was the next step. After reaching ~77 °C the humidity value continued to drop slowly from 7.5 % to 5.5 % within 15 minutes. After cool-down the humidity value was initially reduced by ~2 %.


# To be continued ...


## Licenses and Credits

### Documentation and Photos

Copyright (c) 2023 [Jens B.](https://github.com/jnsbyr)

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

### Source Code

Copyright (c) 2023 [Jens B.](https://github.com/jnsbyr)

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

#### Si7021 driver (extended version based on release 0.4.0)

Copyright (C) 2021 [Rob Tillaart](https://github.com/RobTillaart/SHT2x)

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

#### Si4432 driver (extended version based on release 0.1)

Copyright (C) 2014 [Ahmet (theGanymedes) Ipkin](https://github.com/ADiea/si4432)

[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
