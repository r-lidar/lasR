#ifndef PTD_H
#define PTD_H

#include <vector>

#include "hporro/constants.h"
#include "hporro/delaunay.h"
#include "nanoflann/nanoflann.h"

namespace PTD
{

using Point = IncrementalDelaunay::Vec2;
using Grid = IncrementalDelaunay::Grid;

struct PTDParameters
{
  double seed_resolution_search = 10.0;
  double max_iteration_angle = 30.0;
  double max_iteration_distance = 2.0;
  double min_triangle_size = 0.25;
  double buffer_size = 30.0;
  int max_iter = 50;
  bool verbose = false;
};

class PTD
{
public:
  PTD(const PTDParameters& params);
  ~PTD();

  bool run(std::vector<Point>& points);
  std::vector<unsigned int> get_ground_fid() const;
  std::vector<unsigned int> get_spikes_fid() const;

private:
  void calculate_bounds(const std::vector<Point>& points);
  void sort_points_by_z(std::vector<Point>& points);
  void make_seeds(const std::vector<Point>& points);
  void make_buffer();
  void tin_buffer();
  void tin_seeds(const std::vector<Point>& points);
  void densify_tin(const std::vector<Point>& points);
  void detect_spikes();

private:
  PTDParameters params;

  double x_min, y_min, x_max, y_max, z_default;

  std::vector<Point> seeds;
  std::vector<Point> vbuff;
  std::vector<bool> inserted;
  std::vector<bool> is_spike;

  IncrementalDelaunay::Grid gd;
  IncrementalDelaunay::Triangulation* d;
  std::string last_error;
};

struct Vec3
{
  double x, y, z;

  // Constructors
  Vec3() : x(0), y(0), z(0) {}
  Vec3(double x, double y) : x(x), y(y), z(0) {}
  Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
  Vec3(const Point& v) : x(v.x), y(v.y), z(v.z) {}
  Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
  Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
  Vec3 operator*(double scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
  double dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
  Vec3 cross(const Vec3& other) const {return Vec3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x); }
  double length() const { return std::sqrt(x * x + y * y + z * z); }
  double distance(const Vec3& other) const { return (*this - other).length(); }
  Vec3 normalize() const
  {
    double len = length();
    if (len == 0) return *this;
    return Vec3(x / len, y / len, z / len);
  }
};

struct Triangle
{
  Vec3 A, B, C;

  Triangle(const Vec3& a, const Vec3& b, const Vec3& c) : A(a), B(b), C(c) {}
  Vec3 normal() const
  {
    Vec3 u = B - A;
    Vec3 v = C - A;
    return u.cross(v).normalize();
  }

  inline bool contains(const Vec3& P) const
  {
    double xmin = std::min(A.x, std::min(B.x, C.x));
    double xmax = std::max(A.x, std::max(B.x, C.x));
    double ymin = std::min(A.y, std::min(B.y, C.y));
    double ymax = std::max(A.y, std::max(B.y, C.y));

    if (P.x < xmin - IN_TRIANGLE_EPS || P.x > xmax + IN_TRIANGLE_EPS || P.y < ymin - IN_TRIANGLE_EPS || P.y > ymax + IN_TRIANGLE_EPS)
      return false;

    // move to (0,0) to gain arithmetic precision
    double x_offset = xmin;
    double y_offset = ymin;
    Point a(A.x - x_offset, A.y - y_offset);
    Point b(B.x - x_offset, B.y - y_offset);
    Point c(C.x - x_offset, C.y - y_offset);
    Point p(P.x - x_offset, P.y - y_offset);

    double denominator = (a.x*(b.y - c.y) + a.y*(c.x - b.x) + b.x*c.y - b.y*c.x);
    double t1 = (p.x*(c.y - a.y) + p.y*(a.x - c.x) - a.x*c.y + a.y*c.x) / denominator;
    double t2 = (p.x*(b.y - a.y) + p.y*(a.x - b.x) - a.x*b.y + a.y*b.x) / -denominator;
    double s = t1 + t2;

    if (0 <= t1 && t1 <= 1 && 0 <= t2 && t2 <= 1 && s <= 1)
      return true;

    // see http://totologic.blogspot.com/2014/01/accurate-point-in-triangle-test.html
    if (square_distance_point_to_segment(a, b, p) <= IN_TRIANGLE_EPS) return true;
    if (square_distance_point_to_segment(b, c, p) <= IN_TRIANGLE_EPS) return true;
    if (square_distance_point_to_segment(c, a, p) <= IN_TRIANGLE_EPS) return true;

    return false;
  }

  inline double square_distance_point_to_segment(const Point& p1, const Point& p2, const Point& p) const
  {
    double p1_p2_squareLength = (p2.x - p1.x)*(p2.x - p1.x) + (p2.y - p1.y)*(p2.y - p1.y);
    double dot_product = ((p.x - p1.x)*(p2.x - p1.x) + (p.y - p1.y)*(p2.y - p1.y)) / p1_p2_squareLength;

    if ( dot_product < 0 )
    {
      return (p.x - p1.x)*(p.x - p1.x) + (p.y - p1.y)*(p.y - p1.y);
    }
    else if ( dot_product <= 1 )
    {
      double p_p1_squareLength = (p1.x - p.x)*(p1.x - p.x) + (p1.y - p.y)*(p1.y - p.y);
      return p_p1_squareLength - dot_product * dot_product * p1_p2_squareLength;
    }
    else
    {
      return (p.x - p2.x)*(p.x - p2.x) + (p.y - p2.y)*(p.y - p2.y);
    }
  }
};


}

#endif // PTD_H