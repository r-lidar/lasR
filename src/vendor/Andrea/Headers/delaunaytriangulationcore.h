#ifndef DELAUANYTRIANGULATIONCORE_H
#define DELAUANYTRIANGULATIONCORE_H

#include <Andrea/Headers/DataStructures/dagnode.h>
#include <Andrea/Headers/DataStructures/triangle.h>
#include <Andrea/Headers/Static/dag.h>
#include <Andrea/Headers/Static/adjacencies.h>
#include <Andrea/Headers/arrays.h>
//#include <utils/delaunay_checker.h>
#include <float.h>
#include <map>

/**
 * @brief The DelaunayTriangulationCore class
 * This class is the core of all Delaunay Triangulation Algorithm.
 * This class manage the datastructure, the pointer, send the triangulation to the canvas
 * receives the points from a file etc.
 */
class DelaunayTriangulationCore{

public:
    DelaunayTriangulationCore();
    ~DelaunayTriangulationCore();

    // Points of my triangulation
    std::vector<Point2Dd*> points; /**< vector of pointer of Point2Dd of the triangulation */

    // Triangles of my triangulation & Dag
    std::vector<Triangle*> triangles; /**< vector of pointer of Triangle, that rappresent the triangulation */

    // Nodes of my Dag
    std::vector<DagNode*> dagNodes; /**< vector of pointer of DagNode, that rappresent the Dag data structure */

    // This attributes is for validation process not for the triangulation
    std::map<Point2Dd, int> map;  /**< this map associates the Point2Dd with his index of vector of points. */
    Array2D<unsigned int> validTriangles;  /**< counter of validTriangles (triangle with isDeleted == false) */
    int countValidTriangles = 0;  /**< vector of pointer of DagNode, that rappresent the Dag data structure */

    // Add a point inside my triangulation
    bool addPoint(const Point2Dd& p);

    // Clean triangulation
    void cleanDelaunayTriangulation();

    // Load points from a file
    void loadPointFromVector(const std::vector<Point2Dd>& vectorPoints);

    // Set bounding triangle points
    void setBoundingTrianglePoints(const Point2Dd& p1, const Point2Dd& p2, const Point2Dd& p3);

    // Check if a point lie in a line
    bool pointLieInALine(const Point2Dd& p, const Point2Dd& a, const Point2Dd& b);

    // Check if an Edge is legal
    void legalizeEdge(Point2Dd* pr, Point2Dd* pi, Point2Dd* pj, Triangle* tr);

    // Edge Flip of two triangle
    void edgeFlip(Triangle* tr1, Triangle* tr2, Point2Dd* pr, Point2Dd* pi, Point2Dd* pj);

    // Create a new triangle, set his dag node and his pointer of his dag node father
    Triangle* generateTriangle(Point2Dd* p, Point2Dd* p1, Point2Dd* p2, DagNode* dagNodeFather);

    // Create a new triangle, set his dag node and his two pointer of two dag node father
    Triangle* generateTriangle(Point2Dd* p, Point2Dd* p1, Point2Dd* p2, DagNode* dagNodeFather1, DagNode* dagNodeFather2);

    // Manage the triangulation if a point lie inside a triangle or in one of his edge
    void pointLieInsideTriangle(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather);
    void pointLieAB(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather);
    void pointLieBC(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather);
    void pointLieCA(Point2Dd* pr, Triangle* triangleFather, DagNode* dagFather);

    // This method are used for the validation not for the triangulation
    std::vector<Point2Dd> getPointsForValidation();
    void generateTrianglesForValidation();
    int countNumberOfTriangles() const;
    int getCountValidTriangle() const;
    Array2D<unsigned int> getValidTriangles() const;

    // Return my triangulation
    std::vector<Triangle*> getTriangles();

};

#endif // DELAUANYTRIANGULATIONCORE_H
