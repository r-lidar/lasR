/**
 * @author    Stefano Nuvoli (stefano.nuvoli@gmail.com)
 * @copyright Stefano Nuvoli 2017.
 */
#include "Andrea/Headers/delaunay_checker.h"

#include <eigen3/Eigen/Dense>

namespace DelaunayTriangulation {

namespace Checker {

/**
 * @brief Check if a point lies inside a circle passing for three points
 * @param[in] t1 First coordinate of the circle
 * @param[in] t2 Third coordinate of the circle
 * @param[in] t3 Second coordinate of the circle
 * @param[in] p Input point
 * @param[in] includeEndpoints True if we want to include the endpoints
 * @return True if the point lies inside the circle
 */
bool isPointLyingInCircle(const Point2Dd& a, const Point2Dd& b, const Point2Dd& c, const Point2Dd& p, bool includeEndpoints) {
    Eigen::Matrix4d A;

    /*
     * std::cout << "Punti alla funzione" << std::endl;
    std::cout << "p1 " << a.x() << " " << a.y()  << std::endl;
    std::cout << "p2 " << b.x() << " " << b.y()  << std::endl;
    std::cout << "p3 " << c.x() << " " << c.y()  << std::endl;
    std::cout << "p " << p.x() << " " << p.y()  << std::endl;
    */
    A << a.x(), a.y(), a.x()*a.x() + a.y()*a.y(), 1,
            b.x(), b.y(), b.x()*b.x() + b.y()*b.y(), 1,
            c.x(), c.y(), c.x()*c.x() + c.y()*c.y(), 1,
            p.x(), p.y(), p.x()*p.x() + p.y()*p.y(), 1;

    if (includeEndpoints) {
        return (A.determinant() >= -std::numeric_limits<double>::epsilon());
    }
    else {
        return (A.determinant() > 0);
    }
}

/**
 * @brief Check if the triangulation is a Delaunay triangulation (brute force, O(n^2))
 * @param points Vector of points
 * @param triangles Vector of triangles (represented by a vector of three points)
 * @return true if the triangulation is a delaunay triangulations, false otherwise
 */
bool isDeulaunayTriangulation(const std::vector<Point2Dd>& points, const Array2D<unsigned int>& triangles) {
    assert(triangles.getSizeY() == 3);

    unsigned int n = triangles.getSizeX();
    bool isDelaunay = true;
    #pragma omp parallel for
    for (unsigned int i = 0; i < n; i++) {
        const Point2Dd& a = points[triangles(i,0)]; // points of the triangle
        const Point2Dd& b = points[triangles(i,1)];
        const Point2Dd& c = points[triangles(i,2)];

        for (const Point2Dd& p : points) { // for all the points different by a, b, c
            if (!isDelaunay) break;
            if (p != a && p != b && p != c) {
                if (isPointLyingInCircle(a,b,c,p,false)) { // the point p must be outside the circle formed by a, b, c
                    isDelaunay = false;
                    break;
                }
            }
        }
    }
    return isDelaunay;
}


}

}
