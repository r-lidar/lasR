#include <iostream>

#include "pred3d.h"
#include "utils.h"

#include "delaunay.h"
#include "constants.h"

static bool initialized = false;

#include <queue>
#include <stack>
#include <algorithm>
#include <cmath>

bool Triangulation::isInside(int t, Vec2 p)
{
    if (t == -1)
      return false;

    if ((triangles[t].v[0] == -1) && (triangles[t].v[1] == -1) && (triangles[t].v[2] == -1))
      return false;

    Vec2 p1 = vertices[triangles[t].v[0]].pos;
    Vec2 p2 = vertices[triangles[t].v[1]].pos;
    Vec2 p3 = vertices[triangles[t].v[2]].pos;

    return (orient2d(&(p1.x),&(p2.x),&(p.x))>0) &&
           (orient2d(&(p2.x),&(p3.x),&(p.x))>0) &&
           (orient2d(&(p3.x),&(p1.x),&(p.x))>0);
}

bool Triangulation::isInside(int t, int v)
{
    if (t==-1) return 0;

    return isInside(t, vertices[v].pos);
}

bool Triangulation::validTriangle(int t){ // checks for repeated vertices or triangles
    return true;
    if(t==-1)return true;
    bool res = true;
    if(t>=tcount)res=false;
    if(t<-1)res=false;
    int menosunos = 0;
    for(int i=0;i<3;i++)menosunos+=triangles[t].t[i]==-1?1:0;
    if(menosunos<2)if(
        triangles[t].t[0]==triangles[t].t[1] ||
        triangles[t].t[0]==triangles[t].t[2] ||
        triangles[t].t[1]==triangles[t].t[2]
    ) return false;
    for(int i=0;i<3;i++)if(triangles[t].t[i]<-1)res=false;
    for(int i=0;i<3;i++)if(triangles[t].t[i]>tcount)res=false;
    for(int i=0;i<3;i++)if(triangles[t].v[i]<0)res=false;
    for(int i=0;i<3;i++)if(triangles[t].v[i]>vcount)res=false;

    for(int i=0;i<3;i++){
        if(triangles[t].v[i]==triangles[t].v[(i+1)%3])res=false;
        if(triangles[t].t[i]==triangles[t].t[(i+1)%3] && triangles[t].t[i]!=-1)res=false;
    }
    // if(!res) __H_BREAKPOINT__;
    return res;
}

bool Triangulation::isInEdge(int t, Vec2 p){
    if(t==-1)return false;
    if(
        triangles[t].v[0] == -1 &&
        triangles[t].v[1] == -1 &&
        triangles[t].v[2] == -1
    ) return false;

    Vec2 p1 = vertices[triangles[t].v[0]].pos;
    Vec2 p2 = vertices[triangles[t].v[1]].pos;
    Vec2 p3 = vertices[triangles[t].v[2]].pos;

    return (orient2d(&(p1.x),&(p2.x),&(p.x))==0) &&
           (orient2d(&(p2.x),&(p3.x),&(p.x))==0) &&
           (orient2d(&(p3.x),&(p1.x),&(p.x))==0);
    // b-a goes from a to b
    // Vec2 a1 = p1-p2; Vec2 b1 = p-p2;
    // Vec2 a2 = p2-p3; Vec2 b2 = p-p3;
    // Vec2 a3 = p3-p1; Vec2 b3 = p-p1;
    // if(mightBeLeft(p2-p1,p-p1) && mightBeLeft(p3-p2,p-p2) && mightBeLeft(p1-p3,p-p3))
    // if( (a1[0]>=b1[0] && a1[1]>=b1[1]) || (a2[0]>=b2[0] && a2[1]>=b2[1]) || (a3[0]>=b3[0] && a3[1]>=b3[1]) ) return true;
    // return false;
}

Triangulation::Triangulation(const std::vector<Vec2>& points, int numP, bool logSearch = true) :  doLogSearch(logSearch), doSorting(false) {
    if(!initialized){
        exactinit();
        initialized = true;
    }
    double minx = 10000000;
    double miny = 10000000;
    double maxx =-10000000;
    double maxy =-10000000;

    //if(doSorting)
    //    std::sort(points.begin(),points.end(),[](Vec2 a,Vec2 b){return (a.x==b.x?a.y<b.y:a.x<b.x);});

    for(auto p: points){
        minx = std::min(minx,p.x);
        miny = std::min(miny,p.y);
        maxx = std::max(maxx,p.x);
        maxy = std::max(maxy,p.y);
    }

    a = std::max(maxx-minx,maxy-miny);

    p0 = Vec2(minx,miny) + Vec2(-a/10,-a/10);
    p1 = p0 + Vec2(a+2*a/10,0);
    p2 = p0 + Vec2(a+2*a/10,a+2*a/10);
    p3 = p0 + Vec2(0,a+2*a/10);

    maxVertices = numP+6;
    maxTriangles = numP*2+7;
    vertices = new Vertex[maxVertices]; // num of vertices
    triangles = new Triangle[maxTriangles]; // 2(n+6) - 2 - 3 = 2n+7 // num of faces
    lengths = new double[maxTriangles*3]; // One for every triangle in an edge

    vertices[0] = Vertex(Vec2(-10000000,-1000000));
    vertices[1] = Vertex(Vec2( 10000000,-10000000));
    vertices[2] = Vertex(Vec2( 10000000, 10000000));
    vertices[3] = Vertex(Vec2(-10000000, 10000000));

    triangles[0] = Triangle(0,1,2,-1,1,-1);
    triangles[1] = Triangle(0,2,3,-1,-1,0);

    vcount = 4;
    tcount = 2;

/*#if ASSERT_PROBLEMS
    assert(isCCW(0)&&isCCW(1));
    assert(frontTest(0));
#endif*/

    for(int i=0;i<(int)points.size();i++){
        delaunayInsertion(points[i]);
    }
}

