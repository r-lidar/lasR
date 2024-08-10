#include "utils.h"
#include "constants.h"
#include "pred3d.h"

#include <math.h> // sqrt

double crossa(Vec2 a, Vec2 b){
  return a.x*b.y-a.y*b.x;
}

bool isLeft(Vec2 a, Vec2 b){
  return crossa(a,b) >= IN_TRIANGLE_EPS;
}

bool isRight(Vec2 a, Vec2 b){
  return crossa(a,b) < IN_TRIANGLE_EPS;
}

bool mightBeLeft(Vec2 a, Vec2 b){
  return crossa(a,b) >= -IN_TRIANGLE_EPS;
}

double det(double a, double b, double c, double d, double e, double f, double g, double h, double i){
  return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
}

double inCircle(Vec2 a, Vec2 b, Vec2 c, Vec2 d){
  return incircle(&(a.x),&(b.x),&(c.x),&(d.x));
  // return det( a.x-d.x,a.y-d.y,(a.x-d.x)*(a.x-d.x)+(a.y-d.y)*(a.y-d.y),
  //             b.x-d.x,b.y-d.y,(b.x-d.x)*(b.x-d.x)+(b.y-d.y)*(b.y-d.y),
  //             c.x-d.x,c.y-d.y,(c.x-d.x)*(c.x-d.x)+(c.y-d.y)*(c.y-d.y));
}

//gives the square of the length of a Vec2
double sqrtLength(Vec2 v){
  return v.x*v.x + v.y*v.y;
}

double dot(Vec2 a, Vec2 b){
  return a.x*b.x + a.y*b.y;
}

bool pointInSegment(Vec2 p, Vec2 p1, Vec2 p2){
  if(p1==p2) return false;
  if(p.x < amin(p1.x,p2.x) + IN_TRIANGLE_EPS) return false;
  if(p.x > amax(p1.x,p2.x) + IN_TRIANGLE_EPS) return false;
  if(p.y < amin(p1.y,p2.y) + IN_TRIANGLE_EPS) return false;
  if(p.y > amax(p1.y,p2.y) + IN_TRIANGLE_EPS) return false;

  Vec2 a = p1-p2;
  Vec2 n = Vec2(-a.y,a.x);

  return abs(dot(p-p1,n)) < IN_TRIANGLE_EPS;
}

double dist2(Vec2 a,Vec2 b){
  return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}

double mod(Vec2 a){
  return sqrt(a.x*a.x + a.y*a.y);
}