#ifndef COMMON_MODULE_POINT2D_H
#define COMMON_MODULE_POINT2D_H

#include <sstream>
#include <assert.h>
#include <string>
#include <iostream>

/**
 * \~Engish
 * @class Point2D
 * @brief The Point2D class models a point or a vector in a 2D plane.
 *
 * It representes a 2D point or vector, with the precision given by the template type T.
 * Specified types with T = int, float or double are already defined as Point2Di,
 * Point2Df and Point2Dd (Vec2).
 *
 * @author Alessandro Muntoni
 */
template <class T> class Point2D {
    public:
        Point2D(T x = 0.0, T y = 0.0);
        #ifdef COMMON_WITH_EIGEN
        Point2D(const Eigen::VectorXd &v);
        #endif

        /***
         * Const Member Functions
         **/
        const T& x() const;
        const T& y() const;
        double dist(const Point2D<T> &otherPoint) const;
        double dot(const Point2D<T> &otherVector) const;
        double perpendicularDot(const Point2D<T> &otherVector) const;
        double getLength() const;
        double getLengthSquared() const;
        Point2D<T> min(const Point2D<T> &otherPoint) const;
        Point2D<T> max(const Point2D<T> &otherPoint) const;

        const T& operator[](unsigned int i)                 const;
        const T& operator()(unsigned int i)                 const;
        bool operator == (const Point2D<T> &otherPoint)   const;
        bool operator != (const Point2D<T> &otherPoint)   const;
        bool operator < (const Point2D<T>& otherPoint)    const;
        bool operator > (const Point2D<T>& otherPoint)    const;
        bool operator <= (const Point2D<T>& otherPoint)   const;
        bool operator >= (const Point2D<T>& otherPoint)   const;
        Point2D<T> operator - ()                          const;
        Point2D<T> operator + (const T& scalar) const;
        Point2D<T> operator + (const Point2D<T>& otherPoint) const;
        Point2D<T> operator - (const Point2D<T>& otherPoint) const;
        Point2D<T> operator * (const T& scalar)      const;
        Point2D<T> operator * (const Point2D<T>& otherPoint) const;
        Point2D<T> operator / (const T& scalar )     const;
        Point2D<T> operator / (const Point2D<T>& otherPoint) const;

        T& x();
        T& y();
        void setXCoord(const T& x);
        void setYCoord(const T& y);
        void set(const T& x, const T& y);
        double normalize();
        void rotate(T matrix[][2], const Point2D<T>& centroid = Point2D<T>());
        Point2D<T> operator += (const Point2D<T>& otherPoint);
        Point2D<T> operator -= (const Point2D<T>& otherPoint);
        Point2D<T> operator *= (const T& scalar);
        Point2D<T> operator *= (const Point2D<T>& otherPoint);
        Point2D<T> operator /= (const T& scalar );
        Point2D<T> operator /= (const Point2D<T>& otherPoint);

    private:
        T xCoord, yCoord;
        void rot(T matrix[][2]);
};

/****************
* Other Methods *
*****************/

template <class T>
Point2D<T> operator * (const T& scalar, const Point2D<T>& point);

template <class T>
Point2D<T> normalOfSegment(const Point2D<T>& p1, const Point2D<T>& p2);

/******************
* Type definition *
******************/

typedef Point2D<double> Point2Dd;
typedef Point2D<float> Point2Df;
typedef Point2D<int> Point2Di;
typedef Point2D<double> Vec2;

#include "Andrea/Headers/point2d.h"
#include <cmath>

/**
 * @brief Point2D<T>::Point2D
 *
 * Constructor, initializes the coordinates of the point
 *
 * @param x
 * @param y
 */
template <class T>
inline Point2D<T>::Point2D(T x, T y): xCoord(x), yCoord(y) {
}

#ifdef COMMON_WITH_EIGEN
/**
 * @brief Point2D<T>::Point2D
 *
 * Constructor using the values of an EigenVector.
 *
 * @param v
 */
template <class T>
Point2D<T>::Point2D(const Eigen::VectorXd& v) : xCoord(v(0)), yCoord(v(1)){
}
#endif

/**
 * @brief Point2D<T>::x
 * @return a const reference of the x coordinate
 */
template <class T>
inline const T& Point2D<T>::x() const {
  return xCoord;
}

/**
 * @brief Point2D<T>::y
 * @return a const reference of the y coordinate
 */
template <class T>
inline const T& Point2D<T>::y() const {
  return yCoord;
}

/**
 * @brief Point2D<T>::dist
 *
 * @param[in] otherPoint: another point
 * @return the distance between this and otherPoint
 */
