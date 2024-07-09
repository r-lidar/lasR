/*
 * @author    Alessandro Muntoni (muntoni.alessandro@gmail.com)
 * @copyright Alessandro Muntoni 2016.
 */

#ifndef COMMON_MODULE__POINT_H
#define COMMON_MODULE__POINT_H

#include <assert.h>
#include <string>
#include <iostream>

#ifdef CINOLIB_DEFINED
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable" //Doesn't work on gcc < 6.0
#include <cinolib/geometry/vec3.h>
#pragma GCC diagnostic pop
#endif //__GNUC__
#endif //CINOLIB_DEFINED

/**
 * \~English
 * @class Point
 * @brief The Point class models a point or a vector on a 3D space.
 *
 * Represents a 3D point or vector, with the precision given by the template type T.
 * In particular, it is possible to have vectros/points with integer, float or double precision,
 * using the specified types Pointi, Pointf and Pointd.
 * There is also the type Vec3, that is a Pointd, which is a simple sinctactic sugar in order to
 * distinguish between points on a 3D space and vectors.
 *
 * @author Alessandro Muntoni (muntoni.alessandro@gmail.com)
 */
template <class T> class Point {

    public:

        /****************
         * Constructors *
         ****************/

        Point(T xCoord = 0.0, T yCoord = 0.0, T zCoord = 0.0);
        #ifdef COMMON_WITH_EIGEN
        Point(const Eigen::VectorXd &v);
        #endif
        #ifdef CINOLIB_DEFINED
        Point(const cinolib::vec3<T> &v);
        #endif

        /*************************
        * Public Inline Methods *
        *************************/

        const T& x()                                        const;
        const T& y()                                        const;
        const T& z()                                        const;
        double dist(const Point<T>& otherPoint)             const;
        double dot(const Point<T>& otherVector)             const;
        Point<T> cross(const Point<T>& otherVector)         const;
        double getLength()                                  const;
        double getLengthSquared()                           const;
        Point<T> min(const Point<T>& otherPoint)            const;
        Point<T> max(const Point<T>& otherPoint)            const;
        // SerializableObject interface
        void serialize(std::ofstream &myfile)               const;

        // Operators
        const T& operator[](unsigned int i)                 const;
        const T& operator()(unsigned int i)                 const;
        bool operator == (const Point<T>& otherPoint)       const;
        bool operator != (const Point<T>& otherPoint)       const;
        bool operator < (const Point<T>& otherPoint)        const;
        Point<T> operator - ()                              const;
        Point<T> operator + (const T& scalar)               const;
        Point<T> operator + (const Point<T>& otherPoint)    const;
        Point<T> operator - (const T& scalar)               const;
        Point<T> operator - (const Point<T>& otherPoint)    const;
        Point<T> operator * (const T& scalar)               const;
        Point<T> operator * (const Point<T>& otherPoint)    const;
        Point<T> operator / (const T& scalar )              const;
        Point<T> operator / (const Point<T>& otherPoint)    const;


        T& x();
        T& y();
        T& z();
        void setX(const T& x);
        void setY(const T& y);
        void setZ(const T& z);
        void set(const T& x, const T& y, const T& z);
        double normalize();
        #ifdef COMMON_WITH_EIGEN
        void rotate(const Eigen::Matrix3d &matrix, const Point<T>& centroid = Point<T>());
        #endif //COMMON_WITH_EIGEN
        void rotate(double matrix[3][3], const Point<T>& centroid = Point<T>());

        // SerializableObject interface
        bool deserialize(std::ifstream &myfile);

        // Operators
        T& operator[](unsigned int i);
        T& operator()(unsigned int i);
        Point<T> operator += (const Point<T>& otherPoint);
        Point<T> operator -= (const Point<T>& otherPoint);
        Point<T> operator *= (const T& scalar);
        Point<T> operator *= (const Point<T>& otherPoint);
        Point<T> operator /= (const T& scalar );
        Point<T> operator /= (const Point<T>& otherPoint);

        /*****************
        * Public Methods *
        ******************/

        std::string toString() const;

    protected:

        /**************
        * Attributes *
        **************/

        T xCoord; /**< \~English @brief The \c x component of the point/vector */
        T yCoord; /**< \~English @brief The \c y component of the point/vector */
        T zCoord; /**< \~English @brief The \c z component of the point/vector */

};

