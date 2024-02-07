#ifndef GROUPER_H
#define GROUPER_H

#include "Interval.h"

#include <vector>
#include <unordered_map>

class Grouper
{
public:
  Grouper();
  bool insert(int key);
  bool insert(const std::vector<int>& keys);
  //void merge_intervals(std::vector<Interval>& x);
  void clear();
  int largest_group_size();

public:
  int npoints;
  std::unordered_map<int, std::vector<Interval>> map;
};

#endif