template <class T>
inline double Point2D<T>::dist(const Point2D<T> &otherPoint) const{
  return sqrt ( std::pow((xCoord - otherPoint.xCoord), 2) +
                std::pow((yCoord - otherPoint.yCoord), 2) );
}

/**
 * @brief Point2D<T>::dot
 * @param[in] otherVector: another vector
 * @return the dot product between this and otherVector
 */
template <class T>
inline double Point2D<T>::dot(const Point2D<T> &otherVector) const{
  return xCoord * otherVector.xCoord +
    yCoord * otherVector.yCoord;
}

/**
 * @brief Point2D<T>::perpendicularDot
 * @param[in] otherVector: another vector
 * @return the perpendicular dot product between this and otherVector
 */
template <class T>
inline double Point2D<T>::perpendicularDot(const Point2D<T>& otherVector) const {
  return xCoord * otherVector.yCoord -
    yCoord * otherVector.xCoord;
}

/**
 * @brief Point2D<T>::getLength
 * @return the length of this vector.
 */
template <class T>
inline double Point2D<T>::getLength() const{
  return std::sqrt( xCoord*xCoord + yCoord*yCoord );
}

/**
 * @brief Point2D<T>::getLengthSquared
 * @return the squared length of this vector.
 */
template <class T>
inline double Point2D<T>::getLengthSquared() const{
  return xCoord*xCoord + yCoord*yCoord;
}

/**
 * @brief Point2D<T>::min
 * @param[in] otherPoint: another point
 * @return the point composed of the minimum x coordinate and the minimum y coordinate between the two points.
 */
template <class T>
inline Point2D<T> Point2D<T>::min(const Point2D<T> &otherPoint) const{
  return Point2D<T>(std::min(x(), otherPoint.xCoord),
                    std::min(y(), otherPoint.yCoord));
}

/**
 * @brief Point2D<T>::max
 * @param otherPoint
 * @return the point composed of the maximum x coordinate and the maximum y coordinate between the two points.
 */
template <class T>
inline Point2D<T> Point2D<T>::max(const Point2D<T> &otherPoint) const{
  return Point2D<T>(std::max(x(), otherPoint.xCoord),
                    std::max(y(), otherPoint.yCoord));
}

/**
 * @brief Point2D<T>::operator []
 * @param i: can be 0 or 1, assert in other cases;
 * @return x coord if index is 0, y coord if index is 1
 */
template <class T>
const T& Point2D<T>::operator[](unsigned int i) const {
  assert(i < 2);
  switch (i){
  case 0: return xCoord;
  case 1: return yCoord;
  }
  return xCoord;
}

/**
 * @brief Point2D<T>::operator ()
 * @param i: can be 0 or 1, assert in other cases;
 * @return x coord if index is 0, y coord if index is 1
 */
template <class T>
const T&Point2D<T>::operator()(unsigned int i) const {
  assert(i < 2);
  switch (i){
  case 0: return xCoord;
  case 1: return yCoord;
  }
  return xCoord;
}

/**
 * @brief Point2D<T>::operator ==
 * @param otherPoint
 * @return true if other point is equal to this, false otherwise
 */
template <class T>
inline bool Point2D<T>::operator == (const Point2D<T>& otherPoint) const {
  if ( otherPoint.xCoord != xCoord )	return false;
  if ( otherPoint.yCoord != yCoord )	return false;
  return true;
}

/**
 * @brief Point2D<T>::operator !=
 * @param otherPoint
 * @return false id other point is equal to this, true otherwise
 */
template <class T>
inline bool Point2D<T>::operator != (const Point2D<T>& otherPoint) const {
  if ( otherPoint.xCoord != xCoord )	return true;
  if ( otherPoint.yCoord != yCoord )	return true;
  return false;
}

/**
 * @brief Point2D<T>::operator <
 * Lexicographical order of points: check first for x coord, and if they are equal, y.
 * @param otherPoint
 * @return true if this point is < than other point, false otherwise.
 */
template <class T>
inline bool Point2D<T>::operator < (const Point2D<T>& otherPoint) const {
  if (this->xCoord < otherPoint.xCoord) return true;
  if (this->xCoord > otherPoint.xCoord) return false;
  if (this->yCoord < otherPoint.yCoord) return true;
  return false;
}

/**
 * @brief Point2D<T>::operator >
 * Lexicographical order of points: check first for x coord, and if they are equal, y.
 * @param otherPoint
 * @return true if this point is > than other point, false otherwise.
 */