/****************
* Other Methods *
*****************/

template <class T>
Point<T> operator * (const T& scalar, const Point<T>& point);

template <class T>
Point<T> mul(const T m[][3], const Point<T>& point);

#ifdef COMMON_WITH_EIGEN
template <class T>
Point<T> mul(const Eigen::Matrix3d &m, const Point<T>& point);
#endif //COMMON_WITH_EIGEN

template <class T>
std::ostream& operator<< (std::ostream& inputStream, const Point<T>& p);

/**************
* Other Types *
***************/

typedef Point<float>  Pointf; /**< \~English @brief Point composed of float components */
typedef Point<double> Pointd; /**< \~English @brief Point composed of double components */
typedef Point<int>    Pointi; /**< \~English @brief Point composed of integer components */
typedef Point<double>   Vec3; /**< \~English @brief Point composed of double components, sinctactic sugar for discriminate points from vectors */
template<typename T>
using Point3D = Point<T>; /**< \~English @brief alias of Point */

#include "Andrea/Headers/point.h"
#include <cmath>

/****************
 * Constructors *
 ****************/

/**
 * \~English
 * @brief Constructor, initializes the point with the input values
 * @param[in] x: value of \c x component, default 0
 * @param[in] y: value of \c y component, default 0
 * @param[in] z: value of \c z component, default 0
 */
template <class T>
inline Point<T>::Point(T x, T y, T z) : xCoord(x), yCoord(y), zCoord(z) {
}

#ifdef COMMON_WITH_EIGEN
template <class T>
Point<T>::Point(const Eigen::VectorXd& v) : xCoord(v(0)), yCoord(v(1)), zCoord(v(2)) {
}
#endif

#ifdef CINOLIB_DEFINED
template <class T>
Point<T>::Point(const cinolib::vec3<T>& v) : xCoord(v.x()), yCoord(v.y()), zCoord(v.z()) {
}
#endif


/*************************
 * Public Inline Methods *
 *************************/

/**
 * \~English
 * @brief Returns the \c x component of the point/vector
 * @return \c x component
 */
template <class T>
inline const T& Point<T>::x() const {
  return this->xCoord;
}

/**
 * \~English
 * @brief Returns the \c y component of the point/vector
 * @return \c y component
 */
template <class T>
inline const T& Point<T>::y() const {
  return this->yCoord;
}

/**
 * \~English
 * @brief Returns the \c z component of the point/vector
 * @return \c z component
 */
template <class T>
inline const T& Point<T>::z() const {
  return this->zCoord;
}

/**
 * \~English
 * @brief Function that calculates the euclidean distance between two points
 * @param[in] otherPoint: point on which is calculated the distance
 * @return The distance between the point and \c otherPoint
 */
template <class T>
inline double Point<T>::dist(const Point<T>& otherPoint) const {
  return sqrt ( std::pow((xCoord - otherPoint.xCoord), 2) +
                std::pow((yCoord - otherPoint.yCoord), 2) +
                std::pow((zCoord - otherPoint.zCoord), 2) );
}

/**
 * \~English
 * @brief Function that calculates the dot product between two vectors
 * @param[in] otherVector: vector on which is calculated the dot product
 * @return The dot product between this and \c otherVector
 */
template <class T>
inline double Point<T>::dot(const Point<T>& otherVector) const {
  return xCoord * otherVector.xCoord +
    yCoord * otherVector.yCoord +
    zCoord * otherVector.zCoord;
}

/**
 * \~English
 * @brief Function which calculates the cross product between two vectors
 * @param[in] otherVector: vector on which is calculated the cross product
 * @return The cross product between this and \c otherVector
 */
template <class T>
inline Point<T> Point<T>::cross(const Point<T>& otherVector) const {
  return Point<T>(yCoord * otherVector.zCoord - zCoord * otherVector.yCoord,
                  zCoord * otherVector.xCoord - xCoord * otherVector.zCoord,
                  xCoord * otherVector.yCoord - yCoord * otherVector.xCoord);
}

/**
 * \~English
 * @brief Function which calculated the length of the vector
 * @return The length of the vector
 */
template <class T>
inline double Point<T>::getLength() const {
  return sqrt( xCoord*xCoord + yCoord*yCoord + zCoord*zCoord );
}

