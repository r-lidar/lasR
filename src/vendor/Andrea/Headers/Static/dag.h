#ifndef DAG_H
#define DAG_H

//#include <viewer/objects/objects.h>
#include <Andrea/Headers/Static/adjacencies.h>
#include <Andrea/Headers/DataStructures/dagnode.h>
#include <Andrea/Headers/point2d.h>


class DagNode;

/**
 * @brief The Dag class
 * Dag class is composed by only static method. It is use to set, check and navigate the DagNode
 * that composed the Dag data structure.
 */
class Dag {

public:
    Dag();

    static bool pointInTriangle (const Point2Dd& p, const Point2Dd& a, const Point2Dd& b, const Point2Dd& c);

    static DagNode *navigate(DagNode *dagNode, const Point2Dd& p);

    static void addNode(DagNode* node, DagNode* dagNodeFather);

};

#endif // DAG_H
