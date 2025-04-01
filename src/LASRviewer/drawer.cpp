#include <GL/gl.h>
#include <GL/glu.h>

#include <chrono>
#include <random>
#include <algorithm>

#include "drawer.h"
#include "PSquare.h"
#include "PointCloud.h"
#include "Progress.h"

static const std::vector<std::array<unsigned char, 3>> zgradient = {
  {0, 0, 255},
  {0, 29, 252},
  {0, 59, 250},
  {0, 89, 248},
  {0, 119, 246},
  {0, 148, 244},
  {0, 178, 242},
  {0, 208, 240},
  {0, 238, 238},
  {31, 240, 208},
  {63, 242, 178},
  {95, 244, 148},
  {127, 246, 118},
  {159, 248, 89},
  {191, 250, 59},
  {223, 252, 29},
  {255, 255, 0},
  {255, 223, 0},
  {255, 191, 0},
  {255, 159, 0},
  {255, 127, 0},
  {255, 95, 0},
  {255, 63, 0},
  {255, 31, 0},
  {255, 0, 0}
};

static const std::vector<std::array<unsigned char, 3>> classcolor = {
  {211, 211, 211}, // [1]
  {211, 211, 211}, // [2]
  {0,   0,   255}, // [3]
  {50,  205, 50},  // [4]
  {34,  139, 34},  // [5]
  {0,   100, 0},   // [6]
  {255, 0,   0},   // [7]
  {255, 255, 0},   // [8]
  {255, 255, 0},   // [9]
  {100, 149, 237}, // [10]
  {255, 255, 0},   // [11]
  {51,  51,  51},  // [12]
  {255, 255, 0},   // [13]
  {255, 192, 203}, // [14]
  {255, 192, 203}, // [15]
  {160, 32,  240}, // [16]
  {255, 192, 203}, // [17]
  {255, 165, 0},   // [18]
  {255, 255, 0}    // [19]
};

static const std::vector<std::array<unsigned char, 3>> igradient = {
  {255,   0,   0},  // [1]
  {255,  14,   0},  // [2]
  {255,  28,   0},  // [3]
  {255,  42,   0},  // [4]
  {255,  57,   0},  // [5]
  {255,  71,   0},  // [6]
  {255,  85,   0},  // [7]
  {255,  99,   0},  // [8]
  {255, 113,   0},  // [9]
  {255, 128,   0},  // [10]
  {255, 142,   0},  // [11]
  {255, 156,   0},  // [12]
  {255, 170,   0},  // [13]
  {255, 184,   0},  // [14]
  {255, 198,   0},  // [15]
  {255, 213,   0},  // [16]
  {255, 227,   0},  // [17]
  {255, 241,   0},  // [18]
  {255, 255,   0},  // [19]
  {255, 255,  21},  // [20]
  {255, 255,  64},  // [21]
  {255, 255, 106},  // [22]
  {255, 255, 149},  // [23]
  {255, 255, 191},  // [24]
  {255, 255, 234}   // [25]
};

Drawer::Drawer(SDL_Window *window, PointCloud* las)
{
  zNear = 1;
  zFar = 100000;
  fov = 70;

  this->window = window;
  this->las = las;

  init_viewport();

  this->npoints = las->npoints;
  this->minx = las->header->min_x;
  this->miny = las->header->min_y;
  this->minz = las->header->min_z;
  this->maxx = las->header->max_x;
  this->maxy = las->header->max_y;
  this->maxz = las->header->max_z;
  this->xcenter = (maxx+minx)/2;
  this->ycenter = (maxy+miny)/2;
  this->zcenter = (maxz+minz)/2;
  this->xrange = maxx-minx;
  this->yrange = maxy-miny;
  this->zrange = maxz-minz;
  this->range = std::max(xrange, yrange);
  this->zqmin = minz-zcenter;
  this->zqmax = maxz-zcenter;

  this->draw_index = false;
  this->point_budget = 200000;
  this->point_size = 5.0;
  this->lightning = true;
  this->palette = zgradient;
  this->max_percentile = 0.99;
  this->min_percentile = 0.01;

  this->pp.reserve(this->point_budget*1.1);

  setAttribute(3);

  double distance = sqrt(xrange*xrange+yrange*yrange);
  this->camera.setDistance(distance);
  this->camera.setPanSensivity(distance*0.001);
  this->camera.setZoomSensivity(distance*0.05);

  auto start = std::chrono::high_resolution_clock::now();

  const Header& h = *las->header;
  double dx = h.max_x - h.min_x;
  double dy = h.max_y - h.min_y;
  double dz = h.max_z - h.min_z;
  double xc = (h.max_x + h.min_x)/2;
  double yc = (h.max_y + h.min_y)/2;
  double zc = (h.max_z + h.min_z)/2;
  double delta = MAX3(dx,dy,dz)

  root = OctreeNode(xc-delta, yc-delta, zc-delta, xc+delta, yc+delta, zc+delta, 7, 0);

  Progress progress;
  progress.set_total(las->npoints);
  progress.set_display(true);
  progress.set_prefix("LoD");

  int i = 0;
  unsigned int curr;
  while (las->read_point())
  {
    curr = las->current_point;
    root.insert(las->point);
    /*if (i % 1000000 == 0)
    {
      camera.changed = true;
      draw();
      las->seek(curr); // Draw is seeking and invalidates the index
    }*/
    progress++;
    progress.show();
    i++;
  }

  //print("n point at lvl 0 = %d\n", root.points.size());
  //root.print();

  progress.done(true);
  progress.show();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;
  //printf("Indexation: %.1lf seconds (%.1lfM pts/s)\n", duration.count(), las->npoints/duration.count()/1000000);

  this->point_budget *= 10;
  camera.changed = true;
  draw();
}