bool Triangulation::integrity(int t){ // checks that every t's neighbour has t as its neighbour
return true;
    int t0 = triangles[t].t[0];
    int t1 = triangles[t].t[1];
    int t2 = triangles[t].t[2];

    bool a=true,b=true,c=true;

    if(t0!=-1) a = (t==triangles[t0].t[0]) || (t==triangles[t0].t[1]) || (t==triangles[t0].t[2]);
    if(t1!=-1) b = (t==triangles[t1].t[0]) || (t==triangles[t1].t[1]) || (t==triangles[t1].t[2]);
    if(t2!=-1) c = (t==triangles[t2].t[0]) || (t==triangles[t2].t[1]) || (t==triangles[t2].t[2]);

    return (a&&b)&&c;
}

bool Triangulation::frontTest(int t){ // checks that every point is in the same index of a triangle that the triangle in front of it
    if(t==-1)return true;
    bool res = true;
    for(int i=0;i<3;i++){
        int v = triangles[t].v[i];
        int f = triangles[t].t[i];
        if(f!=-1)for(int j=0;j<3;j++){
            if(triangles[f].v[j]==v){
                res=false;
                std::cout << t << " " <<  v << " != " << f << " " << triangles[f].v[j] << std::endl;
            }
        }
    }
    return res;
}

void Triangulation::addPointInside(Vec2 v, int tri_index){
    remem();
    int f = tri_index;
    int f1 = tcount++;
    int f2 = tcount++;

    int p = vcount++;

    int p0 = triangles[f].v[0];
    int p1 = triangles[f].v[1];
    int p2 = triangles[f].v[2];
    int t1 = triangles[f].t[1];
    int t2 = triangles[f].t[2];

    triangles[f1] = Triangle(p,p2,p0,t1,f2,f);
    triangles[f2] = Triangle(p,p0,p1,t2,f,f1);

    if(t1!=-1){
        if(triangles[t1].t[0]==f)
        triangles[t1].t[0] = f1;
        if(triangles[t1].t[1]==f)
        triangles[t1].t[1] = f1;
        if(triangles[t1].t[2]==f)
        triangles[t1].t[2] = f1;
    }

    if(t2!=-1){
        if(triangles[t2].t[0]==f)
        triangles[t2].t[0] = f2;
        if(triangles[t2].t[1]==f)
        triangles[t2].t[1] = f2;
        if(triangles[t2].t[2]==f)
        triangles[t2].t[2] = f2;
    }

    triangles[f].v[0] = p;
    triangles[f].t[1] = f1;
    triangles[f].t[2] = f2;

    vertices[p] = Vertex(v,f);
}

int Triangulation::findContainerTriangleLinearSearch(Vec2 p){
    for(int i=0;i<tcount;i++){
        if(isInside(i,p)){
            return i;
        }
    }
    for(int i=0;i<tcount;i++){
        if(isInEdge(i,p)){
            return i;
        }
    }
    return -1;
}

int Triangulation::findContainerTriangleSqrtSearch(Vec2 p, int prop){

    if (prop < 0) prop = tcount-1;

    if(isInside(prop,p))return prop;
    if(isInEdge(prop,p))return prop;

    Vec2 v = (vertices[triangles[prop].v[0]].pos+vertices[triangles[prop].v[1]].pos+vertices[triangles[prop].v[2]].pos)/3.0;


    int t = prop;
    for(int i=0;i<3;i++){
        int f = triangles[t].t[i];
        if(f==-1)continue;
        Vec2 a = vertices[triangles[t].v[(i+1)%3]].pos;
        Vec2 b = vertices[triangles[t].v[(i+2)%3]].pos;
        // &(v)
        if(
            (orient2d(&(v.x),&(p.x),&(a.x))*orient2d(&(v.x),&(p.x),&(b.x))<0) &&
            (orient2d(&(a.x),&(b.x),&(p.x))*orient2d(&(a.x),&(b.x),&(v.x))<0)
            // (mightBeLeft(p-v,a-v) && mightBeLeft(b-v,p-v)) ||
            // (mightBeLeft(p-v,b-v) && mightBeLeft(a-v,p-v))
        ){
            return findContainerTriangleSqrtSearch(p,f);
        }
    }

    return -1;
}

bool Triangulation::delaunayInsertion(Vec2 p, int prop){

    remem();

    int tri_index = -1;
    if(doLogSearch) tri_index = findContainerTriangleSqrtSearch(p,prop);
    else tri_index = findContainerTriangleLinearSearch(p);

    //printf("Max vertice %d, tcount %d\n", maxVertices, tcount);
    //printf("tri index %d, v[0] = %d\n", tri_index, triangles[tri_index].v[0]);
    Vec2 a = vertices[triangles[tri_index].v[0]].pos;
    Vec2 b = vertices[triangles[tri_index].v[1]].pos;
    Vec2 c = vertices[triangles[tri_index].v[2]].pos;
    Vec2 points[] = {a,b,c};

    //printf("  Insertion in triangle %d (%.1lf, %.1lf), (%.1lf, %.1lf) (%.1lf, %.1lf)\n", tri_index, points[0][0], points[0][1], points[1][0], points[1][1], points[2][0], points[2][1]);

    for(int i=0;i<3;i++){ // we dont insert repeated points
        if((abs(p[0]-points[i][0])<IN_TRIANGLE_EPS) && (abs(p[1]-points[i][1])<IN_TRIANGLE_EPS))return false;
    }

    for(int i=0;i<3;i++){
        if(pointInSegment(p,points[(i+1)%3],points[(i+2)%3])){
            // insert a point in the i edge
            if(triangles[tri_index].t[i]==-1){
                addPointInEdge(p,tri_index);
                legalize(tri_index);
                legalize(tcount-1);
                legalize(tcount-2);
            }
            else{
                addPointInEdge(p,tri_index,triangles[tri_index].t[i]);
                for(int j=0;j<3;j++){
                    legalize(triangles[tri_index].t[i]);
                    legalize(tri_index);
                    legalize(tcount-1);
                    legalize(tcount-2);
                }
            }
            return true;
        }
    }

    if(tri_index!=-1){
        this->incount++;
        addPointInside(p,tri_index);
        int a = tri_index,b=tcount-1,c=tcount-2;
        legalize(a);
        legalize(b);
        legalize(c);
        return true;
    }
    return true;
}

