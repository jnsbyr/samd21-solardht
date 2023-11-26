/*****************************************************************************
 *
 * Calculate Moving Average of Measurement Data Stream
 *
 * file:     Measurement.h
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

#include <vector>

class Measurement
{
public:
  Measurement() = default;

public:
  void setMaxSamples(size_t maxSamples)
  {
    this->maxSamples = maxSamples;
  }

  void add(float sample)
  {
    samples.push_back(sample);
    while (samples.size() > maxSamples)
    {
      samples.erase(samples.begin());
    }
  }

  float getAverage()
  {
    size_t count = samples.size();
    if (count)
    {
      float sum = 0;
      for (auto s: samples)
      {
        sum += s;
      }
      return sum/count;
    }
    else
    {
      return 0;
    }
  }

private:
  size_t maxSamples = 4;
  std::vector<float> samples;
};
