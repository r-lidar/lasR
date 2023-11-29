#ifndef SHAPES_H
#define SHAPES_H

#include "macros.h"
#include <vector>
#include <cstdlib>

struct PointXY
{
  double x;
  double y;

  PointXY();
  PointXY(double x, double y);
  size_t hash() const;
  bool operator==(const PointXY& other) const;
  bool operator<(const PointXY& other) const;
};

struct PointXYZ : public PointXY
{
  double z;

  PointXYZ();
  PointXYZ(double x, double y);
  PointXYZ(double x, double y, double z);
  bool operator<(const PointXYZ& other) const;
  bool operator==(const PointXYZ& other) const;
};

struct Pixel
{
  int i, j, val;
  Pixel() : i(0), j(0), val(0) {}
  Pixel(int i, int j, int val) : i(i), j(j), val(val) {}
};

class LASpoint;

struct PointLAS : public PointXYZ
{
  PointLAS();
  PointLAS(const LASpoint* const p);
  void copy(const LASpoint* const p);
  unsigned int FID;
  unsigned short intensity;
  unsigned char return_number;
  unsigned char number_of_returns;
  bool scan_direction_flag;
  bool edge_of_flight_line;
  unsigned char classification;
  bool synthetic_flag;
  bool keypoint_flag;
  bool withheld_flag;
  bool overlap_flag;
  float scan_angle;
  char user_data;
  unsigned short point_source_ID;
  double gps_time;
  unsigned short R;
  unsigned short G;
  unsigned short B;
  unsigned short NIR;
};

enum ShapeType { UNKNOWN, RECTANGLE, CIRCLE, TRIANGLE };

struct Shape
{
  virtual ~Shape() = 0;
  virtual double xmin() const = 0;
  virtual double ymin() const = 0;
  virtual double xmax() const = 0;
  virtual double ymax() const = 0;
  virtual bool contains(double x, double y) const = 0;
  virtual PointXYZ centroid() const = 0;
  virtual ShapeType type() const { return ShapeType::UNKNOWN; };
};

struct Rectangle : public Shape
{
  double minx;
  double miny;
  double maxx;
  double maxy;

  Rectangle(double xmin, double ymin, double xmax, double ymax) : minx(xmin), miny(ymin), maxx(xmax), maxy(ymax) { };
  ~Rectangle() {};
  double xmin() const override { return minx; };
  double ymin() const override { return miny; };
  double xmax() const override { return maxx; };
  double ymax() const override { return maxy; };
  double width() const { return maxx-minx; };
  double height() const { return maxy-miny; };
  bool contains(double x, double y) const override { return x >= minx && x <= maxx && y >= miny && y <= maxy; };
  ShapeType type() const override { return ShapeType::RECTANGLE; };
  PointXYZ centroid() const override { return PointXYZ((maxx+minx)/2, (maxy+miny)/2, 0); };
};

struct Circle : public Shape
{
  PointXY center;
  double radius;

  Circle(double x, double y, double radius) { this->center.x = x ; this->center.y = y ; this->radius = radius; };
  Circle(PointXY& center, double radius) { this->center.x = center.x ; this->center.y = center.y ; this->radius = radius; };
  double xmin() const override { return center.x - radius; };
  double ymin() const override { return center.y - radius; };
  double xmax() const override { return center.x + radius; };
  double ymax() const override { return center.y + radius; };
  bool contains(double x, double y) const override { return (center.x - x)*(center.x - x)+(center.y - y)*(center.y - y) < radius*radius; };
  ShapeType type() const override { return ShapeType::CIRCLE; };
  PointXYZ centroid() const override{ return PointXYZ(center.x, center.y, 0); };
};

class TriangleXYZ : public Shape
{
public:
  PointXYZ A;
  PointXYZ B;
  PointXYZ C;

  TriangleXYZ() { }
  TriangleXYZ(const PointXYZ& A, const PointXYZ& B, const PointXYZ& C);
  void make_clock_wise();
  void make_counter_clock_wise();
  PointXYZ centroid() const override;
  bool contains(double x, double y) const override;
  bool contains(const PointXY& p) const;
  double square_max_edge_size() const;
  void linear_interpolation(PointXYZ& p) const;
  inline double xmin() const override { return MIN3(A.x, B.x, C.x); }
  inline double xmax() const override { return MAX3(A.x, B.x, C.x); }
  inline double ymin() const override { return MIN3(A.y, B.y, C.y); }
  inline double ymax() const override { return MAX3(A.y, B.y, C.y); }
  ShapeType type() const override { return ShapeType::TRIANGLE; };

private:
  double square_distance_point_to_segment(const PointXY& p1, const PointXY& p2, const PointXY& p) const;
  int orientation() const;

  enum orientation {COLINEAR, CLOCKWISE, COUNTERCLOCKWISE};
};

struct Edge
{
  PointXY A;
  PointXY B;

  Edge(const PointXY& A, const PointXY& B) : A(A), B(B) { }
  Edge(double x1, double y1, double x2, double y2);
  bool operator<(const Edge& other) const;
  bool operator==(const Edge& other) const;
  std::size_t hash() const;
  inline double xmin() const { return MIN(A.x, B.x); }
  inline double xmax() const { return MAX(A.x, B.x); }
  inline double ymin() const { return MIN(A.y, B.y); }
  inline double ymax() const { return MAX(A.y, B.y); }

private:
  static void hash_combine(std::size_t& seed, std::size_t hash) { seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2); }
};

class PolygonXY
{
public:
  std::vector<PointXY> coordinates;

  PolygonXY() { }
  PolygonXY(const std::vector<PointXY>& coords);
  void push_back(const PointXY& p);
  bool is_closed();
  void close();
  bool is_clockwise();

private:
  double signed_area();
};


namespace std
{
  template<> struct hash<Edge>
  {
    std::size_t operator()(const Edge& obj) const
    {
      return obj.hash();
    }
  };

  template<> struct hash<PointXY>
  {
    std::size_t operator()(const PointXY& obj) const
    {
      return obj.hash();
    }
  };
}

#endif