// gets the pair of vertex indices shared by two triangles
std::pair<int,int> Triangulation::getVerticesSharedByTriangles(int t1, int t2){
/*#if ASSERT_PROBLEMS
    assert(areConnected(t1,t2));
#endif*/
    for(int i=0;i<3;i++){
        std::pair<int,int> v1 = std::make_pair(triangles[t1].v[i],triangles[t1].v[(i+1)%3]);
        for(int j=0;j<3;j++){
            std::pair<int,int> v2 = std::make_pair(triangles[t2].v[j],triangles[t2].v[(j+1)%3]);
            if(v1==v2)return v1;
            v2 = std::make_pair(triangles[t2].v[(j+1)%3],triangles[t2].v[j]);
            if(v1==v2)return v1;
        }
    }
    return std::make_pair(-1,-1);
}

bool Triangulation::legalize(int t){
    for(int i=0;i<3;i++)legalize(t,triangles[t].t[i]);
    return true;
}

bool Triangulation::legalize(int t1, int t2){
    if(t2==-1)return 1;
    if(t1==-1)return 1;
    if(!areConnected(t1,t2))return 1;
    int a[6];
    a[0] = triangles[t1].v[0];
    a[1] = triangles[t1].v[1];
    a[2] = triangles[t1].v[2];
    a[3] = triangles[t2].v[0];
    a[4] = triangles[t2].v[1];
    a[5] = triangles[t2].v[2];
    int b[8];

    b[0] = a[0];
    b[1] = a[1];
    b[2] = a[2];
    for(int i=3;i<6;i++){
        if((a[i]!=b[0]) && (a[i]!=b[1]) && (a[i]!=b[2])) b[3] = a[i];
    }
    b[4] = a[3];
    b[5] = a[4];
    b[6] = a[5];
    for(int i=0;i<3;i++){
        if((a[i]!=b[4]) && (a[i]!=b[5]) && (a[i]!=b[6])) b[7] = a[i];
    }

    if(isConvexBicell(t1,t2) &&
       (inCircle(vertices[b[0]].pos,vertices[b[1]].pos,vertices[b[2]].pos,vertices[b[3]].pos)>0 ||
        inCircle(vertices[b[4]].pos,vertices[b[5]].pos,vertices[b[6]].pos,vertices[b[7]].pos)>0)
    ) {
        bool p = flip(t1,t2);
        legalize(t1);
        legalize(t2);
        return p;
    }
    return 1;
}
void Triangulation::addPointInEdge(Vec2 v, int t0, int t1){
/*#if ASSERT_PROBLEMS
    assert(isCCW(t0)&&isCCW(t1));
    assert(integrity(t0)&&integrity(t1));
    assert(frontTest(t0)&&frontTest(t1));
    assert(areConnected(t0,t1));
    assert(sanity(t0));
    assert(sanity(t1));
    assert(next(t0,t1));
#endif*/
    remem();
    int p = vcount;
    vertices[vcount++] = Vertex(v);

    int t0_v = -1;
    int t1_v = -1;

    int f0,f3;
    int p1;

    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
            if(triangles[t0].t[i]==t1 && triangles[t1].t[j]==t0){
                t0_v = i;
                t1_v = j;
            }
        }
    }

/*#if ASSERT_PROBLEMS
    assert(t0_v!=-1);
    assert(t1_v!=-1);
    assert(triangles[t0].t[t0_v]==t1);
    assert(triangles[t1].t[t1_v]==t0);
#endif*/

    f0 = triangles[t0].t[(t0_v+1)%3];
    p1 = triangles[t0].v[(t0_v+2)%3];

    f3 = triangles[t1].t[(t1_v+2)%3];

    int t2 = tcount++;
    int t3 = tcount++;

    triangles[t2] = Triangle(p,p1,triangles[t0].v[t0_v],f0,t0,t3);
    if(f0!=-1)for(int i=0;i<3;i++)if(triangles[f0].t[i]==t0)triangles[f0].t[i] = t2;

    triangles[t3] = Triangle(p,triangles[t1].v[t1_v],p1,f3,t2,t1);
    if(f3!=-1)for(int i=0;i<3;i++)if(triangles[f3].t[i]==t1)triangles[f3].t[i] = t3;

    triangles[t0].v[(t0_v+2)%3] = p;
    triangles[t0].t[(t0_v+1)%3] = t2;
    triangles[t1].v[(t1_v+1)%3] = p;
    triangles[t1].t[(t1_v+2)%3] = t3;

    remem();

/*#if ASSERT_PROBLEMS
    assert(isCCW(t0));
    assert(isCCW(t1));
    assert(isCCW(t2));
    assert(isCCW(t3));
    assert(integrity(t0));
    assert(integrity(t1));
    assert(integrity(t2));
    assert(integrity(t3));
#endif*/
}

