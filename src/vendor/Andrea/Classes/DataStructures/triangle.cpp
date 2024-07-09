#include <Andrea/Headers/DataStructures/triangle.h>

Triangle::Triangle() {}

Triangle::Triangle(Point2Dd* vA, Point2Dd* vB, Point2Dd*  vC){
    this->vA = vA;
    this->vB = vB;
    this->vC = vC;
}

Triangle::~Triangle(){}

/**
 * Getter & Setter
 */

Point2Dd* Triangle::getA() const{
    return this->vA;
}

Point2Dd* Triangle::getB() const{
    return this->vB;
}

Point2Dd* Triangle::getC() const{
    return this->vC;
}

void Triangle::setA(Point2Dd* vA){
    this->vA = vA;
}

void Triangle::setB(Point2Dd* vB){
    this->vB = vB;
}

void Triangle::setC(Point2Dd* vC){
    this->vC = vC;
}

DagNode* Triangle::getDagNode() const{
    return this->dagNode;
}

void Triangle::setDagNode(DagNode* dn){
    this->dagNode = dn;
}

void Triangle::setIsDeleted(bool b){
    this->isDeleted = b;
}

bool Triangle::getIsDeleted() const{
    return this->isDeleted;
}

Triangle* Triangle::getTriangleAdjacentA() const{
   return triangleAdjacentA;
}

Triangle* Triangle::getTriangleAdjacentB() const{
   return triangleAdjacentB;
}

Triangle* Triangle::getTriangleAdjacentC() const{
   return triangleAdjacentC;
}

void Triangle::setTriangleAdjacentA(Triangle* trAdjA){
    this->triangleAdjacentA = trAdjA;
}

void Triangle::setTriangleAdjacentB(Triangle* trAdjB){
    this->triangleAdjacentB = trAdjB;
}

void Triangle::setTriangleAdjacentC(Triangle* trAdjC){
    this->triangleAdjacentC = trAdjC;
}

/**
 * @brief Triangle::checkPointIsVertexOfTriangle
 * @param p is the point that will be a vertex of the triangle
 * @return true is p is equal to vA, vB or vC, else otherwise
 * Check if a point is a vertex of the triangle
 */
bool Triangle::checkPointIsVertexOfTriangle(const Point2Dd& p){
    if(p.x() == this->vA->x() && p.y() == this->vA->y())
        return true;
    else if(p.x() == this->vB->x() && p.y() == this->vB->y())
        return true;
    else if(p.x() == this->vC->x() && p.y() == this->vC->y())
        return true;
    else
        return false;
}

/**
 * @brief Triangle::getThirdPoint
 * @param p1 is the first point to check
 * @param p2 is the second point to check
 * @return a pointer to the third point of the triangle tr
 * This method return the third point of a triangle.
 */
Point2Dd* Triangle::getThirdPoint(const Point2Dd& p1, const Point2Dd& p2){
    if( (*this->vA == p1 && *this->vB == p2) || (*this->vB == p1 && *this->vA == p2) ) // AB
        return this->vC;

    if( (*this->vA == p1 && *this->vC == p2) || (*this->vC == p1 && *this->vA == p2) ) // AC
        return this->vB;

    if( (*this->vB == p1 && *this->vC == p2) || (*this->vC == p1 && *this->vB == p2) ) // BC
        return this->vA;

    return nullptr;
}

/**
 * @brief Triangle::operator ==
 * @param tr
 * @return true if two triangles are equals, else otherwise
 */
bool Triangle::operator == (const Triangle& tr){
   if( *this->vA == *tr.getA() && *this->vB == *tr.getB() && *this->vC == *tr.getC())
     return true;
   else
     return false;
}
