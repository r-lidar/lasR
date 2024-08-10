#ifndef DELAUNAY_H
#define DELAUNAY_H

#define __H_BREAKPOINT__ __asm__("int $3")
// #define __H_ASSERT__(condition) if(!(condition)) {__H_BREAKPOINT__; return false;}
#define __H_ASSERT__(condition) if(!(condition)) { std::cout << __LINE__ << std::endl; triangles[t1] = pt1; triangles[t2] = pt2; return false;}
// #define __H_BREAK_ASSERT__(condition) if(!(condition)) {__H_BREAKPOINT__; assert(condition);}
#define __H_BREAK_ASSERT__(condition)

#include <vector>
#include <set>
#include <unordered_set>

#include "utils.h"

class Triangle;

class Vertex {
public:
    Vertex(Vec2 v, int t);
    Vertex();
    Vertex(Vec2);
    Vec2 pos;
    int tri_index;
    void print();
};

class Triangle {
public:
    Triangle(int v1,int v2,int v3,int t1,int t2,int t3);
    Triangle();
    int v[3]; // indices to the vertices vector
    int t[3]; // indices to the triangles vector
};

class Triangulation {
public:
    Triangulation(std::vector<Vec2> points, int numP, bool logSearch);
    ~Triangulation();

    Vertex *vertices;
    Triangle *triangles;
    double *lengths;

    bool delaunayInsertion(Vec2 point, int tri_index = -1);
    void addPointInside(Vec2 point,int);
    void addPointInEdge(Vec2 point, int t1, int t2);
    void addPointInEdge(Vec2 point, int t);
    bool flip(int t1, int t2);
    int findContainerTriangleLinearSearch(Vec2 p);
    int findContainerTriangleSqrtSearch(Vec2 p, int prop = -1);
    int findCHTriangle(int guess);

    std::set<int> getNeighbours(int index);
    std::set<int> getNeighbourTriangles(int index);
    void movePoint(int index, Vec2 delta);
    void whichTriangle();
    int closestNeighbour(int index);
    float closestNeighbourDistance(int index);
    std::pair<int,int> getVerticesSharedByTriangles(int t1, int t2);

    double triangleArea(int f);

    bool isInside(int t, Vec2); //checks if a Vec2 is inside the triangle in the index t
    bool isInside(int t, int v);
    bool isInEdge(int t, Vec2); //checks if a Vec2 is in a edge of a triangle
    bool legalize(int t);
    bool legalize(int t1, int t2);
    bool isConvexBicell(int t1, int t2); // Checks if a bicell is convex

    bool areConnected(int,int);
    bool frontTest(int);
    bool sanity(int);
    bool allSanity();
    bool isCCW(int f); // check if a triangle, in the position f of the triangles array, is ccw
    bool integrity(int t);
    bool validTriangle(int t);
    bool next(int t0,int t1); //checks if two triangles are next to each other

    void print(); // prints the triangulation to standard output
    void print_ind(); // prints connectivity

    int maxTriangles;
    int maxVertices;
    int vcount = 0;
    int tcount = 0;
    int incount = 0;
    int edgecount = 0;
    int oedgecount = 0;
    int point_being_moved = -1;

    bool doLogSearch = true;
    bool doSorting = true;

    float a;
    Vec2 p0,p1,p2,p3;

    // ---------------------- T2 --------------------
    int calcLongestEdge(int t); // gives the int corresponding to the opposite vertex of the longest edge of a triangle
    std::vector<int> calcLepp(int t);
    void centroidAll(double angle);
    void addCentroids();
    void longestEdgeBisect(int t);
    void remem(); // checks if more memory is needed, and if it is needed, allocates more memory.

    // Kinetic delaunay
    Vec2* velocity;
    float radius = 0.5;
    struct RemovedVertex{
        int t[3]; // indices to the triangles array
        int v; // index to the vertices array
    };
    RemovedVertex removeVertex(int v); // removes a vertex from a triangulation, and returns it
    void reAddVertex(RemovedVertex rmvx); // add a vertex when it was previously deleted
    std::set<int> getFRNN(int v, float r);
    std::set<std::pair<int,double>> getFRNN_distance(int v, float r);
    std::set<std::pair<int,double>> getFRNN_distance_exp(int v, float r);
    std::vector<std::vector<std::pair<int,double>>> get_all_FRNN(float r);
};

#endif
