#include "Shape.h"

#include <utility>    // std::swap
#include <functional> // std::hash
#include <cmath>

/* ====================
 * POINT
 * ====================*/

PointXY::PointXY() : x(0), y(0) {}
PointXY::PointXY(double x, double y) : x(x), y(y) {}

size_t PointXY::hash() const
{
  // Create individual hashes for x and y
  size_t hashX = std::hash<double>{}(x);
  size_t hashY = std::hash<double>{}(y);

  // Combine the individual hashes using a hash combiner
  size_t seed = 0;
  seed ^= hashX + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= hashY + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

/*bool PointXY::operator<(const PointXY& other) const
{
  if (x != other.x) return x < other.x;
  else return y < other.y;
}


bool PointXYZ::operator<(const PointXYZ& other) const
{
  if (x != other.x) return x < other.x;
  else if (y != other.y) return y < other.y;
  else return z < other.z;
}*/

bool PointXY::operator==(const PointXY& other) const
{
  return (x == other.x) && (y == other.y);
}

PointXYZ::PointXYZ() : PointXY(), z(0) {}
PointXYZ::PointXYZ(double x, double y) : PointXY(x,y), z(0) {}
PointXYZ::PointXYZ(double x, double y, double z) : PointXY(x,y), z(z) {}

double PointXYZ::distance(const PointXYZ& p) const
{
  return std::sqrt(pow(x - p.x, 2) + pow(y - p.y, 2) + pow(z - p.z, 2));
}

PointXYZ PointXYZ::operator-(const PointXYZ &other) const
{
  return PointXYZ(x - other.x, y - other.y, z - other.z);
}

PointXYZ PointXYZ::operator*(double scalar) const
{
  return PointXYZ(x * scalar, y * scalar, z * scalar);
}

double PointXYZ::dot(const PointXYZ &other) const
{
  return x * other.x + y * other.y + z * other.z;
}

PointXYZ PointXYZ::operator+(const PointXYZ &other) const
{
  return PointXYZ(x + other.x, y + other.y, z + other.z);
}

bool PointXYZ::operator==(const PointXYZ& other) const
{
  return (x == other.x) && (y == other.y) && (z == other.z);
}

/* ====================
 * SHAPE
 * ====================*/

Shape::~Shape() {}
Shape3D::~Shape3D() {}

/* ====================
 * TRIANGLE
 * ====================*/

TriangleXYZ::TriangleXYZ(const PointXYZ& A, const PointXYZ& B, const PointXYZ& C)
{
  this->A = A;
  this->B = B;
  this->C = C;
}

void TriangleXYZ::make_clock_wise()
{
  if (orientation() == COUNTERCLOCKWISE) std::swap(this->B, this->C);
}

void TriangleXYZ::make_counter_clock_wise()
{
  if (orientation() == CLOCKWISE) std::swap(this->B, this->C);
}

PointXYZ TriangleXYZ::normal() const
{
  // Compute edges
  PointXYZ edge1(B.x - A.x, B.y - A.y, B.z - A.z);
  PointXYZ edge2(C.x - A.x, C.y - A.y, C.z - A.z);

  // Compute cross product
  PointXYZ n;
  n.x = edge1.y * edge2.z - edge1.z * edge2.y;
  n.y = edge1.z * edge2.x - edge1.x * edge2.z;
  n.z = edge1.x * edge2.y - edge1.y * edge2.x;

  // Normalize the vector
  double length = sqrt(n.dot(n));
  n.x /= length;
  n.y /= length;
  n.z /= length;

  // Ensure the z-component is oriented upward
  /*if (n.z < 0)
  {
    n.x = -n.x;
    n.y = -n.y;
    n.z = -n.z;
  }*/

  return n;
}

PointXYZ TriangleXYZ::centroid() const
{
  PointXYZ centroid;
  centroid.x = (A.x + B.x + C.x)/3;
  centroid.y = (A.y + B.y + C.y)/3;
  centroid.z = (A.z + B.z + C.z)/3;
  return centroid;
}

bool TriangleXYZ::contains(const PointXY& P) const
{
  if (P.x < xmin() - EPSILON || P.x > xmax() + EPSILON || P.y < ymin() - EPSILON || P.y > ymax() + EPSILON)
    return false;

  // move to (0,0) to gain arithmetic precision
  double x_offset = xmin();
  double y_offset = ymin();
  PointXY a(A.x - x_offset, A.y - y_offset);
  PointXY b(B.x - x_offset, B.y - y_offset);
  PointXY c(C.x - x_offset, C.y - y_offset);
  PointXY p(P.x - x_offset, P.y - y_offset);

  double denominator = (a.x*(b.y - c.y) + a.y*(c.x - b.x) + b.x*c.y - b.y*c.x);
  double t1 = (p.x*(c.y - a.y) + p.y*(a.x - c.x) - a.x*c.y + a.y*c.x) / denominator;
  double t2 = (p.x*(b.y - a.y) + p.y*(a.x - b.x) - a.x*b.y + a.y*b.x) / -denominator;
  double s = t1 + t2;

  if (0 <= t1 && t1 <= 1 && 0 <= t2 && t2 <= 1 && s <= 1)
    return true;

  // see http://totologic.blogspot.com/2014/01/accurate-point-in-triangle-test.html
  if (square_distance_point_to_segment(a, b, p) <= EPSILON) return true;
  if (square_distance_point_to_segment(b, c, p) <= EPSILON) return true;
  if (square_distance_point_to_segment(c, a, p) <= EPSILON) return true;

  return false;
}

bool TriangleXYZ::contains(double x, double y) const
{
  PointXY p(x, y);
  return contains(p);
}

double TriangleXYZ::square_max_edge_size() const
{
  // ABC is resented by vector u,v and w
  PointXYZ u(A.x - B.x, A.y - B.y);
  PointXYZ v(A.x - C.x, A.y - C.y);
  PointXYZ w(B.x - C.x, B.y - C.y);

  // Compute the AB, AC, BC edge comparable length
  double edge_AB = u.x * u.x + u.y * u.y;
  double edge_AC = v.x * v.x + v.y * v.y;
  double edge_BC = w.x * w.x + w.y * w.y;
  return MAX3(edge_AB, edge_AC, edge_BC);
}

void TriangleXYZ::linear_interpolation(PointXYZ& p) const
{
  double dx1 = p.x - A.x;
  double dy1 = p.y - A.y;
  double dx2 = B.x - A.x;
  double dy2 = B.y - A.y;
  double dx3 = C.x - A.x;
  double dy3 = C.y - A.y;
  p.z = A.z + ((dy1 * dx3 - dx1 * dy3) * (B.z - A.z) + (dx1 * dy2 - dy1 * dx2) * (C.z - A.z)) / (dx3 * dy2 - dx2 * dy3);
}

double TriangleXYZ::square_distance_point_to_segment(const PointXY& p1, const PointXY& p2, const PointXY& p) const
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

int TriangleXYZ::orientation() const
{
  double val = (B.x - A.x) * (C.y - B.y) - (B.y - A.y) * (C.x - B.x);
  if (val == 0) return COLINEAR;
  return (val > 0) ? CLOCKWISE : COUNTERCLOCKWISE;
}

/* ====================
 * Edge
 * ====================*/

/*Edge::Edge(double x1, double y1, double x2, double y2)
{
  A.x = x1;
  B.x = x2;
  A.y = y1;
  B.y = y2;
}*/

/*bool Edge::operator<(const Edge& other) const
{
  if (xmin() != other.xmin()) return xmin() < other.xmin();
  if (ymin() != other.ymin()) return ymin() < other.ymin();
  if (xmax() != other.xmax()) return xmax() < other.xmax();
  return ymax() < other.ymax();
}*/

bool Edge::operator==(const Edge& other) const
{
  return (A == other.A && B == other.B) || (A == other.B && B == other.A);
}

std::size_t Edge::hash() const
{
  size_t hashX = std::hash<double>{}(xmin());
  size_t hashY = std::hash<double>{}(ymax());

  size_t seed = 0;
  seed ^= hashX + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  seed ^= hashY + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}

/* ====================
 * POLYGON
 * ====================*/

PolygonXY::PolygonXY(const std::vector<PointXY>& coords)
{
  coordinates = coords;
}

void PolygonXY::push_back(const PointXY& p)
{
  coordinates.push_back(p);
}

bool PolygonXY::is_closed() const
{
  if (coordinates.size() == 0) return false;
  return &coordinates.front() == &coordinates.back();
}

void PolygonXY::close()
{
  if (!is_closed()) coordinates.push_back(coordinates.front());
}

bool PolygonXY::is_clockwise() const
{
  return signed_area() > 0;
}

double PolygonXY::signed_area() const
{
  int n = coordinates.size();
  double signed_area = 0.0;

  for (int i = 0; i < n ; ++i)
  {
    const PointXY& current = coordinates[i];
    const PointXY& next = coordinates[(i + 1) % n];
    signed_area += (next.x - current.x) * (next.y + current.y);
  }

  return signed_area;
}
