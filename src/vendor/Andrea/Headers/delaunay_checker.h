/**
 * @author    Stefano Nuvoli (stefano.nuvoli@gmail.com)
 * @copyright Stefano Nuvoli 2017.
 */
#ifndef TRIANGULATIONGEOMETRY_H
#define TRIANGULATIONGEOMETRY_H

#include <Andrea/Headers/point2d.h>
//#include <common/utils.h>
#include <Andrea/Headers/arrays.h>

namespace DelaunayTriangulation {

namespace Checker {

bool isPointLyingInCircle(const Point2Dd& a, const Point2Dd& b, const Point2Dd& c, const Point2Dd& p, bool includeEndpoints);

bool isDeulaunayTriangulation(const std::vector<Point2Dd>& points, const Array2D<unsigned int>& triangles);

}

}

#endif // TRIANGULATIONGEOMETRY_H
