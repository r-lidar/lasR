#ifndef UTILS_H
#define UTILS_H

#include "Vec2.h"

//HELPER FUNCTIONS
double crossa(Vec2 a, Vec2 b);
bool isLeft(Vec2 a, Vec2 b);
bool isRight(Vec2 a, Vec2 b);
bool mightBeLeft(Vec2 a, Vec2 b);

template<class T> T amin(T a, T b)
{
    if(a<b)return a;
    return b;
}

template<class T> T amax(T a, T b)
{
    if(a>b)return a;
    return b;
}

double det(double a, double b, double c, double d, double e, double f, double g, double h, double i);
double inCircle(Vec2 a, Vec2 b, Vec2 c, Vec2 d);
double sqrtLength(Vec2 v);
double dot(Vec2 a, Vec2 b);
bool pointInSegment(Vec2 p, Vec2 p1, Vec2 p2);
double dist2(Vec2 a,Vec2 b);
double mod(Vec2 a);

template<class T> inline T pow2(T a)
{
    return a*a;
}

template<class T> inline T pow3(T a)
{
    return a*a*a;
}

#endif