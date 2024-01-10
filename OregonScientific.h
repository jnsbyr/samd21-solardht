/*****************************************************************************
 *
 * Oregon Scientfic Protocol Encoder
 *
 * file:     OregonScientific.h
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

/**
 * @see https://wmrx00.sourceforge.net/ for specification details
 */
class OregonScientific
{
public:
  static const byte MAX_MESSAGE_SIZE = 13; // [bytes]
  static const u_int16_t BIT_RATE = 1024; // [bit/s]

public:
  OregonScientific() = default;

public:
  /**
   * if enabled will invert each bit, default disabled
   */
  void setInvertBits(bool enabled)
  {
    invertBits = enabled;
  }

  /**
   * if enabled will swap nibbles in each input byte, default enable = most significant nibble first
   */
  void setFlipInputNibbles(bool enabled)
  {
    flipInputNibbles = enabled;
  }

  /**
   * if enabled will swap nibbles in each output byte, default disabled = 1st nibble -> lower nibble
   */
  void setFlipOutputNibbles(bool enabled)
  {
    flipOutputNibbles = enabled;
  }


  /**
   * encode message for temperature and dewpoint
   *
   * OD 2.1 frame: 2.5 + 1 = 3.5 bytes, total 12 bytes, note: must be inverted/interleaved bitwise
   * OD 3.0 frame: 3.5 + 1 = 4.5 bytes, total 13 bytes
   * message:      4 + 1 = 5 bytes
   * TH data:      3.5 bytes
   *
   * Oregon receiver:
   * - most significant nibble first (flipInputNibbles)
   * - least significant bit first (requires a transmitter option)
   * - all bits inverted (invertBits or transmitter option)
   *
   * @param id model ID
   *           0x1D20 (OS 2.1: THGR122NX, THGN123N)
   *           0x1D30 (OS 2.1: THGR968)
   *           0xF824 (OS 3.0: THGN801, THGR810)
   *           0xF8B4 (OS 3.0: THGR810)
   * @param channel device channel, 1 .. 3
   * @param rollingCode house code, created at power up
   * @param lowBatt low battery
   * @param temp temperature [°C], min -99.9, max 99.9
   * @param hum humidity [%], min 0, max 99
   * @return message size on success [bytes], should be 12/13 bytes, or 0 on error
   */
  byte encodeTH(uint16_t id, byte channel, byte rollingCode, bool lowBatt, float temp, byte hum)
  {
    // clear message buffer
    bzero(message, MAX_MESSAGE_SIZE);
    nibbles = 0;

    if (channel >= 1 && channel <= 3)
    {
      // preamble
      byte version = (id == 0x1D20 || id == 0x1D30)? 2 : 3;
      for (byte i=0; i<version; i++)
        addByte(0xFF);
      // sync
      //addNibble(0b0101);
      addNibble(0b1010);

      // init checksum
      checksum = 0;

      // id
      addByte(id >> 8);
      addByte(id & 0xFF);
      // channel
      addNibble((1 << (channel - 1)) & 0xF);
      // rolling code
      addByte(rollingCode);
      // flags
      addNibble(lowBatt? 0x4 : 0);

      // temperature
      uint16_t t = round(abs(temp)*10);
      if (t > 999) t = 999;
      for (byte i=0; i<3; i++)
      {
        addNibble(t % 10);
        t /= 10;
      }

      // temperature sign
      addNibble(temp >= 0? 0: 1);

      // humidity
      byte h = hum;
      if (h > 99) h = 99;
      for (byte i=0; i<2; i++)
      {
        addNibble(h % 10);
        h /= 10;
      }

      // filler
      addNibble(0);

      // checksum (least significant nibble first)
      byte c = checksum;
      addNibble(c & 0xF);
      addNibble(c >> 4);

      // postamble (adjust message to byte size)
      if (nibbles % 2 == 0)
      {
        addByte(0xFF);
      }
      else
      {
        addNibble(0xF);
      }
    }

    return (nibbles + 1) / 2; // [bytes]
  }

  byte* getMessage()
  {
    return message;
  }

private:
  void addNibble(byte nibble)
  {
    byte index = nibbles / 2;
    if (index < MAX_MESSAGE_SIZE)
    {
      byte n = nibble & 0xF;
      checksum += n;

      if (invertBits)
      {
        n = ~n & 0xF;
      }

      bool even = nibbles % 2 == 0;
      if (flipOutputNibbles)
      {
        // 1st nibble to higher byte (flipped)
        if (even)
        {
          message[index] = n << 4;
        }
        else
        {
          message[index] |= n;
        }
      }
      else
      {
        // 1st nibble to lower byte (default)
        if (even)
        {
          message[index] = n;
        }
        else
        {
          message[index] |= n << 4;
        }
      }
      nibbles++;
    }
  }

  void addByte(byte b)
  {
    if (flipInputNibbles)
    {
      // higher nibble first (flipped)
      addNibble(b >> 4);
      addNibble(b & 0xF);
    }
    else
    {
      // lower nibble first (default)
      addNibble(b & 0xF);
      addNibble(b >> 4);
    }
  }

private:
  byte message[MAX_MESSAGE_SIZE];
  byte nibbles = 0;
  byte checksum;
  bool invertBits = false;
  bool flipInputNibbles = true;
  bool flipOutputNibbles = false;
};
