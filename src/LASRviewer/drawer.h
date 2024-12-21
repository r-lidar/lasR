#ifndef DRAWER_H
#define DRAWER_H

#include "PointSchema.h"

#include <SDL2/SDL.h>

#include "LODtree.h"
#include "camera.h"

class LAS;

class Drawer
{
public:
  enum AttributeEnum{Z, I, RGB, CLASS};

  Drawer(SDL_Window*, PointCloud*);
  bool draw();
  void resize();
  void setPointSize(float);
  void setAttribute(AttributeEnum x);
  void display_hide_spatial_index() { draw_index = !draw_index; camera.changed = true; };
  void display_hide_edl() { lightning = !lightning; camera.changed = true; };
  void point_size_plus() { point_size++; camera.changed = true; };
  void point_size_minus() { point_size--; camera.changed = true; };
  void budget_plus() { point_budget += 500000; camera.changed = true; };
  void budget_minus() { if (point_budget > 500000) point_budget -= 500000; camera.changed = true; };
  Camera camera;
  OctreeNode root;

  float point_size;
  bool lightning;

private:
  void edl();
  void compute_cell_visibility();
  void query_rendered_point();
  void traverse_and_collect(OctreeNode* octant, std::vector<const OctreeNode*>& visible_octants);
  void init_viewport();

  bool draw_index;
  uint32_t npoints;
  int point_budget;
  int rgb_norm;

  double minx;
  double miny;
  double minz;
  double maxx;
  double maxy;
  double maxz;
  double xcenter;
  double ycenter;
  double zcenter;
  double xrange;
  double yrange;
  double zrange;
  double range;
  double zqmin;
  double zqmax;
  double minattr;
  double maxattr;
  double attrrange;

  AttributeEnum attr;
  std::vector<Point> pp;
  std::vector<const OctreeNode*> visible_octants;

  SDL_Window *window;
  float zNear;
  float zFar;
  float fov;
  int width;
  int height;

  PointCloud* las;
  AttributeAccessor intensity;
  AttributeAccessor red;
  AttributeAccessor green;
  AttributeAccessor blue;
  AttributeAccessor classification;
};

#endif //DRAWER_H
