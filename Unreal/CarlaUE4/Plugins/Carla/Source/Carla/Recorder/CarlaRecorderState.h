// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <sstream>

#pragma pack(push, 1)

struct CarlaRecorderStateTrafficLight
{
  uint32_t DatabaseId;
  bool IsFrozen;
  float ElapsedTime;
  char State;

  void Read(std::istream &InFile);

  void Write(std::ostream &OutFile);

};

#pragma pack(pop)

class CarlaRecorderStates
{

public:

  void Add(const CarlaRecorderStateTrafficLight &State);

  void Clear(void);

  void Write(std::ostream &OutFile);

  void Read(std::istream &InFile);

  const std::vector<CarlaRecorderStateTrafficLight>& GetStates();

private:

  std::vector<CarlaRecorderStateTrafficLight> StatesTrafficLights;
};
