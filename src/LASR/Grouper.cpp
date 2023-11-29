#include "Grouper.h"

#include <algorithm>

inline bool sort_range(const Interval& a, const Interval& b) { return a.start < b.start; }

Grouper::Grouper()
{
  npoints = 0;
}

bool Grouper::insert(int key)
{
  std::vector<Interval>& ranges = map[key];

  if (ranges.size() == 0)
  {
    ranges.push_back({npoints, npoints});
  }
  else
  {
    Interval& interval = ranges.back();
    if (interval.end == npoints - 1)
    {
      interval.end = npoints;
    }
    else
    {
      ranges.push_back({npoints, npoints});
    }
  }

  npoints++;
  return true;
}

int Grouper::largest_group_size()
{
  int max = 0;
  for (const auto& pair : map)
  {
    int sum = 0;
    for (const auto& interval : pair.second) sum += (interval.end - interval.start) + 1;
    if (sum >= max) max = sum;
  }

  return max;
}

void Grouper::merge_intervals(std::vector<Interval>& x)
{
  if (x.size() < 2) return;
  std::sort(x.begin(), x.end(), sort_range);
  std::vector<Interval> ans;
  ans.reserve(x.size()/2);
  Interval prev = x[0];

  for (size_t i = 1 ; i < x.size() ; i++)
  {
    Interval current = x[i];
    if ((current.start-prev.end) <= 1)
    {
      prev.end = current.end;
    }
    else
    {
      ans.push_back(prev);
      prev = current;
    }
  }

  ans.push_back(prev);
  x.swap(ans);
}

void Grouper::clear()
{
  map.clear();
  npoints = 0;
}