template <class T>
inline bool Point2D<T>::operator >(const Point2D<T>& otherPoint) const {
  if (this->xCoord < otherPoint.xCoord) return false;
  if (this->xCoord > otherPoint.xCoord) return true;
  if (this->yCoord > otherPoint.yCoord) return true;
  return false;
}

template <class T>
/**
 * @brief Point2D<T>::operator <=
 * Lexicographical order of points: check first for x coord, and if they are equal, y.
 * @param otherPoint
 * @return true if this point is <= than other point, false otherwise.
 */
inline bool Point2D<T>::operator <=(const Point2D<T>& otherPoint) const {
  return *this == otherPoint || *this < otherPoint;
}

/**
 * @brief Point2D<T>::operator >=
 * Lexicographical order of points: check first for x coord, and if they are equal, y.
 * @param otherPoint
 * @return true if this point is >= than other point, false otherwise.
 */
template <class T>
inline bool Point2D<T>::operator >=(const Point2D<T>& otherPoint) const {
  return *this == otherPoint || *this > otherPoint;
}

/**
 * @brief Point2D<T>::operator -
 * @return a Point2D with negated coordinates
 */
template <class T>
inline Point2D<T> Point2D<T>::operator - () const {
  return Point2D(-xCoord, -yCoord);
}

/**
 * @brief Point2D<T>::operator +
 * @param scalar
 * @return a Point2D with the coordinates sommed with the input scalar.
 */
template <class T>
Point2D<T> Point2D<T>::operator +(const T& scalar) const {
  return Point2D<T>(xCoord + scalar,
                    yCoord + scalar);
}

/**
 * @brief Point2D<T>::operator +
 * @param otherPoint
 * @return a Point2D which is the sum, coord by coord, of this point and other point.
 */
template <class T>
inline Point2D<T> Point2D<T>::operator + (const Point2D<T>& otherPoint) const {
  return Point2D(xCoord + otherPoint.xCoord,
                 yCoord + otherPoint.yCoord);
}

/**
 * @brief Point2D<T>::operator -
 * @param otherPoint
 * @return a Point2D which is the difference, coord by coord, of this point and other point.
 */
template <class T>
inline Point2D<T> Point2D<T>::operator - (const Point2D<T>& otherPoint) const {
  return Point2D(xCoord - otherPoint.xCoord,
                 yCoord - otherPoint.yCoord);
}

/**
 * @brief Point2D<T>::operator *
 * @param scalar
 * @return a Point2D which is the product of every coord with the scalar
 */
template <class T>
inline Point2D<T> Point2D<T>::operator * (const T& scalar) const {
  return Point2D(xCoord * scalar, yCoord * scalar);
}

/**
 * @brief Point2D<T>::operator *
 * @param otherPoint
 * @return a Point2D which is the product, coord by coord, of this point and other point.
 */
template <class T>
inline Point2D<T> Point2D<T>::operator * (const Point2D<T>& otherPoint) const {
  return Point2D(xCoord * otherPoint.xCoord, yCoord * otherPoint.yCoord);
}

/**
 * @brief Point2D<T>::operator /
 * @param scalar
 * @return a Point2D which is the division of every coord with the scalar
 */
template <class T>
inline Point2D<T> Point2D<T>::operator / (const T& scalar) const {
  return Point2D(xCoord / scalar, yCoord / scalar);
}

/**
 * @brief Point2D<T>::operator /
 * @param otherPoint
 * @return a Point2D which is the division, coord by coord, of this point and other point.
 */
template <class T>
inline Point2D<T> Point2D<T>::operator / (const Point2D<T>& otherPoint) const {
  return Point2D(xCoord / otherPoint.xCoord, yCoord / otherPoint.yCoord);
}

/**
 * @brief Point2D<T>::x
 * @return the reference of x coord
 */
template<class T>
T&Point2D<T>::x() {
  return xCoord;
}

/**
 * @brief Point2D<T>::y
 * @return the reference of y coord
 */
template<class T>
T&Point2D<T>::y() {
  return yCoord;
}

/**
 * @brief Point2D<T>::setXCoord
 * set the x coord
 * @param x
 */
template <class T>
inline void Point2D<T>::setXCoord(const T& x) {
  xCoord = x;
}

/**
 * @brief Point2D<T>::setYCoord
 * set the y coord
 * @param y
 */
template <class T>
inline void Point2D<T>::setYCoord(const T& y) {
  yCoord = y;
}

/**
 * @brief Point2D<T>::set
 * set the coordinates of the point
 * @param x
 * @param y
 */
template <class T>
inline void Point2D<T>::set(const T& x, const T& y) {
  xCoord = x;
  yCoord = y;
}

