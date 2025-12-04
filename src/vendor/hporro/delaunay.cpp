#include "constants.h"
#include "pred3d.h"
#include "delaunay.h"

#include <algorithm>
#include <cmath>

// --- temporary global stats ---
#include <chrono>
static long long g_query_count = 0;
static long long f_query_count = 0;
static long long n_triangle_tested = 0;
std::chrono::duration<double> total_fast_search_time(0);
std::chrono::duration<double> total_search_time(0);
std::chrono::duration<double> total_insertion_time(0);
std::chrono::duration<double> total_legalize_time(0);
std::chrono::duration<double> total_unindex_time(0);
std::chrono::duration<double> total_index_time(0);

static bool initialized = false;

#ifndef MAX3
#define MAX3(a,b,c) (((a)>(b)) ? (((a)>(c)) ? (a) : (c)) : (((b)>(c)) ? (b) : (c)));
#define MIN3(a,b,c) (((a)>(b)) ? (((b)>(c)) ? (c) : (b)) : (((a)>(c)) ? (c) : (a)));
#endif

Triangulation::Triangulation(const Grid& index_) : index(index_)
{
  int numP = 10000; // allocate memory for 10000 points

  total_fast_search_time = std::chrono::milliseconds::zero();
  total_search_time = std::chrono::milliseconds::zero();
  total_insertion_time = std::chrono::milliseconds::zero();
  total_legalize_time = std::chrono::milliseconds::zero();
  total_unindex_time = std::chrono::milliseconds::zero();
  total_index_time = std::chrono::milliseconds::zero();

  if (!initialized)
  {
    exactinit();
    initialized = true;
  }

  double minx = -1e8;
  double miny = -1e8;
  double maxx = 1e8;
  double maxy = 1e8;

  a = std::max(maxx-minx,maxy-miny);

  p0 = Vec2(minx,miny) + Vec2(-a/10,-a/10);
  p1 = p0 + Vec2(a+2*a/10,0);
  p2 = p0 + Vec2(a+2*a/10,a+2*a/10);
  p3 = p0 + Vec2(0,a+2*a/10);

  maxVertices = numP+6;
  maxTriangles = numP*2+7;
  vertices = new Vertex[maxVertices]; // num of vertices
  triangles = new Triangle[maxTriangles]; // 2(n+6) - 2 - 3 = 2n+7 // num of faces

  vertices[0] = Vertex(Vec2(minx, miny));
  vertices[1] = Vertex(Vec2(maxx, miny));
  vertices[2] = Vertex(Vec2(maxx, maxy));
  vertices[3] = Vertex(Vec2(minx, maxy));

  triangles[0] = Triangle(0,1,2,-1,1,-1);
  triangles[1] = Triangle(0,2,3,-1,-1,0);

  vcount = 4;
  tcount = 2;

  grid.resize(index.get_ncells());
}

Triangulation::~Triangulation()
{
  printf("Fast search took %.2f secs with an average of %.1lf triangle per query\n", total_fast_search_time.count(), (double)n_triangle_tested/(double)f_query_count);
  printf("Fallback search called %llu times and took %.2f secs\n", g_query_count, total_search_time.count());
  printf("Delaunay insertion took %.2f secs\n", total_insertion_time.count());
  printf("  legalize took %.2f secs\n", total_legalize_time.count());
  printf("    index took %.2f secs\n", total_index_time.count());
  printf("    deindex took %.2f secs\n", total_unindex_time.count());
  delete[] triangles;
  delete[] vertices;
}