/**
 * \~Italian
 * @brief Operatore per il calcolo della lunghezza al quadrato di un vettore
 * @return La lunghezza al quadrato del vettore this
 */
template <class T>
inline double Point<T>::getLengthSquared() const {
  return xCoord * xCoord + yCoord * yCoord + zCoord * zCoord;
}

/**
 * \~Italian
 * @brief Funzione di minimo tra punti/vettori.
 *
 * Ogni componente del punto/vettore restituito sarà uguale alla corrispondente componente minore tra il punto/vettore
 * this e otherPoint.
 *
 * @param[in] otherPoint: punto/vettore con cui viene calcolata la funzione di minimo
 *
 * @return Il punto/vettore dei minimi
 */
template <class T>
inline Point<T> Point<T>::min(const Point<T>& otherPoint) const {
  return Point<T>(std::min(x(), otherPoint.x()),
                  std::min(y(), otherPoint.y()),
                  std::min(z(), otherPoint.z()));
}

/**
 * \~Italian
 * @brief Funzione di massimo tra punti/vettori.
 *
 * Ogni componente del punto/vettore restituito sarà uguale alla corrispondente componente maggiore tra il punto/vettore
 * this e otherPoint.
 *
 * @param[in] otherPoint: punto/vettore con cui viene calcolata la funzione di massimo
 *
 * @return Il punto/vettore dei massimi
 */
template <class T>
inline Point<T> Point<T>::max(const Point<T>& otherPoint) const {
  return Point<T>(std::max(x(), otherPoint.x()),
                  std::max(y(), otherPoint.y()),
                  std::max(z(), otherPoint.z()));
}

template <class T>
inline const T& Point<T>::operator[](unsigned int i) const {
  assert(i < 3);
  switch (i){
  case 0: return xCoord;
  case 1: return yCoord;
  case 2: return zCoord;
  }
  return xCoord;
}

template <class T>
inline const T& Point<T>::operator()(unsigned int i) const {
  assert(i < 3);
  switch (i){
  case 0: return xCoord;
  case 1: return yCoord;
  case 2: return zCoord;
  }
  return xCoord;
}

/**
 * \~Italian
 * @brief Operatore di uguaglianza tra punti/vettori.
 *
 * Due punti/vettori sono considerati uguali se tutte e tre le loro componenti sono uguali.
 *
 * @param[in] otherPoint: punto/vettore con cui viene verificata l'uguaglianza
 * @return True se il punto e otherPoint sono uguali, false altrimenti
 */
template <class T>
inline bool Point<T>::operator == (const Point<T>& otherPoint) const {
  if ( otherPoint.xCoord != xCoord )	return false;
  if ( otherPoint.yCoord != yCoord )	return false;
  if ( otherPoint.zCoord != zCoord )	return false;
  return true;
}

/**
 * \~Italian
 * @brief Operatore di disuguaglianza tra punti/vettori.
 *
 * Due punti/vettori sono considerati diversi se almeno una delle loro componenti è diversa.
 *
 * @param[in] otherPoint: punto/vettore con cui viene verificata la disuguaglianza
 * @return True se il punto e otherPoint sono diversi, false altrimenti
 */
template <class T>
inline bool Point<T>::operator != (const Point<T>& otherPoint) const {
  if ( otherPoint.xCoord != xCoord )	return true;
  if ( otherPoint.yCoord != yCoord )	return true;
  if ( otherPoint.zCoord != zCoord )	return true;
  return false;
}

/**
 * \~Italian
 * @brief Operatore di minore tra punti/vettori.
 *
 * In questo contesto, il punto/vettore è minore di otherPoint se la sua componente x
 * è minore di quella di otherPoint; in caso di uguaglianza si verifica la componente y
 * e in caso di ultieriore uguaglianza la componente z.
 *
 * @param[in] otherPoint: altro punto/vettore
 * @return True se il punto/vettore this è minore di otherPoint, false altrimenti
 */
template <class T>
inline bool Point<T>::operator < (const Point<T>& otherPoint) const {
  if (this->xCoord < otherPoint.xCoord) return true;
  if (this->xCoord > otherPoint.xCoord) return false;
  if (this->yCoord < otherPoint.yCoord) return true;
  if (this->yCoord > otherPoint.yCoord) return false;
  if (this->zCoord < otherPoint.zCoord) return true;
  return false;
}