/**
 * @brief Point2D<T>::normalize
 * makes the vector of magniture 1
 * @return the previous magnitude of the vector
 */
template <class T>
inline double Point2D<T>::normalize() {
  double len = getLength();
  xCoord /= len;
  yCoord /= len;
  return len;
}

/**
 * @brief Point2D<T>::rotate
 * it rotates the point given the centroid and the rotation matrix
 *
 * @param matrix
 * @param centroid
 */
template <class T>
inline void Point2D<T>::rotate(T matrix[][2], const Point2D<T>& centroid){
  *this -= centroid;
  rot(matrix);
  *this += centroid;
}

/**
 * @brief Point2D<T>::operator +=
 * sums, coord by coord, this point to the other point
 *
 * @param otherPoint
 * @return
 */
template <class T>
inline Point2D<T> Point2D<T>::operator += (const Point2D<T>& otherPoint) {
  xCoord += otherPoint.xCoord;
  yCoord += otherPoint.yCoord;
  return *this;
}

/**
 * @brief Point2D<T>::operator -=
 * subtracts, coord by coord, this point to the other point
 *
 * @param otherPoint
 * @return
 */
template <class T>
inline Point2D<T> Point2D<T>::operator -= (const Point2D<T>& otherPoint) {
  xCoord -= otherPoint.xCoord;
  yCoord -= otherPoint.yCoord;
  return *this;
}

/**
 * @brief Point2D<T>::operator *=
 * multiplies every coord to the scalar
 *
 * @param scalar
 * @return
 */
template <class T>
inline Point2D<T> Point2D<T>::operator *= (const T& scalar) {
  xCoord *= scalar;
  yCoord *= scalar;
  return *this;
}

/**
 * @brief Point2D<T>::operator *=
 * multiplies, coord by coord, this point to the other point
 *
 * @param otherPoint
 * @return
 */
template <class T>
inline Point2D<T> Point2D<T>::operator *= (const Point2D<T>& otherPoint) {
  xCoord *= otherPoint.xCoord;
  yCoord *= otherPoint.yCoord;
  return *this;
}

/**
 * @brief Point2D<T>::operator /=
 * divides every coord to the scalar
 *
 * @param scalar
 * @return
 */
template <class T>
inline Point2D<T> Point2D<T>::operator /= (const T& scalar) {
  xCoord /= scalar;
  yCoord /= scalar;
  return *this;
}

/**
 * @brief Point2D<T>::operator /=
 * divides, coord by coord, this point to the other point
 *
 * @param otherPoint
 * @return
 */
template <class T>
inline Point2D<T> Point2D<T>::operator /= (const Point2D<T>& otherPoint) {
  xCoord /= otherPoint.xCoord;
  yCoord /= otherPoint.yCoord;
  return *this;
}

/**
 * @brief Point2D<T>::rot
 * @param matrix
 */
template <class T>
inline void Point2D<T>::rot(T matrix[][2]){
  Point2D<T> p;
  p.setXCoord(matrix[0][0]*xCoord + matrix[0][1]*yCoord);
  p.setYCoord(matrix[1][0]*xCoord + matrix[1][1]*yCoord);
  xCoord = p.x();
  yCoord = p.y();
}

/**
 * @brief operator *
 * multiplies a scalar with a point
 * @param scalar
 * @param point
 * @return the product
 */
template <class T>
inline Point2D<T> operator *(const T& scalar, const Point2D<T>& point) {
  return Point2D<T>(point.x() * scalar,
                    point.y() * scalar);
}

/**
 * @brief normalOfSegment
 * @param p1
 * @param p2
 * @return the normal of the segment defined by the two points p1 and p2
 */
template <class T>
inline Point2D<T> normalOfSegment(const Point2D<T>& p1, const Point2D<T>& p2) {
  double matrix[2][2] = {{0,-1},
  {1,0}};

  Point2D<T> p = p1 - p2;
  p.normalize();
  p.rotate(matrix);
  return p;
}

/**
 * @brief area
 * @param p0
 * @param p1
 * @param p2
 * @return the area of the triangle generated by the points p0, p1, and p2
 */
template <class T>
inline double area(const Point2D<T>& p0, const Point2D<T>& p1, const Point2D<T>& p2){
  //return (p1.x() - p0.x())*(p2.y()-p0.y()) - ((p2.x()-p0.x())*(p1.y()-p0.y()))/2;
  double A = -(p1.y() - p0.y());
  double B = p1.x() - p0.x();
  double C = -(A * p0.x() + B * p0.y());
  return A * p2.x() + B * p2.y() + C;
}

#endif // COMMON_MODULE_POINT2D_H
