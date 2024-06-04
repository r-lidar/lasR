// ======================================================================================
// Copyright 2017 State Key Laboratory of Remote Sensing Science,
// Institute of Remote Sensing Science and Engineering, Beijing Normal University

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ======================================================================================

#include "point_cloud.h"


void csf::PointCloud::computeBoundingBox(Point& bbMin, Point& bbMax)
{
  if (empty())
  {
    bbMin = bbMax = Point();
    return;
  }

  bbMin = bbMax = at(0);

  for (std::size_t i = 1; i < size(); i++)
  { // zwm
    const csf::Point& P = at(i);

    if (P.x < bbMin.x) bbMin.x = P.x;
    else if (P.x > bbMax.x) bbMax.x = P.x;

    if (P.y < bbMin.y) bbMin.y = P.y;
    else if (P.y > bbMax.y) bbMax.y = P.y;

    if (P.z < bbMin.z) bbMin.z = P.z;
    else if (P.z > bbMax.z) bbMax.z = P.z;
  }
}