/**
 * \~Italian
 * @brief Operatore prefisso di negazione, restituisce il punto/vettore negato
 * @return Il punto/vettore negato
 */
template <class T>
inline Point<T> Point<T>::operator - () const {
  return Point<T>(-xCoord, -yCoord, -zCoord);
}

template <class T>
inline Point<T> Point<T>::operator +(const T& scalar) const {
  return Point<T>(xCoord + scalar,
                  yCoord + scalar,
                  zCoord + scalar);
}

/**
 * \~Italian
 * @brief Operatore di somma tra punti/vettori
 * @param[in] otherPoint: punto/vettore con cui verrà sommato il punto/vettore this
 * @return Il punto/vettore risultato della somma, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator + (const Point<T>& otherPoint) const {
  return Point<T>(xCoord + otherPoint.xCoord,
                  yCoord + otherPoint.yCoord,
                  zCoord + otherPoint.zCoord);
}

template <class T>
inline Point<T> Point<T>::operator -(const T& scalar) const {
  return Point<T>(xCoord - scalar,
                  yCoord - scalar,
                  zCoord - scalar);
}

/**
 * \~Italian
 * @brief Operatore di sottrazione tra punti/vettori
 * @param[in] otherPoint: punto/vettore che verrà sottratto al punto/vettore this
 * @return Il punto/vettore risultato della differenza, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator - (const Point<T>& otherPoint) const {
  return Point<T>(xCoord - otherPoint.xCoord,
                  yCoord - otherPoint.yCoord,
                  zCoord - otherPoint.zCoord);
}

/**
 * \~Italian
 * @brief Operatore di prodotto scalare tra un punto/vettore e uno scalare
 * @param[in] scalar: scalare con cui verrà eseguito il prodotto scalare
 * @return Il punto/vettore risultato del prodotto scalare tra tra il punto/vettore this e scalar
 */
template <class T>
inline Point<T> Point<T>::operator * (const T& scalar) const {
  return Point<T>(xCoord * scalar, yCoord * scalar, zCoord * scalar);
}

/**
 * \~Italian
 * @brief Operatore di prodotto, componente per componente, tra punti/vettori
 * @param[in] otherPoint: punto/vettore con cui verrà eseguito il prodotto
 * @return Il punto/vettore risultato del prodotto, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator * (const Point<T>& otherPoint) const {
  return Point<T>(xCoord * otherPoint.xCoord, yCoord * otherPoint.yCoord, zCoord * otherPoint.zCoord);
}

/**
 * \~Italian
 * @brief Operatore di quoziente scalare tra un punto/vettore e uno scalare
 * @param[in] scalar: scalare con cui verrà eseguito il quoziente scalare
 * @return Il punto/vettore risultato del quoziente scalare tra il punto/vettore this e scalar
 */
template <class T>
inline Point<T> Point<T>::operator / (const T& scalar) const {
  return Point<T>(xCoord / scalar, yCoord / scalar, zCoord / scalar);
}

/**
 * \~Italian
 * @brief Operatore di quoziente, componente per componente, tra punti/vettori
 * @param[in] otherPoint: punto/vettore con cui verrà eseguito il quoziente
 * @return Il punto/vettore risultato del quoziente, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator / (const Point<T>& otherPoint) const {
  return Point<T>(xCoord / otherPoint.xCoord, yCoord / otherPoint.yCoord, zCoord / otherPoint.zCoord);
}

/**
 * \~English
 * @brief Returns the \c x component of the point/vector
 * @return \c x component
 *
 * \~Italian
 * @brief Restituisce la componente \c x del punto/vettore
 * @return La componente \c x
 */
template <class T>
inline T& Point<T>::x() {
  return this->xCoord;
}

/**
 * \~English
 * @brief Returns the \c y component of the point/vector
 * @return \c y component
 *
 * \~Italian
 * @brief Restituisce la componente \c y del punto/vettore
 * @return La componente \c y
 */
template <class T>
inline T& Point<T>::y() {
  return this->yCoord;
}

/**
 * \~English
 * @brief Returns the \c z component of the point/vector
 * @return \c z component
 *
 * \~Italian
 * @brief Restituisce la componente \c z del punto/vettore
 * @return La componente \c z
 */
template <class T>
inline T& Point<T>::z() {
  return this->zCoord;
}

/**
 * \~Italian
 * @brief Modifica la componente x del punto/vettore this
 * @param[in] x: valore settato come componente x
 */
