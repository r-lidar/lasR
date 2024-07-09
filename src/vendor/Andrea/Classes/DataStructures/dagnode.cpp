#include <Andrea/Headers/DataStructures/dagnode.h>

DagNode::DagNode(){}

DagNode::~DagNode(){ }

DagNode::DagNode(Triangle* tr){
    this->tr = tr;
}

/**
 * Getter & Setter for his attributes
 */

DagNode* DagNode::getChildA() const{
    return this->childA;
}

DagNode* DagNode::getChildB() const{
    return this->childB;
}

DagNode* DagNode::getChildC() const{
    return this->childC;
}

Triangle* DagNode::getTriangle() const{
    return this->tr;
}

void DagNode::setChildA(DagNode* dn){
    this->childA = dn;
}

void DagNode::setChildB(DagNode* dn){
    this->childB = dn;
}

void DagNode::setChildC(DagNode* dn){
    this->childC = dn;
}

void DagNode::setTriangle(Triangle* tr){
    this->tr = tr;
}

/**
 * @brief DagNode::operator ==
 * @param dn
 * @return true if two DagNode are equals, else otherwise
 */
bool DagNode::operator == (const DagNode& dn){
   if( this->tr == dn.getTriangle() )
     return true;
   else
     return false;
}