void Triangulation::addPointInEdge(Vec2 v, int t){
/*#if ASSERT_PROBLEMS
    assert(isCCW(t));
    // assert(isInEdge(t,v));
    assert(integrity(t));
#endif*/
    remem();

    int x = triangles[t].t[0] == -1 ? 0 : (triangles[t].t[1] == -1 ? 1 : 2);

    int f1 = triangles[t].t[x];
    int f2 = triangles[t].t[(x+1)%3];

    int p0 = triangles[t].v[(x+2)%3];
    int p1 = triangles[t].v[x];

    int t1 = tcount++;
    int p = vcount++;

    vertices[p] = Vertex(v);
    triangles[t1] = Triangle(p0,p1,p,t,f1,f2);
    triangles[t].v[(x+2)%3] = p;
    triangles[t].t[(x+1)%3] = t1;

    vertices[p].tri_index = t;

    if(f2!=-1){
        triangles[f2].t[0] = (triangles[f2].t[0] == t ? t1 : triangles[f2].t[0]);
        triangles[f2].t[1] = (triangles[f2].t[1] == t ? t1 : triangles[f2].t[1]);
        triangles[f2].t[2] = (triangles[f2].t[2] == t ? t1 : triangles[f2].t[2]);
    }
/*#if ASSERT_PROBLEMS
    assert(isCCW(t)&&isCCW(t1));
    assert(areConnected(t,t1));
    assert(integrity(t));
    assert(integrity(t1));
    assert(integrity(f2));
#endif*/

    remem();
}
bool Triangulation::areConnected(int t1, int t2){ // check if two triangles are neighbours
    if(t1==-1 || t2==-1) return true;
    bool one = false;
    bool two = false;
    for(int i=0;i<3;i++){
        if(triangles[t1].t[i]==t2)one=true;
        if(triangles[t2].t[i]==t1)two=true;
    };
    return one && two;
}
void Triangulation::print(){
    for(int i=0;i<tcount;i++){
        std::cout << "Triangle " << i << ":\n";
        std::cout << "P0: " << vertices[triangles[i].v[0]].pos.x << " " << vertices[triangles[i].v[0]].pos.y << std::endl;
        std::cout << "P1: " << vertices[triangles[i].v[1]].pos.x << " " << vertices[triangles[i].v[1]].pos.y << std::endl;
        std::cout << "P2: " << vertices[triangles[i].v[2]].pos.x << " " << vertices[triangles[i].v[2]].pos.y << std::endl;
    }
}
void Triangulation::print_ind(){
    for(int i=0;i<tcount;i++){
        std::cout << "Triangle " << i << ": ";
        std::cout << triangles[i].t[0] << " " << triangles[i].t[1] << " " << triangles[i].t[2] << std::endl;
    }
}
bool Triangulation::isCCW(int f){
    if(f==-1)return true;
    Vec2 p0 = vertices[triangles[f].v[0]].pos;
    Vec2 p1 = vertices[triangles[f].v[1]].pos;
    Vec2 p2 = vertices[triangles[f].v[2]].pos;
    return (orient2d(&(p0.x),&(p1.x),&(p2.x))>0);
    // if((crossa(p0,p1)+crossa(p1,p2)+crossa(p2,p0))>IN_TRIANGLE_EPS) return true;
    // return false;
}

double Triangulation::triangleArea(int f){
    if(f==-1)return true;
    Vec2 p0 = vertices[triangles[f].v[0]].pos;
    Vec2 p1 = vertices[triangles[f].v[1]].pos;
    Vec2 p2 = vertices[triangles[f].v[2]].pos;
    return (crossa(p0,p1)+crossa(p1,p2)+crossa(p2,p0));
}

bool Triangulation::flip(int t1, int t2){

    int i;
    if(triangles[t1].t[0] == t2) { i=0; }
    else if (triangles[t1].t[1] == t2) { i=1; }
    else { i=2; }

    int j;
    if(triangles[t2].t[0] == t1) { j=0; }
    else if (triangles[t2].t[1] == t1) { j=1; }
    else { j=2; }

    int p10 = triangles[t1].v[i];
    int p11 = triangles[t1].v[(i+1)%3];
    int p12 = triangles[t1].v[(i+2)%3];

    int f10 = triangles[t1].t[i];
    int f11 = triangles[t1].t[(i+1)%3];
    int f12 = triangles[t1].t[(i+2)%3];

    int p20 = triangles[t2].v[j];
    int p21 = triangles[t2].v[(j+1)%3];
    int p22 = triangles[t2].v[(j+2)%3];

    int f20 = triangles[t2].t[j];
    int f21 = triangles[t2].t[(j+1)%3];
    int f22 = triangles[t2].t[(j+2)%3];

    __H_BREAK_ASSERT__(f10==t2);
    __H_BREAK_ASSERT__(f20==t1);
    __H_BREAK_ASSERT__(p12==p21);
    __H_BREAK_ASSERT__(p22==p11);

    triangles[t1].v[0] = p11;
    triangles[t1].v[1] = p20;
    triangles[t1].v[2] = p10;

    vertices[p11].tri_index = t1;
    vertices[p20].tri_index = t1;
    vertices[p10].tri_index = t1;

    triangles[t1].t[0] = t2;
    triangles[t1].t[1] = f12;
    triangles[t1].t[2] = f21;

    triangles[t2].v[0] = p12;
    triangles[t2].v[1] = p10;
    triangles[t2].v[2] = p20;

    vertices[p12].tri_index = t2;

    triangles[t2].t[0] = t1;
    triangles[t2].t[1] = f22;
    triangles[t2].t[2] = f11;

    if(f11!=-1) {
        for(int k=0;k<3;k++) {
            if (triangles[f11].t[k] == t1) { triangles[f11].t[k] = t2; }
        }
    }

    if(f21!=-1) {
        for(int k=0;k<3;k++) {
            if (triangles[f21].t[k] == t2) { triangles[f21].t[k] = t1; }
        }
    }

    return true;
}

Triangulation::~Triangulation(){
    delete[] triangles;
    delete[] vertices;
    delete[] lengths;
}

