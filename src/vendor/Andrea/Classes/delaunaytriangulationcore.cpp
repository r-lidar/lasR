#include <Andrea/Headers/delaunaytriangulationcore.h>
#include "Andrea/Headers/delaunay_checker.h"

DelaunayTriangulationCore::DelaunayTriangulationCore(){}

DelaunayTriangulationCore::~DelaunayTriangulationCore(){}

/**
 * @brief DelaunayTriangulationCore::addPoint
 * @param p is a point received from delaunaymanager.cpp
 * @return true if the point is added correctly inside the triangulation, else otherwise
 * Add a point inside triagulation.
 * Add point p inside vector of points, and check using Dag data structure where the point lie.
 * If point already exist inside the triangulation, return false else generate 3 new triangle
 * and legalize the edge. The point can lie inside a tringle or in one of his three edge
 */
bool DelaunayTriangulationCore::addPoint(const Point2Dd& p){

    DagNode* dagFather = Dag::navigate(dagNodes.front(), p);

    if(dagFather == nullptr) // Point already exist
        return false;

    points.push_back(new Point2Dd(p.x(), p.y()) );

    Triangle* triangleFather = dagFather->getTriangle();

    // p lie on AB
    if(pointLieInALine(*points.back(), *triangleFather->getA(), *triangleFather->getB()))
        pointLieAB(points.back(), triangleFather, dagFather);
    // p lie on BC
    else if(pointLieInALine(*points.back(), *triangleFather->getB(), *triangleFather->getC()))
        pointLieBC(points.back(), triangleFather, dagFather);
    // p lie on CA
    else if(pointLieInALine(*points.back(), *triangleFather->getC(), *triangleFather->getA()))
        pointLieCA(points.back(), triangleFather, dagFather);
    // p lie inside the triangle
    else
        pointLieInsideTriangle(points.back(), triangleFather, dagFather);

    return true;
}

/**
 * @brief DelaunayTriangulationCore::generateTriangle
 * @param p is the new vertex just insered
 * @param p1 is the second vertex of the new triangle
 * @param p2 is the third vertex of the new triangle
 * @param dagNodeFather is the dagNode of the triangle father (p1 and p2 is the vertex of that triangle)
 * @return a pointer of the new generated triangle
 * Generate a triangle using 3 point. After push_back of triangle, create his corresponding dag node,
 * and set as a child of dagNodeFather
 */
Triangle* DelaunayTriangulationCore::generateTriangle(Point2Dd* p,Point2Dd* p1, Point2Dd* p2, DagNode* dagNodeFather){
    this->triangles.push_back( new Triangle( p, p1, p2 ) );
    this->dagNodes.push_back( new DagNode( triangles.back() ) );

    triangles.back()->setDagNode( dagNodes.back() );

    Dag::addNode(dagNodes.back(), dagNodeFather);

    return triangles.back();
}

/**
 * @brief DelaunayTriangulationCore::generateTriangle
 * @param p is the new vertex just insered
 * @param p1 is the second vertex of the new triangle
 * @param p2 is the third vertex of the new triangle
 * @param dagNodeFather1 is the dagNode of the triangle father
 * @param dagNodeFather2 is the dagNode of the triangle father
 * @return a pointer of the new generated triangle
 * @return a pointer of the new generated triangle
 * Generate a triangle using 3 point after edge flip. After push_back of triangle, create his corresponding dag node,
 * and set as a child of dagNodeFather1 and dagNodeFather2
 */
Triangle* DelaunayTriangulationCore::generateTriangle(Point2Dd* p,Point2Dd* p1, Point2Dd* p2, DagNode* dagNodeFather1, DagNode* dagNodeFather2){
    this->triangles.push_back( new Triangle( p, p1, p2 ) );
    this->dagNodes.push_back( new DagNode( triangles.back() ) );
    triangles.back()->setDagNode( dagNodes.back() );

    Dag::addNode(dagNodes.back(), dagNodeFather1);
    Dag::addNode(dagNodes.back(), dagNodeFather2);

    return triangles.back();
}

