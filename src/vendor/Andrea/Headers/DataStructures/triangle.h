#ifndef TRIANGLE_H
#define TRIANGLE_H
//#include <viewer/objects/objects.h>
#include <Andrea/Headers/Static/adjacencies.h>
#include <Andrea/Headers/Static/dag.h>
#include <Andrea/Headers/point.h>
#include <Andrea/Headers/point2d.h>

class DagNode;

/**
 * @brief The Triangle class
 * This class rappresent a Triangle.
 * A triangle is composed by 3 vertex, his Dagnode
 * and three adjacent triangle.
 */
class Triangle{

private:
    Point2Dd* vA; /**< A pointer to his first vertex */
    Point2Dd* vB; /**< A pointer to his second vertex */
    Point2Dd* vC; /**< A pointer to his third vertex */

    DagNode* dagNode = nullptr; /**< A pointer to his DagNode */

    Triangle* triangleAdjacentA = nullptr; /**< A pointer to his first adjacent triangle */
    Triangle* triangleAdjacentB = nullptr; /**< A pointer to his second adjacent triangle */
    Triangle* triangleAdjacentC = nullptr; /**< A pointer to his third adjacent triangle */

    bool isDeleted = false;  /**< isDeleted will be true if a triangle is deleted from triangulation, but still remain valid for is dagNode reference */

public:
    Triangle();
    Triangle(Point2Dd* vA, Point2Dd* vB, Point2Dd* vC);
    ~Triangle();

    // Getter & setter per i 3 punti del triangolo
    Point2Dd* getA() const;
    Point2Dd* getB() const;
    Point2Dd* getC() const;

    void setA(Point2Dd* vA);
    void setB(Point2Dd* vB);
    void setC(Point2Dd* vC);

    // Getter & Setter for dagNode
    DagNode* getDagNode() const;
    void setDagNode(DagNode* dn);

    // Getter & Setter for adjacencies
    Triangle* getTriangleAdjacentA() const;
    Triangle* getTriangleAdjacentB() const;
    Triangle* getTriangleAdjacentC() const;
    void setTriangleAdjacentA(Triangle* trAdjA);
    void setTriangleAdjacentB(Triangle* trAdjB);
    void setTriangleAdjacentC(Triangle* trAdjC);

    bool checkPointIsVertexOfTriangle(const Point2Dd& p);

    Point2Dd* getThirdPoint(const Point2Dd& p1, const Point2Dd& p2);

    // Getter & setter per isDeleted
    void setIsDeleted(bool b);
    bool getIsDeleted() const;

    bool operator ==(const Triangle& tr);
};

#endif // TRIANGLE_H