bool Triangulation::sanity(int t){
    if(t==-1)return true;
    for(int i=0;i<3;i++){
        int count = 0;
        int f = triangles[t].t[i];
        if(f==-1)continue;
        for(int j=0;j<3;j++){
            for(int k=0;k<3;k++){
                if(triangles[t].v[k]==triangles[f].v[j])count++;
            }
        }
        if(count != 2) return false;
    }
    return true;
}

// -------------------------- T2 ------------------------------

int Triangulation::calcLongestEdge(int t){
    int longestEdge = 0;
    double longestEdgeSqrtLength = sqrtLength(vertices[triangles[t].v[1]].pos-vertices[triangles[t].v[2]].pos);
    for(int i=1;i<3;i++){
        double length = sqrtLength(vertices[triangles[t].v[(i+1)%3]].pos-vertices[triangles[t].v[(i+2)%3]].pos);
        if(length>longestEdgeSqrtLength){
            longestEdgeSqrtLength = length;
            longestEdge = i;
        }
    }
    return longestEdge;
}

std::vector<int> Triangulation::calcLepp(int t){
    std::vector<int> res;
    int currt = t;
    res.push_back(currt);
    while(true){
        int currt = triangles[res[res.size()-1]].t[calcLongestEdge(res[res.size()-1])];
        res.push_back(currt);
        if(res[res.size()-1] == -1) break;
        if((res.size()>2) && (res[res.size()-1] == res[res.size()-3])){
            res.pop_back();
            break;
        }
    }
    return res;
}

void Triangulation::centroidAll(double angle){
    bool global_do = true;
    while(global_do){
        global_do = false;
        for(int i=0;i<tcount;i++){
            bool flag_do = true;
            while(flag_do){
                flag_do = false;
                for(int j=0;j<3;j++){
                    Vec2 x = vertices[triangles[i].v[(j+1)%3]].pos;
                    Vec2 y = vertices[triangles[i].v[(j+2)%3]].pos;
                    Vec2 z = vertices[triangles[i].v[j]].pos;

                    Vec2 a = y-z;
                    Vec2 b = x-z;

                    double s_angle = std::acos(dot(a,b)/(mod(a)*mod(b))) * (180.0 / ID_PI);
                    if(s_angle <=  angle){
                        flag_do = true;
                        global_do = true;
                    }
                }

                if(!flag_do) continue;

                std::vector<int> le = calcLepp(i);
                int f1 = le[le.size()-1],f2 = le[le.size()-2];

                if(f1==-1 && f2==-1) continue;
                if(f1==-1){
                    longestEdgeBisect(f2);
                    continue;
                }
                if(f2==-1){
                    longestEdgeBisect(f1);
                    continue;
                }

                int points[4];
                points[0] = triangles[f1].v[0];
                points[1] = triangles[f1].v[1];
                points[2] = triangles[f1].v[2];
                points[3] = -1;
                for(int j=0;j<3;j++){
                    int p = triangles[f2].v[j];
                    bool isDiff = true;
                    for(int k=0;k<3;k++){
                        if(points[k]==p)isDiff=false;
                    }
                    if(isDiff)points[3] = p;
                }

                //if(points[3]==-1)continue;

                Vec2 p = (vertices[points[0]].pos+vertices[points[1]].pos+vertices[points[2]].pos+vertices[points[3]].pos)/4.0;

                delaunayInsertion(p);
            }
        }
    }
}

void Triangulation::addCentroids(){
    int actTcount = tcount;
    for(int i=0;i<actTcount;i++){
        Vec2 p = (vertices[triangles[i].v[0]].pos + vertices[triangles[i].v[1]].pos + vertices[triangles[i].v[2]].pos)/3.0;
        delaunayInsertion(p);
    }
}

void Triangulation::longestEdgeBisect(int t){
    int op1 = -1;
    for(int i=0;i<3;i++){
        if(triangles[t].t[i]==-1) op1 = i;
    }
    Vec2 p = (vertices[triangles[t].v[(op1+1)%3]].pos + vertices[triangles[t].v[(op1+2)%3]].pos)/2.0;
    addPointInEdge(p,t);
}

void Triangulation::remem(){
    if(tcount >= maxTriangles-4){ // we must get more space
        // std::cout << "al triangulos" << std::endl;
        Triangle *newTriangles = new Triangle[maxTriangles*2];
        std::copy(triangles,triangles+tcount,newTriangles);
        delete[] triangles;
        triangles = newTriangles;

        double* newLengths = new double[maxTriangles*2];
        std::copy(lengths,lengths+tcount,newLengths);
        delete[] lengths;
        lengths = newLengths;

        maxTriangles *= 2;
    }

    if(vcount >= maxVertices-3){ // we must get more space
        // std::cout << "al vertices" << std::endl;
        Vertex *newVertices = new Vertex[maxVertices*2];
        std::copy(vertices,vertices+vcount,newVertices);
        delete[] vertices;
        vertices = newVertices;
        maxVertices *= 2;
    }
}

bool Triangulation::next(int t0, int t1){
    if(t0==-1 && t1==-1)return true;
    if(t0==-1){
        for(int i=0;i<3;i++){
            if(triangles[t1].t[i]==-1)return true;
        }
        return false;
    }
    if(t1==-1){
        for(int i=0;i<3;i++){
            if(triangles[t0].t[i]==-1)return true;
        }
        return false;
    }
    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
            if(triangles[t0].t[i]==t1 && triangles[t1].t[j]==t0)return true;
        }
    }
    return false;
}

void save_mesh(Triangulation *t, const char *filename){
	int vcount = t->vcount;
	int fcount = t->tcount;
	int ecount = 0;
	//following line is for computer with other languages.
	setlocale(LC_NUMERIC, "POSIX");
	FILE *file_descriptor = fopen(filename,"w");
	fprintf(file_descriptor,"OFF\n");
	fprintf(file_descriptor,"%d %d %d\n",vcount, fcount, ecount);
	for(int i=0; i<vcount; i++) {
		fprintf(file_descriptor,"%f %f %f\n",t->vertices[i].pos.x,t->vertices[i].pos.y,0.0);
	}
	for(int i=0; i<fcount; i++) {
		fprintf(file_descriptor,"%d %d %d %d\n", 3, t->triangles[i].v[0],t->triangles[i].v[1],t->triangles[i].v[2]);
	}
	fclose(file_descriptor);
    setlocale(LC_NUMERIC, "");
}