/**
 * @brief DelaunayTriangulationCore::legalizeEdge
 * @param pr is the new vertex just insered. It can be used for and edge flip
 * @param pi is the first vertex which will be used to check the adjacency
 * @param pj is the second vertex which will be used to check the adjacency
 * @param tr is the triangle used for check the adjacencies
 * Check if a edge is legal or not, using the adjacencies of the triangle tr
 */
void DelaunayTriangulationCore::legalizeEdge(Point2Dd* pr, Point2Dd* pi, Point2Dd* pj, Triangle* tr){

    Triangle* adjTriangle = nullptr;

    if(tr->getTriangleAdjacentA() != nullptr){
        if(Adjacencies::isAdjacenciesForTwoPoints(*tr->getTriangleAdjacentA(), *pi, *pj) == true)
            adjTriangle = tr->getTriangleAdjacentA();
    }

    if(tr->getTriangleAdjacentB() != nullptr){
        if(Adjacencies::isAdjacenciesForTwoPoints(*tr->getTriangleAdjacentB(), *pi, *pj) == true)
            adjTriangle = tr->getTriangleAdjacentB();
    }

    if(tr->getTriangleAdjacentC() != nullptr){
        if(Adjacencies::isAdjacenciesForTwoPoints(*tr->getTriangleAdjacentC(), *pi, *pj) == true)
            adjTriangle = tr->getTriangleAdjacentC();
    }

    if(adjTriangle != nullptr){
        if(DelaunayTriangulation::Checker::isPointLyingInCircle(*adjTriangle->getA(),  *adjTriangle->getB(),  *adjTriangle->getC(), *pr,false) == true){
            edgeFlip(tr, adjTriangle, pr, pi, pj);
        }
     }

}

/**
 * @brief DelaunayTriangulationCore::edgeFlip
 * @param tr1 is the first triangle that will be used for edge flip
 * @param tr2 is the second triangle that will be used for edge flip
 * @param pr is the point used to do an edge flip
 * @param pi is the first point that composed the old triangle that will be deleted after flip
 * @param pj is the second point that composed the old triangle that will be deleted after flip
 * If legalizeEdge find an illegal edge, edgeFlip generate two new triangle using third point of the
 * adjacent triangle. After flip, chek the new edge using legalizeEdge again
*/
void DelaunayTriangulationCore::edgeFlip(Triangle* tr1, Triangle* tr2, Point2Dd* pr, Point2Dd* pi, Point2Dd* pj){
    tr1->setIsDeleted(true);
    tr2->setIsDeleted(true);

    Triangle* newTriangle1 = nullptr;
    Triangle* newTriangle2 = nullptr;

    Point2Dd* pk =  tr2->getThirdPoint(*pi, *pj);

    if(pk != nullptr){
        newTriangle1 = generateTriangle(pr, pi, pk, tr1->getDagNode(), tr2->getDagNode() );
        newTriangle2 = generateTriangle(pr, pk, pj, tr1->getDagNode(), tr2->getDagNode() );

        Adjacencies::setAdjacenciesAfterFlip(newTriangle1, newTriangle2, tr1, tr2);
        Adjacencies::setAdjacenciesAfterFlip(newTriangle2, newTriangle1, tr1, tr2);


        legalizeEdge(pr, pi, pk, newTriangle1);
        legalizeEdge(pr, pk, pj, newTriangle2);
    }
}

/**
 * @brief DelaunayTriangulationCore::cleanDelaunayTriangulation
 * Clean vector of points, triangulation, dag and map and other attributes and delete all pointers
 */
void DelaunayTriangulationCore::cleanDelaunayTriangulation(){

    for(unsigned int i = 0; i < points.size(); i++)
        delete points[i];

    for(unsigned int i = 0; i < triangles.size(); i++)
        delete triangles[i];

    for(unsigned int i = 0; i < dagNodes.size(); i++)
        delete dagNodes[i];

    points.clear();
    triangles.clear();
    dagNodes.clear();
    map.clear();
    validTriangles.resize(0,3);
    countValidTriangles = 0;

}

/**
 * @brief DelaunayTriangulationCore::loadPointFromVector
 * @param std::vector<Point2Dd>& vectorPoints is the vector of points received from delaunaymanager.cpp
 * Add one by one a point inside triangulation
 */
