#ifndef VEC2_H
#define VEC2_H

#include <stdexcept>

struct Vec2
{
  double x;
  double y;

  Vec2() : x(0), y(0) {}
  Vec2(double x, double y) : x(x), y(y) {}
  bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
  Vec2 operator-(const Vec2& other) const { return Vec2{x - other.x, y - other.y}; }
  Vec2 operator+(const Vec2& other) const { return Vec2{x + other.x, y + other.y}; }
  Vec2 operator/(float a) const { return Vec2{x / a, y / a}; }
  Vec2 operator*(float a) const { return Vec2{x * a, y * a}; }
  Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }

  double& operator[](std::size_t index)
  {
    if (index == 0)
      return x;
    else if (index == 1)
      return y;
    else
      throw std::out_of_range("Index out of range");
  }

  const double& operator[](std::size_t index) const
  {
    if (index == 0)
      return x;
    else if (index == 1)
      return y;
    else
      throw std::out_of_range("Index out of range");
  }
};

#endif