bool Triangulation::delaunayInsertion(const Vec2& p, int prop)
{
  auto start_time = std::chrono::high_resolution_clock::now();
  remem();

  int tri_index = prop; // assuming prop is the correct triangle from findContainerTriangleFast
  //int tri_index = findContainerTriangle(p, prop);

  Vec2 a = vertices[triangles[tri_index].v[0]].pos;
  Vec2 b = vertices[triangles[tri_index].v[1]].pos;
  Vec2 c = vertices[triangles[tri_index].v[2]].pos;
  Vec2 points[] = {a,b,c};

  // Check for duplicates
  for (int i = 0 ; i < 3 ; i++)
  {
    if ((std::abs(p.x-points[i].x) < IN_TRIANGLE_EPS) && (std::abs(p.y-points[i].y) < IN_TRIANGLE_EPS))
    {
      auto end_time = std::chrono::high_resolution_clock::now();
      total_insertion_time += end_time - start_time;
      return false;
    }
  }

  // --- Check Edges ---
  for (int i = 0; i < 3 ; i++)
  {
    if (pointInSegment(p, points[(i+1)%3], points[(i+2)%3]))
    {
      int t_neighbor = triangles[tri_index].t[i];

      if (t_neighbor == -1)
      {
        addPointInEdge(p, tri_index);
        // Created 2 new triangles from 1.
        // The modified triangles are tri_index and tcount-1.
        // Actually addPointInEdge(p, t) usually creates more splits, let's look at your implementation:
        // It creates t1 (new) and modifies t.
        legalize({tri_index, tcount - 1});
      }
      else
      {
        addPointInEdge(p, tri_index, t_neighbor);
        // addPointInEdge(p, t0, t1) creates t2, t3 and modifies t0, t1.
        // We need to legalize all 4.
        legalize({tri_index, t_neighbor, tcount - 1, tcount - 2});
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      total_insertion_time += end_time - start_time;
      return true;
    }
  }

  // --- Inside Triangle ---
  if (tri_index != -1)
  {
    this->incount++;
    addPointInside(p, tri_index);
    // addPointInside splits tri_index into 3: tri_index, tcount-2, tcount-1
    legalize({tri_index, tcount - 1, tcount - 2});

    auto end_time = std::chrono::high_resolution_clock::now();
    total_insertion_time += end_time - start_time;
    return true;
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  total_insertion_time += end_time - start_time;
  return true;
}

int Triangulation::findContainerTriangle(const Vec2& p, int prop) const
{
  auto start_time = std::chrono::high_resolution_clock::now();
  g_query_count++;

  int iteration = 0;

  // Overallocate to reduce future reallocations
  if ((int)visited.capacity() < tcount)
    visited.reserve(tcount * 2);

  if ((int)visited.size() < tcount)
    visited.resize(tcount, 0);

  // Increment visit tag (reset only if overflow)
  if (++visit_tag == 0)
  {
    // Handle integer overflow (wraparound): clear vector
    std::fill(visited.begin(), visited.end(), 0);
    visit_tag = 1;
  }

  // If the initial triangle index is invalid, start with the last triangle
  if (prop < 0 || prop >= tcount) prop = tcount - 1;

  // Initialize the current triangle index
  int t = prop;

  // Continue searching until we either find the triangle or exhaust neighbors
  while (true)
  {
    // Check if the point is inside the current triangle or on its edge
    if (isInside(t, p) || isInEdge(t, p))
    {
      auto end_time = std::chrono::high_resolution_clock::now();
      total_search_time += end_time - start_time;
      return t;
    }

    // Calculate the centroid of the current triangle
    Vec2 v = (vertices[triangles[t].v[0]].pos + vertices[triangles[t].v[1]].pos + vertices[triangles[t].v[2]].pos) / 3.0;

    bool foundNext = false;

    // Iterate over the neighbors of the current triangle
    for (int i = 0 ; i < 3 ; i++)
    {
      int f = triangles[t].t[i];

      // If there is no neighbor in this direction, skip to the next one
      if (f == -1) continue;

      // Skip if the neighbor has already been visited
      if (visited[f] == visit_tag) continue; // already visited

      // Get the vertices of the edge opposite to the neighbor
      Vec2 a = vertices[triangles[t].v[(i + 1) % 3]].pos;
      Vec2 b = vertices[triangles[t].v[(i + 2) % 3]].pos;

      // Perform orientation tests to check if the point lies in the neighboring triangle
      if ((orient2d(&v.x, &p.x, &a.x) * orient2d(&v.x, &p.x, &b.x) < 0) &&
          (orient2d(&a.x, &b.x, &p.x) * orient2d(&a.x, &b.x, &v.x) < 0))
      {
        // Mark current triangle as visited
        visited[t] = visit_tag;

        // If the point is likely in the neighbor, update the current triangle and continue
        t = f;
        foundNext = true;
        break;
      }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    total_search_time += end_time - start_time;

    // If no suitable neighbor is found, return -1 indicating the point is outside the triangulation
    if (!foundNext) return -1;
  }
}

int Triangulation::findContainerTriangleFast(const Vec2& p) const
{
  auto start_time = std::chrono::high_resolution_clock::now();

  f_query_count++;

  int cell = index.cell_from_xy(p.x, p.y);

  if (cell != -1)
  {
    // Search the candidate triangles in the grid cell
    const auto& candidate_triangles = grid[cell];
    n_triangle_tested += candidate_triangles.size();

    // Search the candidate triangles for an exact match first
    for (int t : candidate_triangles)
    {
      // Check if the ID is valid
      if (t < tcount)
      {
        if (isInside(t, p) || isInEdge(t, p))
        {
          auto end_time = std::chrono::high_resolution_clock::now();
          total_fast_search_time += end_time - start_time;
          return t;
        }
      }
    }

    if (!candidate_triangles.empty())
    {
      return findContainerTriangle(p, candidate_triangles[0]);
    }
  }

  // Fallback if no match is found
  return findContainerTriangle(p, -1);
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

void Triangulation::addPointInside(const Vec2& v, int tri_index)
{
  remem();

  int f = tri_index;
  int f1 = tcount++;
  int f2 = tcount++;

  unindexTriangle(f);

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

  indexTriangle(f);
  indexTriangle(f1);
  indexTriangle(f2);
}

void Triangulation::addPointInEdge(const Vec2& v, int t0, int t1)
{
  remem();

  unindexTriangle(t0);
  unindexTriangle(t1);

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

  indexTriangle(t0);
  indexTriangle(t1);
  indexTriangle(t2);
  indexTriangle(t3);

  remem();
}

void Triangulation::addPointInEdge(const Vec2& v, int t)
{
  remem();

  unindexTriangle(t);

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

  indexTriangle(t);
  indexTriangle(t1);

  remem();
}

void Triangulation::legalize(const std::vector<int>& input_triangles)
{
  auto start_time = std::chrono::high_resolution_clock::now();

  // Stack of edges to check, represented as pairs of {Triangle_A, Triangle_B}
  std::vector<std::pair<int, int>> stack;
  stack.reserve(input_triangles.size() * 3);

  // 1. Initialize stack with edges of the newly inserted triangles
  // We only check edges that connect to existing triangles (neighbor != -1)
  for (int t : input_triangles)
  {
    for (int i = 0; i < 3; i++)
    {
      int neighbor = triangles[t].t[i];
      if (neighbor != -1)
      {
        stack.push_back({t, neighbor});
      }
    }
  }

  // 2. Process the stack
  while (!stack.empty())
  {
    auto [t1, t2] = stack.back();
    stack.pop_back();

    // Sanity check: Ensure they are still connected (topology might have changed due to previous flips)
    if (!areConnected(t1, t2)) continue;

    // --- Prepare vertices for InCircle test ---
    // We need to identify the vertices of the quad formed by t1 and t2
    // t1 vertices: a0, a1, a2
    // t2 vertices: b0, b1, b2
    // Shared edge: 2 vertices.
    // Opposing vertices: The ones not on the shared edge.

    int a[3] = {triangles[t1].v[0], triangles[t1].v[1], triangles[t1].v[2]};
    int b[3] = {triangles[t2].v[0], triangles[t2].v[1], triangles[t2].v[2]};

    // Find the vertex in T2 that is NOT in T1 (the point we test against T1's circumcircle)
    int p_opp_idx = -1;
    for (int i = 0; i < 3; i++)
    {
      if (b[i] != a[0] && b[i] != a[1] && b[i] != a[2])
      {
        p_opp_idx = b[i];
        break;
      }
    }

    // Find the vertex in T1 that is NOT in T2
    int t1_opp_idx = -1;
    for(int i = 0; i < 3; i++)
    {
      if(a[i] != b[0] && a[i] != b[1] && a[i] != b[2])
      {
        t1_opp_idx = a[i];
        break;
      }
    }

    // If data is inconsistent, skip
    if (p_opp_idx == -1 || t1_opp_idx == -1) continue;

    // --- Delaunay Test ---
    // If the quadrilateral is convex AND the point p_opp is inside the circumcircle of T1
    if (isConvexBicell(t1, t2) &&
        inCircle(vertices[a[0]].pos, vertices[a[1]].pos, vertices[a[2]].pos, vertices[p_opp_idx].pos) > 0)
    {
      // Perform the flip
      flip(t1, t2);

      // --- Update Stack ---
      // After flipping t1 and t2, the diagonal changed.
      // The new "external" edges of the t1+t2 quad might now be illegal.
      // We push the neighbors of t1 and t2 (excluding each other) to the stack.

      for (int i = 0; i < 3; i++)
      {
        int n1 = triangles[t1].t[i];
        if (n1 != -1 && n1 != t2) stack.push_back({t1, n1});

        int n2 = triangles[t2].t[i];
        if (n2 != -1 && n2 != t1) stack.push_back({t2, n2});
      }
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  total_legalize_time += end_time - start_time;
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
  unindexTriangle(t1);
  unindexTriangle(t2);

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

  if (f11 != -1)
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

  indexTriangle(t1);
  indexTriangle(t2);

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

  for(i = 0 ; i < 3 ; i++)
  {
    if (triangles[t1].v[i] != triangles[t2].v[0] &&
        triangles[t1].v[i] != triangles[t2].v[1] &&
        triangles[t1].v[i] != triangles[t2].v[2])
      break;
  }

  for(j = 0 ; j < 3 ; j++)
  {
    if (triangles[t2].v[j]!=triangles[t1].v[0] &&
        triangles[t2].v[j]!=triangles[t1].v[1] &&
        triangles[t2].v[j]!=triangles[t1].v[2]
    )
      break;
  }

   //
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
   //

  std::vector<Vec2> bicell = {vertices[triangles[t1].v[(i+1)%3]].pos, vertices[triangles[t2].v[j]].pos, vertices[triangles[t2].v[(j+1)%3]].pos, vertices[triangles[t1].v[i]].pos};

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

/*double Triangulation::orient2d(const Vec2& pa, const Vec2& pb, const Vec2& pc) const
{
  return orient2d(pa, pb, pc);

  //double det = (pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x);
  //if (std::abs(det) < IN_TRIANGLE_EPS) det = 0.0;
  //return det;
}*/

double Triangulation::inCircle(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) const
{
  return incircle(&(a.x), &(b.x), &(c.x), &(d.x));

  // Compute the determinant of the 4x4 matrix
  /*double ax = a.x - d.x;
  double ay = a.y - d.y;
  double bx = b.x - d.x;
  double by = b.y - d.y;
  double cx = c.x - d.x;
  double cy = c.y - d.y;

  double det = (ax * ax + ay * ay) * (bx * cy - by * cx) -
    (bx * bx + by * by) * (ax * cy - ay * cx) +
    (cx * cx + cy * cy) * (ax * by - ay * bx);

  if (std::abs(det) < IN_CIRCLE_EPS) det = 0.0; // Cocircular

  return det;*/
}

bool Triangulation::pointInSegment(const Vec2& p, const Vec2& p1, const Vec2& p2) const
{
  if (p1 == p2) return false;
  if (p.x < std::min(p1.x,p2.x) + IN_TRIANGLE_EPS) return false;
  if (p.x > std::max(p1.x,p2.x) + IN_TRIANGLE_EPS) return false;
  if (p.y < std::min(p1.y,p2.y) + IN_TRIANGLE_EPS) return false;
  if (p.y > std::max(p1.y,p2.y) + IN_TRIANGLE_EPS) return false;

  Vec2 a = p1-p2;
  Vec2 n = Vec2(-a.y,a.x);

  return std::abs((p-p1).dot(n)) < IN_TRIANGLE_EPS;
}

void Triangulation::unindexTriangle(int t)
{
  auto start_time = std::chrono::high_resolution_clock::now();

  if (t < 0 || t >= tcount) return;

  // 1. Get the triangle's AABB (Same logic as indexTriangle)
  Vec2 p1 = vertices[triangles[t].v[0]].pos;
  Vec2 p2 = vertices[triangles[t].v[1]].pos;
  Vec2 p3 = vertices[triangles[t].v[2]].pos;

  double min_x = std::min({p1.x, p2.x, p3.x});
  double max_x = std::max({p1.x, p2.x, p3.x});
  double min_y = std::min({p1.y, p2.y, p3.y});
  double max_y = std::max({p1.y, p2.y, p3.y});

  // 2. Map AABB to Grid Cells (Use your existing index class utility)
  std::vector<int> cells;
  index.get_cells(min_x, min_y, max_x, max_y, cells);

  // 3. Remove t from only the overlapping cells
  for (int cell_index : cells)
  {
    auto& cell = grid[cell_index];
    // Iterate manually to find the ID
    for (size_t i = 0; i < cell.size(); ++i)
    {
      if (cell[i] == t)
      {
        cell[i] = cell.back(); // Move the last element to the current position
        cell.pop_back();       // Remove the last element
        break;                 // Stop, assuming t appears only once per cell
      }
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  total_unindex_time += end_time - start_time;
}

void Triangulation::indexTriangle(int t)
{
  auto start_time = std::chrono::high_resolution_clock::now();

  if (t < 0 || t >= tcount) return;

  // 1. Get the triangle's AABB
  Vec2 p1 = vertices[triangles[t].v[0]].pos;
  Vec2 p2 = vertices[triangles[t].v[1]].pos;
  Vec2 p3 = vertices[triangles[t].v[2]].pos;

  double min_x = std::min({p1.x, p2.x, p3.x});
  double max_x = std::max({p1.x, p2.x, p3.x});
  double min_y = std::min({p1.y, p2.y, p3.y});
  double max_y = std::max({p1.y, p2.y, p3.y});

  // 2. Map AABB to Grid Coordinates
  std::vector<int> cells;
  index.get_cells(min_x, min_y, max_x, max_y, cells);

  // 3. Insert t into all overlapping cells
  for (auto cell : cells)
  {
    grid[cell].push_back(t);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  total_index_time += end_time - start_time;
}