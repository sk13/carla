// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <sstream>
#include <vector>

#pragma pack(push, 1)
struct CarlaRecorderPosition
{
  uint32_t DatabaseId;
  FVector Location;
  FVector Rotation;

  void Read(std::istream &InFile);

  void Write(std::ostream &OutFile);

};
#pragma pack(pop)

class CarlaRecorderPositions
{
public:

  void Add(const CarlaRecorderPosition &InObj);

  void Clear(void);

  void Write(std::ostream &OutFile);

  void Read(std::istream &InFile);

  const std::vector<CarlaRecorderPosition>& GetPositions();

private:

  std::vector<CarlaRecorderPosition> Positions;
};
