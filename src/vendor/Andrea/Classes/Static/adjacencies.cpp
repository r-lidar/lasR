#include <Andrea/Headers/Static/adjacencies.h>

Adjacencies::Adjacencies() {}

/**
 * @brief Adjacencies::setAdjacencies
 * @param tr1 is a pointer to the triangle to which we will set the adjacences
 * @param tr2 is a pointer to the first adjacent triangle of tr1
 * @param tr3 is pointer to the second adjacent triangle of tr1
 * @param trFather is a pointer to the triangle father of tr1, the triangle that contain tr1
 * Calculate the adjacencies for triangle tr1. tr2 and tr3 will be adiacients a priori,
 * while for the third adjacent check the triangle parent.
 */
void Adjacencies::setAdjacencies(Triangle* tr1, Triangle* tr2, Triangle* tr3, Triangle* trFather){

    Adjacencies::setAdjacency(tr1, tr2); // AC
    Adjacencies::setAdjacency(tr1, tr3); // AB


    // Check the father of tr1 and check his adjacencies
    if(trFather == nullptr)
        return;

    // First
    if(trFather->getTriangleAdjacentA() != nullptr){
        if((isAdjacencies(*tr1, *trFather->getTriangleAdjacentA() ) == true) && (trFather->getTriangleAdjacentA()->getIsDeleted() == false)){
            Adjacencies::setAdjacency(tr1, trFather->getTriangleAdjacentA() );
            Adjacencies::overrideAdjacency(trFather->getTriangleAdjacentA(),  tr1);
        }
    }

    // Second
    if(trFather->getTriangleAdjacentB() != nullptr){
        if((isAdjacencies(*tr1, *trFather->getTriangleAdjacentB() ) == true) && (trFather->getTriangleAdjacentB()->getIsDeleted() == false)){
            Adjacencies::setAdjacency(tr1, trFather->getTriangleAdjacentB() );
            Adjacencies::overrideAdjacency(trFather->getTriangleAdjacentB(), tr1);
        }
    }

    // Third
    if(trFather->getTriangleAdjacentC() != nullptr){
        if((isAdjacencies(*tr1, *trFather->getTriangleAdjacentC() ) == true) && (trFather->getTriangleAdjacentC()->getIsDeleted() == false)){
            Adjacencies::setAdjacency(tr1, trFather->getTriangleAdjacentC() );
            Adjacencies::overrideAdjacency(trFather->getTriangleAdjacentC(), tr1);
        }
    }

}

/**
 * @brief Adjacencies::setAdjacenciesAfterFlip
 * @param tr1 is a pointer to the triangle to which we will set the adjacences
 * @param tr2 is a Pointer to the first adjacent triangle of tr1
 * @param tr1Father is a pointer to the first triangle father of tr1, the triangle that contain tr1
 * @param tr2Father is a pointer to the second triangle father of tr1, the triangle that contain tr1
 * Calculate the adjacencies for triangle tr1, after it was done a Edge Flip.
 * So the function check two parents: tr1Father and tr2Father and set the second and third adjacencies
 */
