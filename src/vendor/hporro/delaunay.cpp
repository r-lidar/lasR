#include "constants.h"
#include "pred3d.h"
#include "utils.h"

#include "delaunay.h"

#include <algorithm>
#include <cmath>

static bool initialized = false;

#ifndef MAX3
#define MAX3(a,b,c) (((a)>(b)) ? (((a)>(c)) ? (a) : (c)) : (((b)>(c)) ? (b) : (c)));
#define MIN3(a,b,c) (((a)>(b)) ? (((b)>(c)) ? (c) : (b)) : (((a)>(c)) ? (c) : (a)));
#endif

Triangulation::Triangulation(const std::vector<Vec2>& points)
{
  if (!initialized)
  {
    exactinit();
    initialized = true;
  }

  int numP = points.size();

  double minx = 10000000;
  double miny = 10000000;
  double maxx =-10000000;
  double maxy =-10000000;
  for(auto p: points)
  {
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

  vertices[0] = Vertex(Vec2(-10000000,-1000000));
  vertices[1] = Vertex(Vec2( 10000000,-10000000));
  vertices[2] = Vertex(Vec2( 10000000, 10000000));
  vertices[3] = Vertex(Vec2(-10000000, 10000000));

  triangles[0] = Triangle(0,1,2,-1,1,-1);
  triangles[1] = Triangle(0,2,3,-1,-1,0);

  vcount = 4;
  tcount = 2;

  for(int i=0;i<(int)points.size();i++)
  {
    delaunayInsertion(points[i]);
  }
}

Triangulation::~Triangulation()
{
  delete[] triangles;
  delete[] vertices;
}


bool Triangulation::delaunayInsertion(const Vec2& p, int prop)
{
  remem();

  int tri_index = findContainerTriangle(p,prop);

  Vec2 a = vertices[triangles[tri_index].v[0]].pos;
  Vec2 b = vertices[triangles[tri_index].v[1]].pos;
  Vec2 c = vertices[triangles[tri_index].v[2]].pos;
  Vec2 points[] = {a,b,c};

  //printf("  Insertion in triangle %d (%.1lf, %.1lf), (%.1lf, %.1lf) (%.1lf, %.1lf)\n", tri_index, points[0][0], points[0][1], points[1][0], points[1][1], points[2][0], points[2][1]);

  // we dont insert repeated points
  for (int i = 0 ; i < 3 ; i++)
  {
    if ((std::abs(p[0]-points[i][0]) < IN_TRIANGLE_EPS) && (std::abs(p[1]-points[i][1]) < IN_TRIANGLE_EPS))
      return false;
  }

  for (int i = 0; i < 3 ; i++)
  {
    if (pointInSegment(p, points[(i+1)%3],points[(i+2)%3]))
    {
      // insert a point in the i edge
      if(triangles[tri_index].t[i]==-1)
      {
        addPointInEdge(p, tri_index);
        legalize(tri_index);
        legalize(tcount-1);
        legalize(tcount-2);
      }
      else
      {
        addPointInEdge(p, tri_index, triangles[tri_index].t[i]);
        for(int j=0;j<3;j++)
        {
          legalize(triangles[tri_index].t[i]);
          legalize(tri_index);
          legalize(tcount-1);
          legalize(tcount-2);
        }
      }

      return true;
    }
  }

  if (tri_index !=- 1)
  {
    this->incount++;
    addPointInside(p,tri_index);
    legalize(tri_index);
    legalize(tcount-1);
    legalize(tcount-2);

    return true;
  }

  return true;
}

int Triangulation::findContainerTriangle(const Vec2& p, int prop) const
{
  // If the initial triangle index is invalid, start with the last triangle
  if (prop < 0 || prop >= tcount) prop = tcount - 1;

  // Initialize the current triangle index
  int t = prop;

  // Continue searching until we either find the triangle or exhaust neighbors
  while (true)
  {
    // Check if the point is inside the current triangle or on its edge
    if (isInside(t, p) || isInEdge(t, p)) return t;

    // Calculate the centroid of the current triangle
    Vec2 v = (vertices[triangles[t].v[0]].pos + vertices[triangles[t].v[1]].pos + vertices[triangles[t].v[2]].pos) / 3.0;

    bool foundNext = false;

    // Iterate over the neighbors of the current triangle
    for (int i = 0 ; i < 3 ; i++)
    {
      int f = triangles[t].t[i];

      // If there is no neighbor in this direction, skip to the next one
      if (f == -1) continue;

      // Get the vertices of the edge opposite to the neighbor
      Vec2 a = vertices[triangles[t].v[(i + 1) % 3]].pos;
      Vec2 b = vertices[triangles[t].v[(i + 2) % 3]].pos;

      // Perform orientation tests to check if the point lies in the neighboring triangle
      if ((orient2d(&v.x, &p.x, &a.x) * orient2d(&v.x, &p.x, &b.x) < 0) &&
          (orient2d(&a.x, &b.x, &p.x) * orient2d(&a.x, &b.x, &v.x) < 0))
      {
        // If the point is likely in the neighbor, update the current triangle and continue
        t = f;
        foundNext = true;
        break;
      }
    }

    // If no suitable neighbor is found, return -1 indicating the point is outside the triangulation
    if (!foundNext) return -1;
  }
}

/*int Triangulation::findContainerTriangleLinearSearch(const Vec2& p) const
{
  for (int i = 0 ; i < tcount ; i++)
  {
    if (isInside(i,p))
    {
      return i;
    }
  }

  for(int i = 0 ; i < tcount ; i++)
  {
    if (isInEdge(i,p))
    {
      return i;
    }
  }

  return -1;
}*/

/*int Triangulation::findContainerTriangleSqrtSearch(const Vec2& p, int prop) const
{
  if (prop < 0) prop = tcount-1;

  if(isInside(prop,p)) return prop;
  if(isInEdge(prop,p)) return prop;

  Vec2 v = (vertices[triangles[prop].v[0]].pos+vertices[triangles[prop].v[1]].pos+vertices[triangles[prop].v[2]].pos)/3.0;

  int t = prop;
  for (int i = 0 ; i < 3 ; i++)
  {
    int f = triangles[t].t[i];

    if (f == -1) continue;

    Vec2 a = vertices[triangles[t].v[(i+1)%3]].pos;
    Vec2 b = vertices[triangles[t].v[(i+2)%3]].pos;

    if ((orient2d(&(v.x),&(p.x),&(a.x))*orient2d(&(v.x),&(p.x),&(b.x)) < 0) &&
        (orient2d(&(a.x),&(b.x),&(p.x))*orient2d(&(a.x),&(b.x),&(v.x)) < 0))
    {
      return findContainerTriangleSqrtSearch(p,f);
    }
  }

  return -1;
}*/

double Triangulation::triangleArea(int f) const
{
  if (f == -1) return true;

  Vec2 p0 = vertices[triangles[f].v[0]].pos;
  Vec2 p1 = vertices[triangles[f].v[1]].pos;
  Vec2 p2 = vertices[triangles[f].v[2]].pos;

  return (crossa(p0,p1)+crossa(p1,p2)+crossa(p2,p0));
}

bool Triangulation::isInside(int t, Vec2 p) const
{
  if (t == -1)
    return false;

  if ((triangles[t].v[0] == -1) && (triangles[t].v[1] == -1) && (triangles[t].v[2] == -1))
    return false;

  Vec2 p1 = vertices[triangles[t].v[0]].pos;
  Vec2 p2 = vertices[triangles[t].v[1]].pos;
  Vec2 p3 = vertices[triangles[t].v[2]].pos;

  double lim;

  lim = MIN3(p1.x, p2.x, p3.x);
  if (p.x < lim) return false;

  lim = MAX3(p1.x, p2.x, p3.x);
  if (p.x > lim) return false;

  lim = MIN3(p1.y, p2.y, p3.y);
  if (p.y < lim) return false;

  lim = MAX3(p1.y, p2.y, p3.y);
  if (p.y > lim) return false;

  return (orient2d(&(p1.x),&(p2.x),&(p.x)) > 0) &&
    (orient2d(&(p2.x),&(p3.x),&(p.x)) > 0) &&
    (orient2d(&(p3.x),&(p1.x),&(p.x)) > 0);
}

bool Triangulation::isInside(int t, int v) const
{
  if (t==-1) return 0;

  return isInside(t, vertices[v].pos);
}

bool Triangulation::isInEdge(int t, Vec2 p) const
{
  if (t==-1) return false;

  if (triangles[t].v[0] == -1 && triangles[t].v[1] == -1 && triangles[t].v[2] == -1)
    return false;

  Vec2 p1 = vertices[triangles[t].v[0]].pos;
  Vec2 p2 = vertices[triangles[t].v[1]].pos;
  Vec2 p3 = vertices[triangles[t].v[2]].pos;

  double lim;

  lim = MIN3(p1.x, p2.x, p3.x);
  if (p.x < lim) return false;

  lim = MAX3(p1.x, p2.x, p3.x);
  if (p.x > lim) return false;

  lim = MIN3(p1.y, p2.y, p3.y);
  if (p.y < lim) return false;

  lim = MAX3(p1.y, p2.y, p3.y);
  if (p.y > lim) return false;

  // JR: should be an OR operation??
  return (orient2d(&(p1.x),&(p2.x),&(p.x)) == 0) &&
    (orient2d(&(p2.x),&(p3.x),&(p.x)) == 0) &&
    (orient2d(&(p3.x),&(p1.x),&(p.x)) == 0);

  // b-a goes from a to b
  // Vec2 a1 = p1-p2; Vec2 b1 = p-p2;
  // Vec2 a2 = p2-p3; Vec2 b2 = p-p3;
  // Vec2 a3 = p3-p1; Vec2 b3 = p-p1;
  // if(mightBeLeft(p2-p1,p-p1) && mightBeLeft(p3-p2,p-p2) && mightBeLeft(p1-p3,p-p3))
  // if( (a1[0]>=b1[0] && a1[1]>=b1[1]) || (a2[0]>=b2[0] && a2[1]>=b2[1]) || (a3[0]>=b3[0] && a3[1]>=b3[1]) ) return true;
  // return false;
}

// checks for repeated vertices or triangles
/*bool Triangulation::validTriangle(int t)
{
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
}*/


// checks that every t's neighbour has t as its neighbour
/*bool Triangulation::integrity(int t)
{
  return true;

  int t0 = triangles[t].t[0];
  int t1 = triangles[t].t[1];
  int t2 = triangles[t].t[2];

  bool a=true,b=true,c=true;

  if(t0!=-1) a = (t==triangles[t0].t[0]) || (t==triangles[t0].t[1]) || (t==triangles[t0].t[2]);
  if(t1!=-1) b = (t==triangles[t1].t[0]) || (t==triangles[t1].t[1]) || (t==triangles[t1].t[2]);
  if(t2!=-1) c = (t==triangles[t2].t[0]) || (t==triangles[t2].t[1]) || (t==triangles[t2].t[2]);

  return (a&&b)&&c;
}*/

void Triangulation::addPointInside(const Vec2& v, int tri_index)
{
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

  if (t1 != -1)
  {
    if (triangles[t1].t[0] == f) triangles[t1].t[0] = f1;
    if (triangles[t1].t[1] == f) triangles[t1].t[1] = f1;
    if (triangles[t1].t[2] == f) triangles[t1].t[2] = f1;
  }

  if (t2 != -1)
  {
    if(triangles[t2].t[0]==f) triangles[t2].t[0] = f2;
    if(triangles[t2].t[1]==f) triangles[t2].t[1] = f2;
    if(triangles[t2].t[2]==f) triangles[t2].t[2] = f2;
  }

  triangles[f].v[0] = p;
  triangles[f].t[1] = f1;
  triangles[f].t[2] = f2;

  vertices[p] = Vertex(v,f);
}

void Triangulation::addPointInEdge(const Vec2& v, int t0, int t1)
{
  remem();

  int p = vcount;
  vertices[vcount++] = Vertex(v);

  int t0_v = -1;
  int t1_v = -1;

  int f0,f3;
  int p1;

  for (int i = 0 ; i < 3 ; i++)
  {
    for (int j = 0 ; j < 3 ; j++)
    {
      if (triangles[t0].t[i]==t1 && triangles[t1].t[j]==t0)
      {
        t0_v = i;
        t1_v = j;
      }
    }
  }

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
}

void Triangulation::addPointInEdge(const Vec2& v, int t)
{
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

  if (f2 != -1)
  {
    triangles[f2].t[0] = (triangles[f2].t[0] == t ? t1 : triangles[f2].t[0]);
    triangles[f2].t[1] = (triangles[f2].t[1] == t ? t1 : triangles[f2].t[1]);
    triangles[f2].t[2] = (triangles[f2].t[2] == t ? t1 : triangles[f2].t[2]);
  }

  remem();
}


bool Triangulation::legalize(int t)
{
  for (int i = 0 ; i < 3 ; i++) legalize(t, triangles[t].t[i]);
  return true;
}

bool Triangulation::legalize(int t1, int t2)
{
  if ( t2 == -1) return true;
  if ( t1 == -1) return true;
  if (!areConnected(t1,t2)) return true;

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

  for(int i = 3 ; i < 6 ; i++)
  {
    if ((a[i]!=b[0]) && (a[i]!=b[1]) && (a[i]!=b[2]))
    {
      b[3] = a[i];
    }
  }

  b[4] = a[3];
  b[5] = a[4];
  b[6] = a[5];

  for (int i = 0 ; i < 3 ; i++)
  {
    if((a[i]!=b[4]) && (a[i]!=b[5]) && (a[i]!=b[6]))
    {
      b[7] = a[i];
    }
  }

  if(isConvexBicell(t1,t2) &&
     (inCircle(vertices[b[0]].pos,vertices[b[1]].pos,vertices[b[2]].pos,vertices[b[3]].pos)>0 ||
     inCircle(vertices[b[4]].pos,vertices[b[5]].pos,vertices[b[6]].pos,vertices[b[7]].pos)>0))
  {
    bool p = flip(t1,t2);
    legalize(t1);
    legalize(t2);
    return p;
  }

  return true;
}

// check if two triangles are neighbours
bool Triangulation::areConnected(int t1, int t2) const
{
  if (t1 == -1 || t2==-1) return true;

  bool one = false;
  bool two = false;
  for(int i = 0 ; i < 3 ; i++)
  {
    if (triangles[t1].t[i] == t2) one = true;
    if (triangles[t2].t[i] == t1) two = true;
  };
  return one && two;
}

/*void Triangulation::print()
 {
 for(int i = 0 ; i < tcount ; i++)
 {
 std::cout << "Triangle " << i << ":\n";
 std::cout << "P0: " << vertices[triangles[i].v[0]].pos.x << " " << vertices[triangles[i].v[0]].pos.y << std::endl;
 std::cout << "P1: " << vertices[triangles[i].v[1]].pos.x << " " << vertices[triangles[i].v[1]].pos.y << std::endl;
 std::cout << "P2: " << vertices[triangles[i].v[2]].pos.x << " " << vertices[triangles[i].v[2]].pos.y << std::endl;
 }
 }

 void Triangulation::print_ind()
 {
 for(int i = 0 ; i < tcount ; i++)
 {
 std::cout << "Triangle " << i << ": ";
 std::cout << triangles[i].t[0] << " " << triangles[i].t[1] << " " << triangles[i].t[2] << std::endl;
 }
 }*/

bool Triangulation::isCCW(int f) const
{
  if (f == -1) return true;

  Vec2 p0 = vertices[triangles[f].v[0]].pos;
  Vec2 p1 = vertices[triangles[f].v[1]].pos;
  Vec2 p2 = vertices[triangles[f].v[2]].pos;

  return (orient2d(&(p0.x),&(p1.x),&(p2.x)) > 0);

  // if((crossa(p0,p1)+crossa(p1,p2)+crossa(p2,p0))>IN_TRIANGLE_EPS) return true;
  // return false;
}


bool Triangulation::flip(int t1, int t2)
{
  int i;
  if (triangles[t1].t[0] == t2) { i = 0; }
  else if (triangles[t1].t[1] == t2) { i = 1; }
  else { i = 2; }

  int j;
  if (triangles[t2].t[0] == t1) { j = 0; }
  else if (triangles[t2].t[1] == t1) { j = 1; }
  else { j = 2; }

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

  if(f11 != -1)
  {
    for (int k = 0 ; k < 3 ; k++)
    {
      if (triangles[f11].t[k] == t1)
      {
        triangles[f11].t[k] = t2;
      }
    }
  }

  if (f21 != -1)
  {
    for(int k = 0 ; k < 3 ; k++)
    {
      if (triangles[f21].t[k] == t2)
      {
        triangles[f21].t[k] = t1;
      }
    }
  }

  return true;
}

/*bool Triangulation::sanity(int t)
{
  if (t==-1) return true;

  for(int i = 0 ; i < 3 ; i++)
  {
    int count = 0;
    int f = triangles[t].t[i];
    if (f==-1) continue;
    for(int j = 0 ; j < 3 ; j++)
    {
      for(int k = 0 ; k < 3 ; k++)
      {
        if(triangles[t].v[k]==triangles[f].v[j])count++;
      }
    }
    if(count != 2) return false;
  }
  return true;
}*/

void Triangulation::remem()
{
  // we must get more space
  if (tcount >= maxTriangles-4)
  {
    Triangle *newTriangles = new Triangle[maxTriangles*2];
    std::copy(triangles, triangles+tcount, newTriangles);
    delete[] triangles;
    triangles = newTriangles;

    maxTriangles *= 2;
  }

  // we must get more space
  if (vcount >= maxVertices-3)
  {
    Vertex *newVertices = new Vertex[maxVertices*2];
    std::copy(vertices,vertices+vcount,newVertices);
    delete[] vertices;
    vertices = newVertices;
    maxVertices *= 2;
  }
}

// We assume that both bicells are ccw oriented
bool Triangulation::isConvexBicell(int t1, int t2)
{
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
   //                    i
   //                    ^
   //                   / \
   //                  /   \
   //                 /     \
   //        (i+1)%3 <-------> (j+1)%3
   //                 \     /
   //                  \   /
   //                   \ /
   //                    v
   //                    j
   */

  std::vector<Vec2> bicell = {vertices[triangles[t1].v[(i+1)%3]].pos,vertices[triangles[t2].v[j]].pos,vertices[triangles[t2].v[(j+1)%3]].pos,vertices[triangles[t1].v[i]].pos};

  for( i = 0 ; i < 4 ; i++)
  {
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