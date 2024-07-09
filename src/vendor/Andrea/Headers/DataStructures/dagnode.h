#ifndef DAGNODE_H
#define DAGNODE_H

//#include <viewer/objects/objects.h>
#include <Andrea/Headers/DataStructures/triangle.h>

class Triangle;

/**
 * @brief The DagNode class
 * This class rappresents a a Dag Node. A node have 3 children
 * and a pointer to his triangle on the triangulation vector
 */
class DagNode{

private:
    Triangle* tr = nullptr; /**< A pointer to his triangle */
    DagNode* childA = nullptr; /**< A pointer to his first child */
    DagNode* childB = nullptr; /**< A pointer to his second child */
    DagNode* childC = nullptr; /**< A pointer to his third child */

public:
    DagNode();
    DagNode(Triangle* tr);
    ~DagNode();

    // Getter & Setter
    DagNode* getChildA() const;
    DagNode* getChildB() const;
    DagNode* getChildC() const;
    Triangle* getTriangle() const;

    void setChildA(DagNode* dn);
    void setChildB(DagNode* dn);
    void setChildC(DagNode* dn);
    void setTriangle(Triangle* tr);

    bool operator ==(const DagNode& dn);
};

#endif // DAGNODE_H