void Triangulation::whichTriangle(){
    for(int i=0;i<vcount;i++){
        for(int j=0;j<tcount;j++){
            for(int k=0;k<3;k++){
                if(triangles[j].v[k]==i)vertices[i].tri_index=j;
            }
        }
    }
}

std::set<int> Triangulation::getNeighbours(int index){
    int tri_index = vertices[index].tri_index;
    std::set<int> neighbours = std::set<int>();
    std::set<int> trianglesChecked = std::set<int>();
    std::vector<int> checkingTriangles = std::vector<int>();
    checkingTriangles.push_back(tri_index);
    while(!checkingTriangles.empty()){
        int curr_triangle = checkingTriangles.back();
        checkingTriangles.pop_back();
        bool found = false;
        for(int i=0;i<3;i++){
            if(triangles[curr_triangle].v[i]==index){neighbours.insert(curr_triangle); found = true;}
        }
        for(int i=0;i<3;i++){
            if(trianglesChecked.find(triangles[curr_triangle].t[i])==trianglesChecked.end())
                if(triangles[curr_triangle].t[i]!=-1)
                    if(found)checkingTriangles.push_back(triangles[curr_triangle].t[i]);
        }
        trianglesChecked.insert(curr_triangle);
    }
    return neighbours;
}

std::set<int> Triangulation::getNeighbourTriangles(int index){ // gets the triangles that have a vertex
    std::vector<int> ctriangles = std::vector<int>(10);
    ctriangles.push_back(vertices[index].tri_index);

    std::set<int> res = std::set<int>();

    while(ctriangles.size()>0){
        int t = ctriangles.back();
        ctriangles.pop_back();

        if(t==-1)continue;

        for(int i=0;i<3;i++){
            if(triangles[t].v[i]==index){
                res.insert(t);
                if(res.find(triangles[t].t[(i+1)%3])==res.end())
                    ctriangles.push_back(triangles[t].t[(i+1)%3]);
                if(res.find(triangles[t].t[(i+2)%3])==res.end())
                    ctriangles.push_back(triangles[t].t[(i+2)%3]);
            }
        }
    }

    return res;
}

float Triangulation::closestNeighbourDistance(int index){
    std::set<int> n = getNeighbours(index);
    // int closestNeighbour = -1;
    float closestDistance = abs(p1[0]-p0[0]) * abs(p2[1]-p0[1]);
    for(int v: n){
        float dist = mod(vertices[index].pos-vertices[v].pos);
        if(dist<closestDistance){
            // closestNeighbour = v;
            closestDistance = dist;
        }
    }
    return closestDistance;
}

void Triangulation::movePoint(int index, Vec2 delta){
    point_being_moved = index;
    std::set<int> nt = getNeighbourTriangles(index);
    Vec2 prevPos = vertices[index].pos;

    vertices[index].pos+=delta;

    bool outside = false;

    for(auto t: nt) {
        if(!isCCW(t)){
            outside = true;
            break;
        }
    }

    if(!outside){
        for(auto t: nt) {
            legalize(t);
        }
        return;
    }

    vertices[index].pos=prevPos;
    auto rmvx = removeVertex(index);
    vertices[index].pos+=delta;
    reAddVertex(rmvx);
}

// We assume that both bicells are ccw oriented
bool Triangulation::isConvexBicell(int t1, int t2){
    __H_BREAK_ASSERT__(isCCW(t1)&&isCCW(t2));
    __H_BREAK_ASSERT__(areConnected(t1,t2));

    int i,j; // find which are the different indices
    for(i=0;i<3;i++){
        if(
            triangles[t1].v[i]!=triangles[t2].v[0] &&
            triangles[t1].v[i]!=triangles[t2].v[1] &&
            triangles[t1].v[i]!=triangles[t2].v[2]
        )
            break;
    }
    for(j=0;j<3;j++){
        if(
            triangles[t2].v[j]!=triangles[t1].v[0] &&
            triangles[t2].v[j]!=triangles[t1].v[1] &&
            triangles[t2].v[j]!=triangles[t1].v[2]
        )
            break;
    }

    /*
    //                   i
    //                    ^
    //                   / \
    //                  /   \
    //                 /     \
    //        (i+1)%3 <-------> (j+1)%3
    //                 \     /
    //                  \   /
    //                   \ /
    //                    v
    //                   j
    */

    std::vector<Vec2> bicell = {vertices[triangles[t1].v[(i+1)%3]].pos,vertices[triangles[t2].v[j]].pos,vertices[triangles[t2].v[(j+1)%3]].pos,vertices[triangles[t1].v[i]].pos};

    for(i=0;i<4;i++){
        Vec2 p0 = bicell[(i-1+4)%4];
        Vec2 p1 = bicell[i];
        Vec2 p2 = bicell[(i+1)%4];
        // Vec2 prev = p1-p0;
        // Vec2 act = p2-p1;
        // if(crossa(prev,act)<0) return false;
        if(orient2d(&(p0.x),&(p1.x),&(p2.x))<=0) return false;
    }

    return true;
}