template <class T>
inline void Point<T>::setX(const T& x) {
  xCoord = x;
}

/**
 * \~Italian
 * @brief Modifica la componente y del punto/vettore this
 * @param[in] y: valore settato come componente y
 */
template <class T>
inline void Point<T>::setY(const T& y) {
  yCoord = y;
}

/**
 * \~Italian
 * @brief Modifica la componente z del punto/vettore this
 * @param[in] z: valore settato come componente z
 */
template <class T>
inline void Point<T>::setZ(const T& z) {
  zCoord = z;
}

/**
 * \~Italian
 * @brief Modifica le componenti del punto/vettore this
 * @param[in] x: valore settato come componente x
 * @param[in] y: valore settato come componente y
 * @param[in] z: valore settato come componente z
 */
template <class T>
inline void Point<T>::set(const T& x, const T& y, const T& z) {
  xCoord = x;
  yCoord = y;
  zCoord = z;
}

/**
 * \~Italian
 * @brief Funzione di normalizzazione di un vettore, in modo tale che la sua lunghezza sia pari a 1
 * @return La lunghezza precedente del vettore prima di essere normalizzato
 */
template <class T>
inline double Point<T>::normalize() {
  double len = getLength();
  xCoord /= len;
  yCoord /= len;
  zCoord /= len;
  return len;
}

#ifdef COMMON_WITH_EIGEN
template <class T>
void Point<T>::rotate(const Eigen::Matrix3d& matrix, const Point<T>& centroid) {
  *this -= centroid;
  *this = mul(matrix, *this);
  *this += centroid;
}
#endif //COMMON_WITH_EIGEN

/**
 * \~Italian
 * @brief Applica una matrice di rotazione 3x3 ad un punto/vettore
 * @param[in] m: matrice di rotazione 3x3
 * @param[in] centroid: punto centroide della rotazione, di default (0,0,0)
 */
template <class T>
inline void Point<T>::rotate(double matrix[3][3], const Point<T>& centroid) {
  *this -= centroid;
  *this = mul(matrix, *this);
  *this += centroid;
}

template <class T>
inline T& Point<T>::operator[](unsigned int i) {
  assert(i < 3);
  switch (i){
  case 0: return xCoord;
  case 1: return yCoord;
  case 2: return zCoord;
  }
  return xCoord;
}

template <class T>
inline T& Point<T>::operator()(unsigned int i) {
  assert(i < 3);
  switch (i){
  case 0: return xCoord;
  case 1: return yCoord;
  case 2: return zCoord;
  }
  return xCoord;
}

/**
 * \~Italian
 * @brief Operatore di somma e assegnamento tra punti/vettori.
 *
 * Il risultato della somma è assegnato al punto/vettore this.
 *
 * @param[in] otherPoint: punto/vettore con cui verrà sommato il punto/vettore this
 * @return Il punto/vettore risultato della somma, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator += (const Point<T>& otherPoint) {
  xCoord += otherPoint.xCoord;
  yCoord += otherPoint.yCoord;
  zCoord += otherPoint.zCoord;
  return *this;
}

/**
 * \~Italian
 * @brief Operatore di sottrazione e assegnamento tra punti/vettori.
 *
 * Il risultato della differenza è assegnato al punto/vettore this.
 *
 * @param[in] otherPoint: punto/vettore che verrà sottratto al punto/vettore this
 * @return Il punto/vettore risultato della differenza, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator -= (const Point<T>& otherPoint) {
  xCoord -= otherPoint.xCoord;
  yCoord -= otherPoint.yCoord;
  zCoord -= otherPoint.zCoord;
  return *this;
}

/**
 * \~Italian
 * @brief Operatore di prodotto scalare e assegnamento tra un punto/vettore e uno scalare.
 *
 * Il risultato del prodotto scalare è assegnato al punto/vettore this.
 *
 * @param[in] scalar: scalare con cui verrà eseguito il prodotto scalare
 * @return Il punto/vettore risultato del prodotto scalare tra tra il punto/vettore this e scalar
 */
template <class T>
inline Point<T> Point<T>::operator *= (const T& scalar) {
  xCoord *= scalar;
  yCoord *= scalar;
  zCoord *= scalar;
  return *this;
}

