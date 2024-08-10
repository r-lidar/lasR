#ifndef DELAUNAY_H
#define DELAUNAY_H

#include <vector>

#include "utils.h"
#include "geometry.h"

class Triangulation
{
public:
  Triangulation(const std::vector<Vec2>& points, int numP, bool logSearch);
  ~Triangulation();
  bool delaunayInsertion(const Vec2& point, int tri_index = -1);
  int findContainerTriangleLinearSearch(const Vec2& p) const;
  int findContainerTriangleSqrtSearch(const Vec2& p, int prop = -1) const;
  double triangleArea(int f) const;

  Vertex *vertices;
  Triangle *triangles;
  int vcount = 0;
  int tcount = 0;

private:
  void addPointInside(const Vec2& point,int);
  void addPointInEdge(const Vec2& point, int t1, int t2);
  void addPointInEdge(const Vec2& point, int t);

  bool legalize(int t);
  bool legalize(int t1, int t2);
  bool flip(int t1, int t2);

  bool isInside(int t, Vec2) const; //checks if a Vec2 is inside the triangle in the index t
  bool isInside(int t, int v) const;
  bool isInEdge(int t, Vec2) const; //checks if a Vec2 is in a edge of a triangle

  bool isConvexBicell(int t1, int t2); // Checks if a bicell is convex
  bool isCCW(int f) const; // check if a triangle, in the position f of the triangles array, is ccw
  bool areConnected(int,int) const;

  void remem(); // checks if more memory is needed, and if it is needed, allocates more memory.

  //bool sanity(int);
  //bool integrity(int t);
  //bool validTriangle(int t);

  //void print(); // prints the triangulation to standard output
  //void print_ind(); // prints connectivity

  int maxTriangles;
  int maxVertices;
  int incount = 0;

  bool doLogSearch = true;

  float a;
  Vec2 p0,p1,p2,p3;
};

#endif