void DelaunayTriangulationCore::loadPointFromVector(const std::vector<Point2Dd>& vectorPoints){

    for (unsigned int i = 0; i < vectorPoints.size(); i++){
        addPoint(vectorPoints.at(i));
    }

}

/**
 * @brief DelaunayTriangulationCore::setBoundingTrianglePoints
 * @param p1 is the first vertex of the bounding triangle
 * @param p2 is the second vertex of the bounding triangle
 * @param p3 is the third vertex of the bounding triangle
 * Set the first three points inside the triangulation as bounding triangle.
 * Add the tree points inside the vector of points (as a pointer), and generate the triangle ad is
 * corresponding dag node.
 */
void DelaunayTriangulationCore::setBoundingTrianglePoints(const Point2Dd& p1, const Point2Dd& p2, const Point2Dd& p3){
    this->points.push_back( new Point2Dd (p1.x(), p1.y() ) );
    this->points.push_back( new Point2Dd (p2.x(), p2.y() ) );
    this->points.push_back( new Point2Dd (p3.x(), p3.y() ) );

    this->triangles.push_back( new Triangle( this->points.at(0), this->points.at(1), this->points.at(2)) );
    this->dagNodes.push_back( new DagNode( triangles.back() ) );

    triangles.back()->setDagNode( dagNodes.back() );
}

/**
 * @brief DelaunayTriangulationCore::pointLieInALine
 * @param p the point which will be checked
 * @param a the first point of the segment
 * @param b the second point of the segmen
 * @return true if point lie on an edge, else otherwise
 * To check if of point lie on a segment, is important to set the correct tollerance.
 * After a lot of tests I've set as a tollerance 0.000001. With a bigger tollerance
 * value, the delaunay algorith doesn't work good and probably go in infinite loop.
 * With smaller tollerance is more difficoult to find if a point lie on an edge.
 */
bool DelaunayTriangulationCore::pointLieInALine(const Point2Dd& p, const Point2Dd& a, const Point2Dd& b){

    double tollerance = 0.000001; /**< current tollerance value. Is the best value to find if a point lie on an edge. */
    //double tollerance = 0.01; /**< old tollerance value, try to use it to see the differences. */
    //double tollerance = 0.001; /**< old tollerance value, try to use it to see the differences. */

    double pab = a.dist(p) + p.dist(b);

    double ab = a.dist(b);

    if( (pab - ab) < tollerance)
        return true;
    else
        return false;

}

/***********************************************
 * The following four methods define how the algorith must work
 * after a point is insered inside the triangulation.
 * pointLieInsideTriangle(..) is used when a point is totaly inside the triangle found using the Dag datastructure
 * pointLieAB(..), pointLieBC(..), pointLieCA(..) are called when the point lies on one of
 * three edge of the triangle. The main difference between this function is the 4 triangles that will
 * be generate, composed by different verticies.
 ***********************************************/

/**
 * @brief DelaunayTriangulationCore::pointLieInsideTriangle
 * @param pr is the new point just added
 * @param triangleFather is the triangle where pr lie
 * @param dagFather corresponding dag node of triangleFather
 */
void DelaunayTriangulationCore::pointLieInsideTriangle(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather){
    Triangle* tr1 = generateTriangle(pr, triangleFather->getA(), triangleFather->getB(), dagFather );
    Triangle* tr2 = generateTriangle(pr, triangleFather->getB(), triangleFather->getC(), dagFather );
    Triangle* tr3 = generateTriangle(pr, triangleFather->getC(), triangleFather->getA(), dagFather );

    triangleFather->setIsDeleted(true);

    Adjacencies::setAdjacencies(tr1, tr2, tr3, triangleFather);
    Adjacencies::setAdjacencies(tr2, tr3, tr1, triangleFather);
    Adjacencies::setAdjacencies(tr3, tr1, tr2, triangleFather);

    legalizeEdge( tr1->getA(), tr1->getB(), tr1->getC(), tr1 );
    legalizeEdge( tr2->getA(), tr2->getB(), tr2->getC(), tr2 );
    legalizeEdge( tr3->getA(), tr3->getB(), tr3->getC(), tr3 );
}

/**
 * @brief DelaunayTriangulationCore::pointLieAB
 * @param pr is the new point just added
 * @param triangleFather is the triangle where pr lie
 * @param dagFather corresponding dag node of triangleFather
 */
