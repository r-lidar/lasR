#include "utils.h"
#include "constants.h"

#include <math.h> // sqrt

double orient2d(const Vec2& pa, const Vec2& pb, const Vec2& pc)
{
  double det = (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x);
  if (std::fabs(det) < IN_TRIANGLE_EPS) det = 0.0;
  return det;
}

double crossa(const Vec2& a, const Vec2& b)
{
  return a.x*b.y-a.y*b.x;
}

double det(double a, double b, double c, double d, double e, double f, double g, double h, double i)
{
  return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
}

double inCircle(Vec2& a, Vec2& b, Vec2& c, Vec2& d)
{
  // Assume pa, pb, and pc are in counterclockwise order
  /*if (orient2d(a, b, c) <= 0) {
    throw std::invalid_argument("Points pa, pb, and pc must be in counterclockwise order.");
  }*/

  // Compute the determinant of the 4x4 matrix
  double ax = a.x - d.x;
  double ay = a.y - d.y;
  double bx = b.x - d.x;
  double by = b.y - d.y;
  double cx = c.x - d.x;
  double cy = c.y - d.y;

  double det = (ax * ax + ay * ay) * (bx * cy - by * cx) -
    (bx * bx + by * by) * (ax * cy - ay * cx) +
    (cx * cx + cy * cy) * (ax * by - ay * bx);

  if (std::fabs(det) < IN_CIRCLE_EPS) det = 0.0; // Cocircular

  return det;

  // return det( a.x-d.x,a.y-d.y,(a.x-d.x)*(a.x-d.x)+(a.y-d.y)*(a.y-d.y),
  //             b.x-d.x,b.y-d.y,(b.x-d.x)*(b.x-d.x)+(b.y-d.y)*(b.y-d.y),
  //             c.x-d.x,c.y-d.y,(c.x-d.x)*(c.x-d.x)+(c.y-d.y)*(c.y-d.y));
}

//gives the square of the length of a const Vec2&
double sqrtLength(const Vec2& v)
{
  return v.x*v.x + v.y*v.y;
}

double dot(const Vec2& a, const Vec2& b)
{
  return a.x*b.x + a.y*b.y;
}

bool pointInSegment(const Vec2& p, const Vec2& p1, const Vec2& p2)
{
  if(p1 == p2) return false;
  if(p.x < std::min(p1.x,p2.x) + IN_TRIANGLE_EPS) return false;
  if(p.x > std::max(p1.x,p2.x) + IN_TRIANGLE_EPS) return false;
  if(p.y < std::min(p1.y,p2.y) + IN_TRIANGLE_EPS) return false;
  if(p.y > std::max(p1.y,p2.y) + IN_TRIANGLE_EPS) return false;

  Vec2 a = p1-p2;
  Vec2 n = Vec2(-a.y,a.x);

  return abs(dot(p-p1,n)) < IN_TRIANGLE_EPS;
}

double dist2(const Vec2& a,const Vec2& b)
{
  return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}

double mod(const Vec2& a)
{
  return sqrt(a.x*a.x + a.y*a.y);
}


/*bool isLeft(const Vec2& a, const Vec2& b){
 return crossa(a,b) >= IN_TRIANGLE_EPS;
}

 bool isRight(const Vec2& a, const Vec2& b){
 return crossa(a,b) < IN_TRIANGLE_EPS;
 }

 bool mightBeLeft(const Vec2& a, const Vec2& b){
 return crossa(a,b) >= -IN_TRIANGLE_EPS;
 }*/
