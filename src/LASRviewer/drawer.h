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
  Drawer(SDL_Window*, PointCloud*);
  bool draw();
  void resize();
  void setPointSize(float);
  void setAttribute(int index);
  void setPercentiles(float min, float max);
  void setEDL(bool b);
  void setEDLstrength(float val);
  void setBudget(int budget);
  void display_hide_spatial_index() { draw_index = !draw_index; camera.changed = true; };
  void point_size_plus() { point_size++; camera.changed = true; };
  void point_size_minus() { point_size--; camera.changed = true; };
  void budget_plus() { point_budget += 500000; camera.changed = true; };
  void budget_minus() { if (point_budget > 500000) point_budget -= 500000; camera.changed = true; };
  Camera camera;
  OctreeNode root;

  // Rendering stats
  int rendered_points_count;
  float query_time;
  float rendering_time;
  float edl_time;
  float total_time;

private:
  // Query points
  void compute_cell_visibility();
  void query_rendered_point();
  void traverse_and_collect(OctreeNode* octant, std::vector<const OctreeNode*>& visible_octants);
  std::vector<Point> pp;
  std::vector<const OctreeNode*> visible_octants;

  void init_viewport();
  void compute_attribute_range();

  void edl();

  // Location of the scene
  uint32_t npoints;
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

  // Rendering size, location and depth
  SDL_Window *window;
  float zNear;
  float zFar;
  float fov;
  int width;
  int height;

  // Drawing state
  float max_percentile;
  float min_percentile;
  float point_size;
  bool lightning;
  bool draw_index;
  int point_budget;
  int rgb_norm;
  float edl_strengh;
  std::vector<std::array<unsigned char, 3>> palette;


  // Point cloud and accessors
  PointCloud* las;
  AttributeAccessor get_attribute;
  int attribute_index;
};

#endif //DRAWER_H