void DelaunayTriangulationCore::pointLieAB(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather){

    Triangle* adjacentTriangle = Adjacencies::getTriangleAdjacentByTwoPoints(triangleFather, *triangleFather->getA(), *triangleFather->getB());

    DagNode* adjacentDagNode = adjacentTriangle->getDagNode();

    triangleFather->setIsDeleted(true);
    adjacentTriangle->setIsDeleted(true);


    Point2Dd* pl = adjacentTriangle->getThirdPoint(*triangleFather->getA(), *triangleFather->getB());

    Triangle* tr1 = generateTriangle(pr, triangleFather->getB(), triangleFather->getC(), dagFather);
    Triangle* tr2 = generateTriangle(pr, triangleFather->getC(), triangleFather->getA(), dagFather);
    Triangle* tr3 = generateTriangle(pr, triangleFather->getA(), pl, adjacentDagNode );
    Triangle* tr4 = generateTriangle(pr, pl, triangleFather->getB(), adjacentDagNode );


    Adjacencies::setAdjacencies(tr1, tr2, tr4, triangleFather);
    Adjacencies::setAdjacencies(tr2, tr3, tr1, triangleFather);
    Adjacencies::setAdjacencies(tr3, tr4, tr2, adjacentTriangle);
    Adjacencies::setAdjacencies(tr4, tr1, tr3, adjacentTriangle);

    legalizeEdge( tr4->getA(), tr4->getB(), tr4->getC(), tr4 );
    legalizeEdge( tr3->getA(), tr3->getB(), tr3->getC(), tr3 );
    legalizeEdge( tr2->getA(), tr2->getB(), tr2->getC(), tr2 );
    legalizeEdge( tr1->getA(), tr1->getB(), tr1->getC(), tr1 );
}

/**
 * @brief DelaunayTriangulationCore::pointLieBC
 * @param pr is the new point just added
 * @param triangleFather is the triangle where pr lie
 * @param dagFather corresponding dag node of triangleFather
 */
void DelaunayTriangulationCore::pointLieBC(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather){
    Triangle* adjacentTriangle = Adjacencies::getTriangleAdjacentByTwoPoints(triangleFather, *triangleFather->getB(), *triangleFather->getC());

    DagNode* adjacentDagNode = adjacentTriangle->getDagNode();

    triangleFather->setIsDeleted(true);
    adjacentTriangle->setIsDeleted(true);

    Point2Dd* pl = adjacentTriangle->getThirdPoint(*triangleFather->getB(), *triangleFather->getC());

    Triangle* tr1 = generateTriangle(pr, triangleFather->getC(), triangleFather->getA(), dagFather);
    Triangle* tr2 = generateTriangle(pr, triangleFather->getA(), triangleFather->getB(), dagFather);
    Triangle* tr3 = generateTriangle(pr, triangleFather->getB(), pl, adjacentDagNode );
    Triangle* tr4 = generateTriangle(pr, pl, triangleFather->getC(), adjacentDagNode );

    Adjacencies::setAdjacencies(tr1, tr2, tr4, triangleFather);
    Adjacencies::setAdjacencies(tr2, tr3, tr1, triangleFather);
    Adjacencies::setAdjacencies(tr3, tr4, tr2, adjacentTriangle);
    Adjacencies::setAdjacencies(tr4, tr1, tr3, adjacentTriangle);

    legalizeEdge( tr4->getA(), tr4->getB(), tr4->getC(), tr4 );
    legalizeEdge( tr3->getA(), tr3->getB(), tr3->getC(), tr3 );
    legalizeEdge( tr2->getA(), tr2->getB(), tr2->getC(), tr2 );
    legalizeEdge( tr1->getA(), tr1->getB(), tr1->getC(), tr1 );
}

/**
 * @brief DelaunayTriangulationCore::pointLieCA
 * @param pr is the new point just added
 * @param triangleFather is the triangle where pr lie
 * @param dagFather corresponding dag node of triangleFather
 */
