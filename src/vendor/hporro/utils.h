#include <glm/gtc/type_ptr.hpp>

//HELPER FUNCTIONS
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

template<class T>
T amin(T a, T b){
    if(a<b)return a;
    return b;
}

template<class T>
T amax(T a, T b){
    if(a>b)return a;
    return b;
}

double det(double a, double b, double c, double d, double e, double f, double g, double h, double i){
    return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
}

double inCircle(Vec2 a, Vec2 b, Vec2 c, Vec2 d){
    return incircle(glm::value_ptr(a),glm::value_ptr(b),glm::value_ptr(c),glm::value_ptr(d));
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

Vec2 operator/(Vec2 v, float a){
    return Vec2(v.x/a,v.y/a);
}

Vec2 operator*(Vec2 v, float a){
    return Vec2(v.x*a,v.y*a);
}

double dist2(Vec2 a,Vec2 b){
    return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}

inline double mod(Vec2 a){
    return sqrt(a.x*a.x + a.y*a.y);
}

template<class T>
inline T pow2(T a){
    return a*a;
}

template<class T>
inline T pow3(T a){
    return a*a*a;
}