Triangulation::RemovedVertex Triangulation::removeVertex(int v){
    std::set<int> nt = getNeighbourTriangles(v);
    std::vector<int> ntv = std::vector<int>(nt.begin(),nt.end());

    while(nt.size()>3){
        int n = ntv.size();
        bool found = false;
        for(int i=0;i<n && !found;i++){
            int t1 = ntv[i];
            for(int j=0;j<n && !found;j++){
                if(i==j)continue;
                int t2 = ntv[j];
                if(areConnected(t1,t2) && isConvexBicell(t1,t2)){
                    if(flip(t1,t2))
                        found = true;
                }
            }
        }
        nt = getNeighbourTriangles(v);
        ntv = std::vector<int>(nt.begin(),nt.end());
    }
    __H_BREAK_ASSERT__(ntv.size()==3);
    RemovedVertex res{{ntv[0],ntv[1],ntv[2]},v};
    return res;
}

void Triangulation::reAddVertex(Triangulation::RemovedVertex rmvx){

    __H_BREAK_ASSERT__(sanity(rmvx.t[0]));
    __H_BREAK_ASSERT__(sanity(rmvx.t[1]));
    __H_BREAK_ASSERT__(sanity(rmvx.t[2]));

    int p = rmvx.v;

    int t1 = rmvx.t[0];
    int t2 = rmvx.t[1];
    int t3 = rmvx.t[2];

    int i;
    if      (triangles[t1].v[0] == p) { i=0; }
    else if (triangles[t1].v[1] == p) { i=1; }
    else { i=2; }
    int j;
    if      (triangles[t2].v[0] == p) { j=0; }
    else if (triangles[t2].v[1] == p) { j=1; }
    else { j=2; }
    int k;
    if      (triangles[t3].v[0] == p) { k=0; }
    else if (triangles[t3].v[1] == p) { k=1; }
    else { k=2; }

    int f1 = triangles[t1].t[i];
    int f2 = triangles[t2].t[j];
    int f3 = triangles[t3].t[k];

    int p11 = triangles[t1].v[(i+1)%3];
    int p12 = triangles[t1].v[(i+2)%3];

    int p21 = triangles[t2].v[(j+1)%3];
    int p22 = triangles[t2].v[(j+2)%3];

    int p31 = triangles[t3].v[(k+1)%3];
    int p32 = triangles[t3].v[(k+2)%3];

    if (p31==p22) {
        triangles[t1].v[2] = p31;

        triangles[t1].t[0] = f2;
        triangles[t1].t[1] = f3;

        vertices[p31].tri_index = t1;
    }
    else if (p32==p21) {
        triangles[t1].v[2] = p32;

        triangles[t1].t[0] = f3;
        triangles[t1].t[1] = f2;

        vertices[p32].tri_index = t1;
    }
    else {__H_BREAK_ASSERT__(false);}

    triangles[t1].v[0] = p11;
    triangles[t1].v[1] = p12;

    triangles[t1].t[2] = f1;

    vertices[p11].tri_index = t1;
    vertices[p12].tri_index = t1;

    for(int l=0;l<3;l++) {
        if (triangles[f2].t[l] == t2) { triangles[f2].t[l] = t1; }
        if (triangles[f2].t[l] == t3) { triangles[f2].t[l] = t1; }
    }
    for(int l=0;l<3;l++) {
        if (triangles[f3].t[l] == t2) { triangles[f3].t[l] = t1; }
        if (triangles[f3].t[l] == t3) { triangles[f3].t[l] = t1; }
    }

    triangles[rmvx.t[1]] = Triangle(-1,-1,-1,-1,-1,-1);
    triangles[rmvx.t[2]] = Triangle(-1,-1,-1,-1,-1,-1);

    legalize(rmvx.t[0]);

    int f = findContainerTriangleSqrtSearch(vertices[rmvx.v].pos,rmvx.t[0]); // we have to find the triangle where we'll insert put the point
    // int f = findContainerTriangleLinearSearch(vertices[rmvx.v].pos);
    f1 = rmvx.t[1]; // we reuse the indices
    f2 = rmvx.t[2]; // we reuse the indices

    __H_BREAK_ASSERT__(f!=f1 && f!=f2);
    __H_BREAK_ASSERT__(sanity(f));

    int p0 = triangles[f].v[0];
    int p1 = triangles[f].v[1];
    int p2 = triangles[f].v[2];
    t1 = triangles[f].t[1];
    t2 = triangles[f].t[2];

    triangles[f1] = Triangle(p,p2,p0,t1,f2,f);
    vertices[p2].tri_index = f1;
    vertices[p0].tri_index = f1;
    triangles[f2] = Triangle(p,p0,p1,t2,f,f1);
    vertices[p0].tri_index = f2;
    vertices[p1].tri_index = f2;

    if(t1!=-1){
        if(triangles[t1].t[0]==f)
        triangles[t1].t[0] = f1;
        if(triangles[t1].t[1]==f)
        triangles[t1].t[1] = f1;
        if(triangles[t1].t[2]==f)
        triangles[t1].t[2] = f1;
    }

    if(t2!=-1){
        if(triangles[t2].t[0]==f)
        triangles[t2].t[0] = f2;
        if(triangles[t2].t[1]==f)
        triangles[t2].t[1] = f2;
        if(triangles[t2].t[2]==f)
        triangles[t2].t[2] = f2;
    }

    triangles[f].v[0] = p;
    triangles[f].t[1] = f1;
    triangles[f].t[2] = f2;

    vertices[p] = Vertex(vertices[rmvx.v].pos,f);

    __H_BREAK_ASSERT__(isCCW(f)&&isCCW(f1)&&isCCW(f2));
    __H_BREAK_ASSERT__(frontTest(f)&&frontTest(f1)&&frontTest(f2));
    __H_BREAK_ASSERT__(sanity(f)&&sanity(f1)&&sanity(f2));

    legalize(f);
    legalize(f1);
    legalize(f2);
}