void Drawer::init_viewport()
{
  SDL_GetWindowSize(window, &width, &height);

  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(fov, (float)width/(float)height, zNear, zFar);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void Drawer::setAttribute(int index)
{
  if (index < 0 || index >= las->header->schema.num_attributes()) return; // invalid;
  if (attribute_index == index) return; // no need to redraw

  attribute_index = index;
  auto name = las->header->schema.attributes[index].name;

  get_attribute = AttributeAccessor(name);
  compute_attribute_range();

  if (name == "Z")
    palette = zgradient;
  else if (name == "Intensity")
    palette = igradient;
  else if (name == "Classification")
    palette = classcolor;
  else
    palette = zgradient;

  camera.changed = true;
}

void Drawer::setPointSize(float size)
{
  if (size < 0) return;
  if (point_size == size) return;

  this->point_size = size;
  camera.changed = true;
}

void Drawer::setPercentiles(float min, float max)
{
  if (min_percentile == min && max_percentile == max) return; // no update;
  if (min < 0 || min > 1) return; // invalid
  if (max < 0 || max > 1) return; // invalid
  if (max < min) return; // invalid

  max_percentile = max;
  min_percentile = min;
  compute_attribute_range();
  camera.changed = true;
}

void Drawer::setEDL(bool b)
{
  if (lightning == b) return;

  lightning = b;
  camera.changed = true;
}

void Drawer::setEDLstrength(float val)
{
  if (val < 0) return;
  if (edl_strengh == val) return;

  edl_strengh = val;
  camera.changed = true;
}

void Drawer::setBudget(int val)
{
  if (val < 0) return;
  if (point_budget == val) return;

  point_budget = val;
  camera.changed = true;
}

bool Drawer::draw()
{
  if (!camera.changed) return false;

  auto start = std::chrono::high_resolution_clock::now();

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   // Immediate mode. Should be modernized.
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glLineWidth(2.0f);
  glPointSize(this->point_size);

  camera.look(); // Reposition the camera after rotation and translation of the scene;

  auto start_query = std::chrono::high_resolution_clock::now();

  compute_cell_visibility();
  query_rendered_point();

  auto end_query = std::chrono::high_resolution_clock::now();
  auto start_rendering = std::chrono::high_resolution_clock::now();

  glBegin(GL_POINTS);

  for (auto p : pp)
  {
    float px = p.get_x()-xcenter;
    float py = p.get_y()-ycenter;
    float pz = p.get_z()-zcenter;
    double val = get_attribute(&p);

    val = (std::clamp(val, minattr, maxattr) - minattr) / (attrrange);
    int bin = std::min(static_cast<int>(val * (palette.size() - 1)), static_cast<int>(palette.size() - 1));
    auto& col = palette[bin];
    glColor3ub(col[0], col[1], col[2]);

    /*case AttributeEnum::RGB:
     {
     int pr = red(&p);
     int pg = green(&p);
     int pb = blue(&p);
     glColor3ub(pr/rgb_norm, pg/rgb_norm, pb/rgb_norm);
     break;
     }*/

    glVertex3d(px, py, pz);
  }

  glEnd();

  auto end_rendering = std::chrono::high_resolution_clock::now();

  auto start_edl = std::chrono::high_resolution_clock::now();
  if (lightning) edl();
  auto end_edl = std::chrono::high_resolution_clock::now();
  /*if (draw_index)
  {
    glColor3f(1.0f, 1.0f, 1.0f);

    for (const auto& octant : visible_octants)
    {
      float centerX = octant->bbox[0] - xcenter;
      float centerY = octant->bbox[1] - ycenter;
      float centerZ = octant->bbox[2] - zcenter;
      float halfSize = octant->bbox[3];

      float x0 = centerX - halfSize;
      float x1 = centerX + halfSize;
      float y0 = centerY - halfSize;
      float y1 = centerY + halfSize;
      float z0 = centerZ - halfSize;
      float z1 = centerZ + halfSize;

      glBegin(GL_LINES);

      // Bottom face
      glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
      glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
      glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
      glVertex3f(x0, y0, z1); glVertex3f(x0, y0, z0);

      // Top face
      glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
      glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
      glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
      glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);

      // Vertical edges
      glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0);
      glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
      glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
      glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1);

      glEnd();
    }
  }*/

  // Draw the X axis (red)
  glColor3f(1.0f, 0.0f, 0.0f);
  glBegin(GL_LINES);
  glVertex3f(minx-xcenter, miny-ycenter, minz-zcenter); // Start point of X axis
  glVertex3f(minx-xcenter+20, miny-ycenter, minz-zcenter);  // End point of X axis
  glEnd();

  // Draw the Y axis (green)
  glColor3f(0.0f, 1.0f, 0.0f);
  glBegin(GL_LINES);
  glVertex3f(minx-xcenter, miny-ycenter, minz-zcenter); // Start point of Y axis
  glVertex3f(minx-xcenter, miny-ycenter+20, minz-zcenter);  // End point of Y axis
  glEnd();

  // Draw the Z axis (blue)
  glColor3f(0.0f, 0.0f, 1.0f);
  glBegin(GL_LINES);
  glVertex3f(minx-xcenter, miny-ycenter, minz-zcenter+10); // Start point of Y axis
  glVertex3f(minx-xcenter, miny-ycenter, minz-zcenter+10);  // End point of Y axis
  glEnd();

  camera.changed = false;

  auto end = std::chrono::high_resolution_clock::now();

  // Calculate the duration and stats
  std::chrono::duration<double> total_duration = end - start;
  std::chrono::duration<double> query_duration = end_query - start_query;
  std::chrono::duration<double> rendering_duration = end_rendering - start_rendering;
  std::chrono::duration<double> edl_duration = end_edl - start_edl;

  query_time = query_duration.count();
  rendering_time = rendering_duration.count();
  edl_time = edl_duration.count();
  total_time = total_duration.count();
  rendered_points_count = pp.size();

  /*printf("Displayed %dk/%ldk points (%.1f\%)\n", (int)pp.size()/1000, (long)las->npoints/1000, (double)pp.size()/(double)las->npoints*100);
  printf("Full Rendering: %.3f seconds (%.1f fps)\n", total_time, 1.0f/total_time);
  printf("Cloud rendering: %.3f seconds (%.1f fps, %.1f\%)\n", rendering_time, 1.0f/rendering_time, rendering_time/total_time*100);
  printf("Spatial query: %.3f seconds (%.1f fps %.1f\%)\n", query_time, 1.0f/query_time, query_time/total_time*100);
  printf("\n");*/

  glFlush();
  //SDL_GL_SwapWindow(window);

  return true;
}

#include <vector>
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <GL/gl.h>

void Drawer::edl()
{
  // Step 1: Read Depth and Color Buffers
  std::vector<GLfloat> depth(width * height);
  glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());

  std::vector<GLubyte> colorBuffer(width * height * 3);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, colorBuffer.data());

  const float zNear = 1;
  const float zFar = 10000;
  const float logzFar = std::log2(zFar);

  // Step 2: Compute Log Distances in Parallel
  std::vector<GLfloat> worldLogDistances(width * height);
  #pragma omp parallel for
  for (int i = 0; i < width * height; ++i)
  {
    GLfloat z = depth[i];
    GLfloat zNDC = 2.0f * z - 1.0f;
    GLfloat zCamera = (2.0f * zNear * zFar) / (zFar + zNear - zNDC * (zFar - zNear));
    worldLogDistances[i] = std::log2(zCamera);
  }

  // Precompute neighbor offsets for 1D indexing
  std::vector<int> neighborOffsets = {
    -width - 1, -width, -width + 1,
    -1,          1,
    width - 1,  width,  width + 1
  };

  // Step 3: Compute Shading in Parallel
  #pragma omp parallel for collapse(2)
  for (int y = 0; y < height; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      int idx = y * width + x;
      float wld = worldLogDistances[idx];

      if (wld == logzFar) continue;

      GLfloat maxLogDepth = std::max(0.0f, wld);

      GLfloat sum = 0.0f;
      for (int offset : neighborOffsets)
      {
        int nx = x + (offset % width);
        int ny = y + (offset / width);

        if (nx >= 0 && nx < width && ny >= 0 && ny < height)
        {
          int nIdx = ny * width + nx;
          sum += maxLogDepth - worldLogDistances[nIdx];
        }
      }

      float response = sum / 4.0f;
      float shade = std::exp(-response * 300.0f * edl_strengh);
      shade = 1.0f - std::clamp(shade, 0.0f, 255.0f) / 255.0f;

      colorBuffer[idx * 3] *= shade;
      colorBuffer[idx * 3 + 1] *= shade;
      colorBuffer[idx * 3 + 2] *= shade;
    }
  }

  // Step 4: Draw the Updated Color Buffer
  glDrawPixels(width, height, GL_RGB, GL_UNSIGNED_BYTE, colorBuffer.data());
}