/**
 * \~Italian
 * @brief Operatore di prodotto, componente per componente, e assegnamento tra punti/vettori.
 *
 * Il risultato del prodotto è assegnato al punto/vettore this.
 *
 * @param[in] otherPoint: punto/vettore con cui verrà eseguito il prodotto
 * @return Il punto/vettore risultato del prodotto, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator *= (const Point<T>& otherPoint) {
  xCoord *= otherPoint.xCoord;
  yCoord *= otherPoint.yCoord;
  zCoord *= otherPoint.zCoord;
  return *this;
}

/**
 * \~Italian
 * @brief Operatore di quoziente scalare e assegnamento tra un punto/vettore e uno scalare.
 *
 * Il risultato del quoziente scalare è assegnato al punto/vettore this.
 *
 * @param[in] scalar: scalare con cui verrà eseguito il quoziente scalare
 * @return Il punto/vettore risultato del quoziente scalare tra il punto/vettore this e scalar
 */
template <class T>
inline Point<T> Point<T>::operator /= (const T& scalar) {
  xCoord /= scalar;
  yCoord /= scalar;
  zCoord /= scalar;
  return *this;
}

/**
 * \~Italian
 * @brief Operatore di quoziente, componente per componente, e assegnamento tra punti/vettori.
 *
 * Il risultato del quoziente è assegnato al punto/vettore this.
 *
 * @param[in] otherPoint: punto/vettore con cui verrà eseguito il quoziente
 * @return Il punto/vettore risultato del quoziente, componente per componente, tra i punti/vettori this e otherPoint
 */
template <class T>
inline Point<T> Point<T>::operator /= (const Point<T>& otherPoint) {
  xCoord /= otherPoint.xCoord;
  yCoord /= otherPoint.yCoord;
  zCoord /= otherPoint.zCoord;
  return *this;
}

/*****************
 * Public Methods *
 ******************/

/**
 * \~Italian
 * @brief Funzione toString di un punto/vettore
 * @return Una stringa rappresentativa del punto/vettore this
 */
template <class T>
std::string Point<T>::toString() const {
  return "[" + std::to_string(xCoord) + ", " + std::to_string(yCoord) + ", " + std::to_string(zCoord) + "]";
}

/****************
 * Other Methods *
 *****************/
/**
 * \~Italian
 * @brief Operatore di prodotto scalare tra un punto/vettore e uno scalare
 * @param[in] scalar: scalare con cui verrà eseguito il prodotto scalare
 * @param[in] point: punto/vettore con cui verrà eseguito il prodotto scalare
 * @return Il punto/vettore risultato del prodotto scalare tra tra point e scalar
 */
template <class T>
inline Point<T> operator * (const T& scalar, const Point<T>& point) {
  return Point<T>(point.xCoord * scalar,
                  point.yCoord * scalar,
                  point.zCoord * scalar);
}

template <class T>
inline Point<T> mul(const T m[][3], const Point<T>& point) {
  Point<T> tmp = point;
  tmp.setX(m[0][0]*point.x() + m[0][1]*point.y() + m[0][2]*point.z());
  tmp.setY(m[1][0]*point.x() + m[1][1]*point.y() + m[1][2]*point.z());
  tmp.setZ(m[2][0]*point.x() + m[2][1]*point.y() + m[2][2]*point.z());
  return tmp;
}

#ifdef COMMON_WITH_EIGEN
template <class T>
inline Point<T> mul(const Eigen::Matrix3d &m, const Point<T>& point) {
  Point<T> tmp = point;
  tmp.setX(m(0,0)*point.x() + m(0,1)*point.y() + m(0,2)*point.z());
  tmp.setY(m(1,0)*point.x() + m(1,1)*point.y() + m(1,2)*point.z());
  tmp.setZ(m(2,0)*point.x() + m(2,1)*point.y() + m(2,2)*point.z());
  return tmp;
}
#endif //COMMON_WITH_EIGEN

/**
 * \~Italian
 * @brief Operatore di stram sul punto/vettore
 * @param[in] input_stream: stream di input
 * @return Lo stream di input a cui è stato accodato lo stream del punto/vettore
 */
template <class T>
std::ostream& operator<<(std::ostream& inputStream, const Point<T>& p) {
  inputStream << "[" << p.x() << ", " << p.y() << ", " << p.z() << "]";
  return inputStream;
}

#endif // COMMON_MODULE_POINT_H