std::set<int> Triangulation::getFRNN(int index, float r){
    int tri_index = vertices[index].tri_index;
    std::set<int> neighbours = std::set<int>();
    std::set<int> trianglesChecked = std::set<int>();
    std::vector<int> checkingTriangles = std::vector<int>();
    checkingTriangles.push_back(tri_index);
    while(!checkingTriangles.empty()){
        int curr_triangle = checkingTriangles[checkingTriangles.size()-1];
        checkingTriangles.pop_back();
        bool isInside = false;
        for(int i=0;i<3;i++){
            if(triangles[curr_triangle].v[i]==index){continue;}
            else if(dist2(vertices[triangles[curr_triangle].v[i]].pos,vertices[index].pos)<=r){
                neighbours.insert(triangles[curr_triangle].v[i]);
                isInside=true;
            }
        }
        for(int i=0;i<3;i++){
            if(trianglesChecked.find(triangles[curr_triangle].t[i])==trianglesChecked.end())
                if(triangles[curr_triangle].t[i]!=-1 && isInside)checkingTriangles.push_back(triangles[curr_triangle].t[i]);
        }
        trianglesChecked.insert(curr_triangle);
    }
    return neighbours;
}

std::set<std::pair<int,double>> Triangulation::getFRNN_distance_exp(int index, float r){

    auto neighbors = std::set<std::pair<int,double>>();
    auto checkedPoints = std::set<int>();
    auto pointsToCheck = std::vector<int>();
    pointsToCheck.push_back(index);
    while(!pointsToCheck.empty()){
        int curr_point = pointsToCheck[pointsToCheck.size()-1];
        pointsToCheck.pop_back();

        if(checkedPoints.find(curr_point)!=checkedPoints.end()) continue;
        if(curr_point!=index){
            double dist = dist2(vertices[curr_point].pos,vertices[index].pos); // O(I) distance calculations
            if(dist<r){
                neighbors.insert(std::make_pair(curr_point,dist));
            }
        }

        checkedPoints.insert(curr_point);

        auto ns = getNeighbourTriangles(curr_point);
        for(int t: ns){ // every triangle that has curr_point
            int i=-1;
            for(int j=0;j<3;j++){
                if(triangles[t].v[j]==curr_point)i=j;
            }

            if(lengths[t*3+i]>r)continue; // if the edge is bigger than r, there is no way the point is inside the frnn of index
            int j = triangles[t].v[(i+1)%3];
            if(checkedPoints.find(j)==checkedPoints.end()){
                pointsToCheck.push_back(j);
            }
        }
    }
    return neighbors;
}

std::set<std::pair<int,double>> Triangulation::getFRNN_distance(int index, float r){
    int tri_index = vertices[index].tri_index;
    std::set<std::pair<int,double>> neighbours = std::set<std::pair<int,double>>();
    std::set<int> trianglesChecked = std::set<int>();
    std::vector<int> checkingTriangles = std::vector<int>();
    checkingTriangles.push_back(tri_index);
    while(!checkingTriangles.empty()){
        int curr_triangle = checkingTriangles[checkingTriangles.size()-1];
        checkingTriangles.pop_back();
        bool isInside = false;
        for(int i=0;i<3;i++){
            if(triangles[curr_triangle].v[i]==index){continue;}
            double d;
            if((d = dist2(vertices[triangles[curr_triangle].v[i]].pos,vertices[index].pos))<=r){
                neighbours.insert(std::make_pair(triangles[curr_triangle].v[i],d));
                isInside=true;
            }
        }
        for(int i=0;i<3;i++){
            if(trianglesChecked.find(triangles[curr_triangle].t[i])==trianglesChecked.end())
                if(triangles[curr_triangle].t[i]!=-1 && isInside)checkingTriangles.push_back(triangles[curr_triangle].t[i]);
        }
        trianglesChecked.insert(curr_triangle);
    }
    return neighbours;
}

bool Triangulation::allSanity(){
    bool res = true;
    for(int i=0;i<tcount;i++)if(!sanity(i)){std::cout << i << std::endl;res = false;}
    return res;
}

std::vector<std::vector<std::pair<int,double>>> Triangulation::get_all_FRNN(float r){
    auto all_neighbours = std::vector<std::vector<std::pair<int,double>>>(vcount,std::vector<std::pair<int,double>>());
    for(int i=0;i<vcount;i++)all_neighbours[i].reserve(32);

    int *neighbours = new int[vcount];
    int *trianglesChecked = new int[tcount];
    std::vector<int> tchk = std::vector<int>();
    for(int i=0;i<vcount;i++)neighbours[i]=0;
    for(int i=0;i<tcount;i++)trianglesChecked[i]=0;
    for(int index=0;index<vcount;index++){
        tchk.clear();
        int tri_index = vertices[index].tri_index;
        std::stack<int> checkingTriangles = std::stack<int>();
        checkingTriangles.push(tri_index);
        while(!checkingTriangles.empty()){
            int curr_triangle = checkingTriangles.top();
            checkingTriangles.pop();
            bool isInside = true;
            for(int i=0;i<3;i++){
                if(triangles[curr_triangle].v[i]==index){continue;}
                int j = triangles[curr_triangle].v[i];
                if(neighbours[j]){continue;}
                double d = dist2(vertices[j].pos,vertices[index].pos);
                if(d<=r){
                    all_neighbours[index].push_back(std::make_pair(j,d));
                    neighbours[j]=1;
                }
                if(d>r) isInside=false;
            }
            for(int i=0;i<3;i++){
                if(!trianglesChecked[triangles[curr_triangle].t[i]])
                    if(triangles[curr_triangle].t[i]!=-1 && isInside)checkingTriangles.push(triangles[curr_triangle].t[i]);
            }
            trianglesChecked[curr_triangle]=1;
            tchk.push_back(curr_triangle);
        }
        for(int i=0;i<all_neighbours[index].size();i++)neighbours[all_neighbours[index][i].first]=0;
        for(int i=0;i<tchk.size();i++)trianglesChecked[tchk[i]]=0;
    }
    return all_neighbours;
}