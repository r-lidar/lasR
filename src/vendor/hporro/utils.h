#ifndef UTILS_H
#define UTILS_H

#include "geometry.h"

double orient2d(const Vec2& pa, const Vec2& pb, const Vec2& pc);
double crossa(const Vec2& a, const Vec2& b);
double det(double a, double b, double c, double d, double e, double f, double g, double h, double i);
double inCircle(Vec2& a, Vec2& b, Vec2& c, Vec2& d);
double sqrtLength(const Vec2& v);
double dot(const Vec2& a, const Vec2& b);
bool pointInSegment(const Vec2& p, const Vec2& p1, const Vec2& p2);
double dist2(const Vec2& a,const Vec2& b);
double mod(const Vec2& a);

/*bool isLeft(const Vec2& a, const Vec2& b);
 bool isRight(const Vec2& a, const Vec2& b);
 bool mightBeLeft(const Vec2& a, const Vec2& b);*/

#endif