void Adjacencies::setAdjacenciesAfterFlip(Triangle* tr1, Triangle* tr2, Triangle* tr1Father, Triangle* tr2Father){

    Adjacencies::setAdjacency(tr1, tr2);

    // First father
    if( (isAdjacencies(*tr1, *tr1Father->getTriangleAdjacentA() ) ) && (tr1Father->getTriangleAdjacentA()->getIsDeleted() == false)){
        Adjacencies::setAdjacency(tr1, tr1Father->getTriangleAdjacentA() );
        Adjacencies::overrideAdjacency(tr1Father->getTriangleAdjacentA(),  tr1);
    }

    if( (isAdjacencies(*tr1, *tr1Father->getTriangleAdjacentB() ) ) && (tr1Father->getTriangleAdjacentB()->getIsDeleted() == false)){
        Adjacencies::setAdjacency(tr1, tr1Father->getTriangleAdjacentB() );
        Adjacencies::overrideAdjacency(tr1Father->getTriangleAdjacentB(),  tr1);
    }

    if( (isAdjacencies(*tr1, * tr1Father->getTriangleAdjacentC() ) ) && ( tr1Father->getTriangleAdjacentC()->getIsDeleted() == false)){
        Adjacencies::setAdjacency(tr1,  tr1Father->getTriangleAdjacentC() );
        Adjacencies::overrideAdjacency( tr1Father->getTriangleAdjacentC(),  tr1);
    }

    // Second father
    if( (isAdjacencies(*tr1, *tr2Father->getTriangleAdjacentA() ) ) && (tr2Father->getTriangleAdjacentA()->getIsDeleted() == false)){
        Adjacencies::setAdjacency(tr1, tr2Father->getTriangleAdjacentA() );
        Adjacencies::overrideAdjacency(tr2Father->getTriangleAdjacentA(),  tr1);
    }

    if( (isAdjacencies(*tr1, *tr2Father->getTriangleAdjacentB() ) ) && (tr2Father->getTriangleAdjacentB()->getIsDeleted() == false)){
        Adjacencies::setAdjacency(tr1, tr2Father->getTriangleAdjacentB() );
        Adjacencies::overrideAdjacency(tr2Father->getTriangleAdjacentB(),  tr1);
    }

    if( tr2Father->getTriangleAdjacentC() != nullptr ){
        if( (isAdjacencies(*tr1, *tr2Father->getTriangleAdjacentC() ) ) && (tr2Father->getTriangleAdjacentC()->getIsDeleted() == false)){
            Adjacencies::setAdjacency(tr1, tr2Father->getTriangleAdjacentC() );
            Adjacencies::overrideAdjacency(tr2Father->getTriangleAdjacentC(),  tr1);
        }
    }

}

/**
 * @brief Adjacencies::isAdjacencies
 * @param tr1 is the first triangle to check
 * @param tr2 is the second triangle to check
 * @return true is tr1 and tr1 are adjacent, false otherwise
 * Check if two triangles are adjacent trying all possible combinations, and return true if
 * the triangles are adjacent.
 */
bool Adjacencies::isAdjacencies(const Triangle& tr1, const Triangle& tr2){

    if(
        // AB
        ( (tr1.getA() == tr2.getA() || tr1.getB() == tr2.getA() || tr1.getC() == tr2.getA()) &&
          (tr1.getA() == tr2.getB() || tr1.getB() == tr2.getB() || tr1.getC() == tr2.getB()) ) )
            return true;

        // BC
       if ( (tr1.getA() == tr2.getB() || tr1.getB() == tr2.getB() || tr1.getC() == tr2.getB()) &&
          (tr1.getA() == tr2.getC() || tr1.getB() == tr2.getC() || tr1.getC() == tr2.getC()) )
           return true;

        // CA
        if( (tr1.getA() == tr2.getC() || tr1.getB() == tr2.getC() || tr1.getC() == tr2.getC()) &&
          (tr1.getA() == tr2.getA() || tr1.getB() == tr2.getA() || tr1.getC() == tr2.getA()) )
            return true;

    return false;
}

/**
 * @brief Adjacencies::isAdjacenciesForTwoPoints
 * @param tr is the triangle to which we will check the adjacences
 * @param p1 is the first vertex to check
 * @param p2 is the second vertex to check
 * @return True if tr have p1 and p2 as verticies, else otherwise
 * Similar to isAdjacencies, but this function check if a triangle have two specific verticies
 */
bool Adjacencies::isAdjacenciesForTwoPoints(const Triangle& tr, const Point2Dd& p1, const Point2Dd& p2){
    if( (*tr.getA() == p1 && *tr.getB() == p2) || (*tr.getB() == p1 && *tr.getA() == p2) ) // AB
        return true;

    else if( (*tr.getA() == p1 && *tr.getC() == p2) || (*tr.getC() == p1 && *tr.getA() == p2) ) // AC
        return true;

    else if( (*tr.getB() == p1 && *tr.getC() == p2) || (*tr.getC() == p1 && *tr.getB() == p2) ) // BC
        return true;
    else
        return false;

}