void Drawer::resize()
{
  SDL_GetWindowSize(window, &width, &height);

  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(fov, (float)width/(float)height, zNear, zFar);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  camera.changed = true;
}

void Drawer::compute_cell_visibility()
{
  visible_octants.clear();

  traverse_and_collect(&root, visible_octants);

  std::sort(visible_octants.begin(), visible_octants.end(), [](const OctreeNode* a, const OctreeNode* b)
  {
    return a->screen_size > b->screen_size;  // Sort in descending order
  });
}

void Drawer::compute_attribute_range()
{
  PSquare p01(min_percentile);
  PSquare p99(max_percentile);
  int jump = 1;
  if (npoints > 100000) jump = npoints/100;
  if (npoints > 1000000) jump = npoints/1000;

  for (int i = 0 ; i < npoints - jump; i += jump)
  {
    las->seek(i);
    p01.addDataPoint(get_attribute(&las->point));
    p99.addDataPoint(get_attribute(&las->point));
  }

  minattr = p01.getQuantile();
  maxattr = p99.getQuantile();
  attrrange = maxattr - minattr;
  if (attrrange == 0) attrrange = EPSILON;
}

void Drawer::traverse_and_collect(OctreeNode* node, std::vector<const OctreeNode*>& visible_octants)
{
  if (node == nullptr) return;

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  int screenWidth = viewport[2];
  int screenHeight = viewport[3];

  float fov = 70 * M_PI / 180;
  float slope = std::tan(fov / 2.0f);

  // Calculate the screen size or other criteria for visibility
  double x = (node->min_x + node->max_x) / 2.0 - xcenter;
  double y = (node->min_y + node->max_y) / 2.0 - ycenter;
  double z = (node->min_z + node->max_z) / 2.0 - zcenter;
  double r = (node->max_x - node->min_x) * 1.414; // Adjusted for diagonal size of the node

  double cx = camera.x;
  double cy = camera.y;
  double cz = camera.z;

  // Check if the current node is visible
  if (camera.see(x,y,z, r))
  {
    float distance = std::sqrt((cx - x) * (cx - x) + (cy - y) * (cy - y) + (cz - z) * (cz - z));
    double screen_size = (screenHeight / 2.0) * (r / (slope * distance));
    node->screen_size = screen_size;

    if (screen_size > 50)
    {
      visible_octants.push_back(node);

      for (const auto& child : node->children)
        traverse_and_collect(child, visible_octants);
    }
  }
}

void Drawer::query_rendered_point()
{
  pp.clear();

  unsigned int n = 0;
  for (const auto octant : visible_octants)
  {
    n += octant->points.size();
    pp.insert(pp.end(), octant->points.begin(), octant->points.end());
    if (n > point_budget) break;
  }
}