void DelaunayTriangulationCore::pointLieCA(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather){
    Triangle* adjacentTriangle = Adjacencies::getTriangleAdjacentByTwoPoints(triangleFather, *triangleFather->getC(), *triangleFather->getA());

    DagNode* adjacentDagNode = adjacentTriangle->getDagNode();

    triangleFather->setIsDeleted(true);
    adjacentTriangle->setIsDeleted(true);

    Point2Dd* pl = adjacentTriangle->getThirdPoint(*triangleFather->getC(), *triangleFather->getA());

    Triangle* tr1 = generateTriangle(pr, triangleFather->getA(), triangleFather->getB(), dagFather);
    Triangle* tr2 = generateTriangle(pr, triangleFather->getB(), triangleFather->getC(), dagFather);
    Triangle* tr3 = generateTriangle(pr, triangleFather->getC(), pl, adjacentDagNode  );
    Triangle* tr4 = generateTriangle(pr, pl, triangleFather->getA(), adjacentDagNode );

    Adjacencies::setAdjacencies(tr1, tr2, tr4, triangleFather);
    Adjacencies::setAdjacencies(tr2, tr3, tr1, triangleFather);
    Adjacencies::setAdjacencies(tr3, tr4, tr2, adjacentTriangle);
    Adjacencies::setAdjacencies(tr4, tr1, tr3, adjacentTriangle);

    legalizeEdge( tr4->getA(), tr4->getB(), tr4->getC(), tr4 );
    legalizeEdge( tr3->getA(), tr3->getB(), tr3->getC(), tr3 );
    legalizeEdge( tr2->getA(), tr2->getB(), tr2->getC(), tr2 );
    legalizeEdge( tr1->getA(), tr1->getB(), tr1->getC(), tr1 );
}

/**
 * @brief DelaunayTriangulationCore::getTriangles
 * @return std::vector<Triangle*>, my triangulation vector
 * Simply return the triangulation vector
 */
std::vector<Triangle*> DelaunayTriangulationCore::getTriangles(){
    return this->triangles;
}

/***********************************************
 * These methods are used for the validation process
 * to perform a better an efficent export of the triangulation
 * the points are stored inside a std::map<Point2Dd, int>
 * that associates the Point2Dd with his index on the main
 * vector of points.
 ***********************************************/

/**
 * @brief DelaunayTriangulationCore::getPointsForValidation
 * @return a vector of Point2Dd
 * Convert std::vector<Point2Dd*> into std::vector<Point2Dd> and
 * stored inside a std::map<Point2Dd, int> the Point2Dd with his index on the main
 * vector of points.
 */
std::vector<Point2Dd> DelaunayTriangulationCore::getPointsForValidation(){
    std::vector<Point2Dd> v;

    for(unsigned int i = 0; i < this->points.size(); i++){
        v.push_back(*this->points.at(i));
        this->map.insert ( std::pair<Point2Dd,int>(*this->points.at(i), i) );
    }
    return v;
}

/**
 * @brief DelaunayTriangulationCore::getTrianglesForValidation
 * Create a Array2D of the triangulation, increasing the counter of valid triangles
 */
void DelaunayTriangulationCore::generateTrianglesForValidation(){

    int index = 0;

    for(unsigned int i = 0; i < this->triangles.size(); i++){
        if(this->triangles.at(i)->getIsDeleted() == false){

            this->countValidTriangles++;

            this->validTriangles.resize( this->countValidTriangles, 3);

            this->validTriangles(index, 0) = this->map.find( *this->triangles.at(i)->getA() )->second;
            this->validTriangles(index, 1) = this->map.find( *this->triangles.at(i)->getB() )->second;
            this->validTriangles(index, 2) = this->map.find( *this->triangles.at(i)->getC() )->second;
            index++;
        }
    }
}

/**
 * @brief DelaunayTriangulationCore::getCountValidTriangle
 * @return the counter of valid triangles.
 * This value will be use on delaunaymanager.cpp to resize the array2D
 */
int DelaunayTriangulationCore::getCountValidTriangle() const{
    return this->countValidTriangles;
}

/**
 * @brief DelaunayTriangulationCore::getValidTriangles
 * @return an array2D that will be use on delaunaychecker to
 * valide the triangulation
 */
Array2D<unsigned int> DelaunayTriangulationCore::getValidTriangles() const{
    return this->validTriangles;
}