/**
 * @brief Adjacencies::setAdjacency
 * @param triangle is a pointer to the triangle to which we will set the adjacent
 * @param adjTriangle is a pointer to the adjacent triangle
 * Set adjTriangle as a adjacent of triangle (settings his attribute) in the
 * first free slot.
 */
void Adjacencies::setAdjacency(Triangle* triangle, Triangle* adjTriangle){

    // Set Adjacencies A
    if(triangle->getTriangleAdjacentA() == nullptr){
        triangle->setTriangleAdjacentA(adjTriangle);
        return;
    }

    // Set Adjacencies B
    if(triangle->getTriangleAdjacentB() == nullptr){
        triangle->setTriangleAdjacentB(adjTriangle);
        return;
    }

    // Set Adjacencies C
    if(triangle->getTriangleAdjacentC() == nullptr){
        triangle->setTriangleAdjacentC(adjTriangle);
        return;
    }
}

/**
 * @brief Adjacencies::overrideAdjacency
 * @param triangle is a pointer to the triangle to which we will set the adjacent
 * @param adjTriangle is a pointer to the adjacent triangle
 * Overrride the adjacency when a new triangle is added. It will be called inside
 * setAdjacencies & setAdjacenciesAfterFlip;
 * If adjTriangle has the same vertex of one of the three adjacencies of *triangle, that attribute
 * will be override with the new adjacentTriangle pointer.
 */
void Adjacencies::overrideAdjacency(Triangle* triangle, Triangle* adjTriangle){

    if( (triangle->getTriangleAdjacentA()->getIsDeleted() == true) && (Adjacencies::isAdjacencies(*adjTriangle, *triangle->getTriangleAdjacentA() ) == true) )
        triangle->setTriangleAdjacentA(adjTriangle);

    if(triangle->getTriangleAdjacentB() != nullptr){
        if( (triangle->getTriangleAdjacentB()->getIsDeleted() == true) && (Adjacencies::isAdjacencies(*adjTriangle, *triangle->getTriangleAdjacentB() ) == true) )
            triangle->setTriangleAdjacentB(adjTriangle);
    }

    if(triangle->getTriangleAdjacentC() != nullptr){
        if( (triangle->getTriangleAdjacentC()->getIsDeleted() == true) && (Adjacencies::isAdjacencies(*adjTriangle, *triangle->getTriangleAdjacentC() ) == true) )
            triangle->setTriangleAdjacentC(adjTriangle);
    }

}

/**
 * @brief Adjacencies::getTriangleAdjacentByTwoPoints
 * @param tr is a pointer to the triangle to which we will check the adjacencies
 * @param a is the first vertex to check
 * @param b is the second vertex to check
 * @return the triangle that shared the two verticies a and b
 * Given a pointer to a triangle, this method check if there are an adjacent triangle that share
 * the two verticies passed as a parameter. If there's not triangle,  it returns a null pointer
 */
Triangle* Adjacencies::getTriangleAdjacentByTwoPoints(Triangle* tr, const Point2Dd& a, const Point2Dd& b){

    if(Adjacencies::isAdjacenciesForTwoPoints(*tr->getTriangleAdjacentA(), a, b))
        return tr->getTriangleAdjacentA();

    else if(Adjacencies::isAdjacenciesForTwoPoints(*tr->getTriangleAdjacentB(), a, b))
        return tr->getTriangleAdjacentB();

    else if(tr->getTriangleAdjacentC() != nullptr){

        if(Adjacencies::isAdjacenciesForTwoPoints(*tr->getTriangleAdjacentC(), a, b))
            return tr->getTriangleAdjacentC();
    }

    return nullptr;
}
