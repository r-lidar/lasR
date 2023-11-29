#ifndef INTERVAL_H
#define INTERVAL_H

struct Interval
{
  Interval() : start(0), end(0) {};
  Interval(int start, int end) : start(start), end(end) {};
  int start;
  int end;
};

#endif