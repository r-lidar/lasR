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


#include "CSF.h"
#include <Rcpp.h>

using namespace Rcpp;

// [[Rcpp::export]]
IntegerVector R_CSF(DataFrame data, bool sloop_smooth, double class_threshold, double cloth_resolution, int rigidness, int interations, double time_step)
{
  CharacterVector names = data.attr("names");
  std::string x = as<std::string>(names[0]);
  std::string y = as<std::string>(names[1]);
  std::string z = as<std::string>(names[2]);

  NumericVector X = data[x];
  NumericVector Y = data[y];
  NumericVector Z = data[z];

  CSF csf;
  csf.params.bSloopSmooth = sloop_smooth;
  csf.params.class_threshold = class_threshold;
  csf.params.cloth_resolution = cloth_resolution;
  csf.params.interations = interations;
  csf.params.rigidness = rigidness;
  csf.params.time_step = time_step;

  std::vector<csf::Point> points(X.size());
  for (int i = 0 ; i < X.size() ; i++)
  {
    csf::Point p;
    p.x = X[i];
    p.y = Y[i];
    p.z = Z[i];
    points[i] = p;
  }

  csf.setPointCloud(points);

  std::vector<int> ground, nonground;
  csf.do_filtering(ground, nonground);

  IntegerVector Rground(wrap(ground));
  return Rground + 1;
}