#include "utils.h"
#include "constants.h"
#include "pred3d.h"

#include <math.h> // sqrt

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
  return incircle(&(a.x),&(b.x),&(c.x),&(d.x));
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
  if(p1==p2) return false;
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
