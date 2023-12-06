#include "pipeline.h"
#include "LAScatalog.h"

#include "addattribute.h"
#include "boundaries.h"
#include "localmaximum.h"
#include "noiseivf.h"
#include "nothing.h"
#include "pitfill.h"
#include "rasterize.h"
#include "sampling.h"
#include "readlas.h"
#include "regiongrowing.h"
#include "summary.h"
#include "triangulate.h"
#include "triangulatedtransformer.h"
#include "writelas.h"
#include "writelax.h"

#ifdef USING_R
#include "R2cpp.h"
#include "aggregate.h"
#include "callback.h"
#include "readdataframe.h"
#endif

bool Pipeline::parse(const SEXP sexpargs, bool build_catalog)
{
  // This is the extent of the coverage.
  // We don't know it yet. We need to parse a reader first
  double xmin = 0;
  double ymin = 0;
  double xmax = 0;
  double ymax = 0;

  parsed = false;
  pipeline.clear();
  delete catalog;
  catalog = nullptr;

  for (auto i = 0; i < Rf_length(sexpargs); ++i)
  {
    SEXP stage = VECTOR_ELT(sexpargs, i);

    std::string name   = get_element_as_string(stage, "algoname");
    std::string uid    = get_element_as_string(stage, "uid");
    std::string filter = get_element_as_string(stage, "filter");
    std::string output = get_element_as_string(stage, "output");

    if (name == "reader_las")
    {
      if (i != 0)
      {
        last_error = "The reader must alway be the first stage of the pipeline.";
        return false;
      }

      // Create a reader stage
      auto v = std::make_shared<LASRlasreader>();
      pipeline.push_back(v);

      // This is the buffer provided by the user. The actual buffer may be larger
      // depending on the stages in the pipeline. User may provide 0 or 5 but the triangulation
      // stage tells us 50.
      buffer = get_element_as_double(stage, "buffer");

      // The reader stage provides the files we will read. We can build a LAScatalog.
      // If we do not build a LAScatalog we can parse the pipeline anyway but all stages
      // will be initialized with an extent of [0,0,0,0] because we do not know the bbox yet.
      if (build_catalog)
      {
        std::vector<std::string> files = get_element_as_vstring(stage, "files");

        catalog = new LAScatalog;
        for (auto& file : files)
        {
          if (!catalog->add_file(file))
          {
            last_error = catalog->last_error; // # nocov
            return false; // # nocov
          }
        }

        // The catalog read all the files, we now know the extent of the coverage
        xmin = catalog->xmin;
        ymin = catalog->ymin;
        xmax = catalog->xmax;
        ymax = catalog->ymax;

        // Special treatment of the reader to find the potential queries in the catalogue
        // TODO: xcenter, ycenter, radius, xmin, ymin and ... have the same size it is checked at R level but should be tested here.
        if (contains_element(stage, "xcenter"))
        {
          std::vector<double> xcenter = get_element_as_vdouble(stage, "xcenter");
          std::vector<double> ycenter = get_element_as_vdouble(stage, "ycenter");
          std::vector<double> radius = get_element_as_vdouble(stage, "radius");
          for (auto j = 0 ; j <  xcenter.size() ; ++j) catalog->add_query(xcenter[j], ycenter[j], radius[j]);
        }

        if (contains_element(stage, "xmin"))
        {
          std::vector<double> xmin = get_element_as_vdouble(stage, "xmin");
          std::vector<double> ymin = get_element_as_vdouble(stage, "ymin");
          std::vector<double> xmax = get_element_as_vdouble(stage, "xmax");
          std::vector<double> ymax = get_element_as_vdouble(stage, "ymax");
          for (auto j = 0 ; j <  xmin.size() ; ++j) catalog->add_query(xmin[j], ymin[j], xmax[j], ymax[j]);
        }
      }
    }
    #ifdef USING_R
    else if (name == "reader_dataframe")
    {
      if (i != 0)
      {
        last_error = "The reader must alway be the first stage of the pipeline.";
        return false;
      }

      // This is the buffer provided by the user. The actual buffer may be larger
      // depending on the stages in the pipeline. User may provide 0 or 5 but the triangulation
      // stage tells us 50. But in this case it is useful only for queries since there is no files
      // and especially no neigbor files
      buffer = get_element_as_double(stage, "buffer");

      SEXP dataframe = get_element(stage, "dataframe");
      std::vector<double> accuracy = get_element_as_vdouble(stage, "accuracy");

      // Compute the bounding box. We assume that the data.frame has element named X and Y.
      // This is not checked at R level. Anyway get_element() will throw an exception
      SEXP X = get_element(dataframe, "X");
      SEXP Y = get_element(dataframe, "Y");
      xmin = F64_MAX;
      ymin = F64_MAX;
      xmax = F64_MIN;
      ymax = F64_MIN;
      for (int k = 0 ; k < Rf_length(X) ; ++k)
      {
        if (REAL(X)[k] < xmin) xmin = REAL(X)[k];
        if (REAL(Y)[k] < ymin) ymin = REAL(Y)[k];
        if (REAL(X)[k] > xmax) xmax = REAL(X)[k];
        if (REAL(Y)[k] > ymax) ymax = REAL(Y)[k];
      }

      auto v = std::make_shared<LASRdataframereader>(xmin, ymin, xmax, ymax, dataframe, accuracy);
      pipeline.push_back(v);

      if (build_catalog)
      {
        catalog = new LAScatalog;
        catalog->add_bbox(xmin, ymin, xmax, ymax, true);
        catalog->npoints = Rf_length(X);

        // Special treatment of the reader to find the potential queries in the catalog
        if (contains_element(stage, "xcenter"))
        {
          std::vector<double> xcenter = get_element_as_vdouble(stage, "xcenter");
          std::vector<double> ycenter = get_element_as_vdouble(stage, "ycenter");
          std::vector<double> radius = get_element_as_vdouble(stage, "radius");
          for (auto j = 0 ; j <  xcenter.size() ; ++j) catalog->add_query(xcenter[j], ycenter[j], radius[j]);
        }

        if (contains_element(stage, "xmin"))
        {
          std::vector<double> xmin = get_element_as_vdouble(stage, "xmin");
          std::vector<double> ymin = get_element_as_vdouble(stage, "ymin");
          std::vector<double> xmax = get_element_as_vdouble(stage, "xmax");
          std::vector<double> ymax = get_element_as_vdouble(stage, "ymax");
          for (auto j = 0 ; j <  xmin.size() ; ++j) catalog->add_query(xmin[j], ymin[j], xmax[j], ymax[j]);
        }
      }
    }
    #endif
    else if (name == "rasterize")
    {
      double res = get_element_as_double(stage, "res");

      if (!contains_element(stage, "connect"))
      {
        std::vector<int> methods = get_element_as_vint(stage, "method");
        auto v = std::make_shared<LASRrasterize>(xmin, ymin, xmax, ymax, res, methods);
        pipeline.push_back(v);
      }
      else
      {
        std::string uid = get_element_as_string(stage, "connect");
        auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::shared_ptr<LASRalgorithm>& obj) { return obj->get_uid() == uid; });
        if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

        LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->get());
        if (p)
        {
          auto v = std::make_shared<LASRrasterize>(xmin, ymin, xmax, ymax, res, p);
          pipeline.push_back(v);
        }
        else
        {
          last_error = "Incompatible stage combination for rasterize";
          return false;
        }
      }
    }
    else if (name == "local_maximum")
    {
      double ws = get_element_as_double(stage, "ws");
      double min_height = get_element_as_double(stage, "min_height");
      std::string use_attribute = get_element_as_string(stage, "use_attribute");
      auto v = std::make_shared<LASRlocalmaximum>(xmin, ymin, xmax, ymax, ws, min_height, use_attribute);
      pipeline.push_back(v);
    }
    else if (name == "summarise")
    {
      double zwbin = get_element_as_double(stage, "zwbin");
      double iwbin = get_element_as_double(stage, "iwbin");
      auto v = std::make_shared<LASRsummary>(xmin, ymin, xmax, ymax, zwbin, iwbin);
      pipeline.push_back(v);
    }
    else if (name == "triangulate")
    {
      double max_edge = get_element_as_double(stage, "max_edge");
      std::string use_attribute = get_element_as_string(stage, "use_attribute");
      auto v = std::make_shared<LASRtriangulate>(xmin, ymin, xmax, ymax, max_edge, use_attribute);
      pipeline.push_back(v);
    }
    else if (name == "write_las")
    {
      bool keep_buffer = get_element_as_bool(stage, "keep_buffer");
      auto v = std::make_shared<LASRlaswriter>(xmin, xmax, ymin, ymax, keep_buffer);
      pipeline.push_back(v);
    }
    else if (name == "write_lax")
    {
      auto v = std::make_shared<LASRlaxwriter>();
      pipeline.push_back(v);
    }
    else if (name == "transform_with_triangulation")
    {
      std::string uid = get_element_as_string(stage, "connect");
      std::string op = get_element_as_string(stage, "operator");
      std::string attr = get_element_as_string(stage, "store_in_attribute");
      auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::shared_ptr<LASRalgorithm>& obj) { return obj->get_uid() == uid; });
      if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

      LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->get());
      if (p)
      {
        auto v = std::make_shared<LASRtriangulatedTransformer>(xmin, ymin, xmax, ymax, p, op, attr);
        pipeline.push_back(v);
      }
      else
      {
        last_error = "Incompatible stage combination for 'rasterize'";
        return false;
      }
    }
    else if (name == "pit_fill")
    {
      std::string uid = get_element_as_string(stage, "connect");
      int lap_size = get_element_as_int(stage, "lap_size");
      float thr_lap = (float)get_element_as_double(stage, "thr_lap");
      float thr_spk = (float)get_element_as_double(stage, "thr_spk");
      int med_size = get_element_as_int(stage, "med_size");
      int dil_radius = get_element_as_int(stage, "dil_radius");

      auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::shared_ptr<LASRalgorithm>& obj) { return obj->get_uid() == uid; });
      if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

      LASRrasterize * p = dynamic_cast<LASRrasterize*>(it->get());
      if (p)
      {
        auto v = std::make_shared<LASRpitfill>(xmin, ymin, xmax, ymax, lap_size, thr_lap, thr_spk, med_size, dil_radius, p);
        pipeline.push_back(v);
      }
      else
      {
        last_error = "Incompatible stage combination for 'rasterize'";
        return false;
      }
    }
    else if (name  == "sampling_voxel")
    {
      double res = get_element_as_double(stage, "res");
      auto v = std::make_shared<LASRsamplingvoxels>(xmin, ymin, xmax, ymax, res);
      pipeline.push_back(v);
    }
    else if (name  == "sampling_pixel")
    {
      double res = get_element_as_double(stage, "res");
      auto v = std::make_shared<LASRsamplingpixels>(xmin, ymin, xmax, ymax, res);
      pipeline.push_back(v);
    }
    else if (name  == "region_growing")
    {
      double th_tree = get_element_as_double(stage, "th_tree");
      double th_seed = get_element_as_double(stage, "th_seed");
      double th_cr = get_element_as_double(stage, "th_cr");
      double max_cr = get_element_as_double(stage, "max_cr");

      std::string uid1 = get_element_as_string(stage, "connect1");
      std::string uid2 = get_element_as_string(stage, "connect2");
      auto it1 = std::find_if(pipeline.begin(), pipeline.end(), [&uid1](const std::shared_ptr<LASRalgorithm>& obj) { return obj->get_uid() == uid1; });
      if (it1 == pipeline.end()) { last_error = "Cannot find stage with this uid";  return false; }
      auto it2 = std::find_if(pipeline.begin(), pipeline.end(), [&uid2](const std::shared_ptr<LASRalgorithm>& obj) { return obj->get_uid() == uid2; });
      if (it2 == pipeline.end()) { last_error = "Cannot find stage with this uid";  return false; }

      LASRlocalmaximum* p = dynamic_cast<LASRlocalmaximum*>(it1->get());
      LASRalgorithmRaster* q = dynamic_cast<LASRalgorithmRaster*>(it2->get());
      if (p && q)
      {
        auto v = std::make_shared<LASRregiongrowing>(xmin, ymin, xmax, ymax,th_tree, th_seed, th_cr, max_cr, q, p);
        pipeline.push_back(v);
      }
      else
      {
        last_error = "Incompatible stage combination for 'region_growing'";
        return false;
      }
    }
    else if (name == "hulls")
    {
      LASRtriangulate* p = nullptr;
      if (contains_element(stage, "connect"))
      {
        std::string uid = get_element_as_string(stage, "connect");
        auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::shared_ptr<LASRalgorithm>& obj) { return obj->get_uid() == uid; });
        if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }
        p = dynamic_cast<LASRtriangulate*>(it->get());
        if (p == nullptr)
        {
          last_error = "Incompatible stage combination for 'boundaries'";
          return false;
        }
      }

      auto v = std::make_shared<LASRboundaries>(xmin, ymin, xmax, ymax, p);
      pipeline.push_back(v);
    }
    else if (name == "classify_isolated_points")
    {
      double res = get_element_as_double(stage, "res");
      int n = get_element_as_int(stage, "n");
      int classification = get_element_as_int(stage, "class");
      auto v = std::make_shared<LASRnoiseivf>(xmin, ymin, xmax, ymax, res, n, classification);
      pipeline.push_back(v);
    }
    else if (name == "add_extrabytes")
    {
      std::string data_type = get_element_as_string(stage, "data_type");
      std::string name = get_element_as_string(stage, "name");
      std::string desc = get_element_as_string(stage, "description");
      double scale = get_element_as_double(stage, "scale");
      double offset = get_element_as_double(stage, "offset");
      auto v = std::make_shared<LASRaddattribute>(data_type, name, desc, scale, offset);
      pipeline.push_back(v);
    }
    else if (name == "nothing")
    {
      auto v = std::make_shared<LASRnothing>();
      pipeline.push_back(v);
    }
    #ifdef USING_R
    else if (name == "aggregate")
    {
      SEXP call = get_element(stage, "call");
      SEXP env = get_element(stage, "env");
      double res = get_element_as_double(stage, "res");
      auto v = std::make_shared<LASRaggregate>(xmin, ymin, xmax, ymax, res, call, env);
      pipeline.push_back(v);
    }
    else if (name  == "callback")
    {
      std::string expose = get_element_as_string(stage, "expose");
      bool modify = !get_element_as_bool(stage, "no_las_update");
      bool drop_buffer = get_element_as_bool(stage, "drop_buffer");
      SEXP fun = get_element(stage, "fun");
      SEXP args = get_element(stage, "args");
      auto v = std::make_shared<LASRcallback>(xmin, ymin, xmax, ymax, expose, fun, args, modify, drop_buffer);
      pipeline.push_back(v);
    }
    #endif
    else
    {
      last_error = "Unsupported stage: " + std::string(name.c_str()); // # nocov
      return false; // # nocov
    }

    auto it = pipeline.back();
    it->set_uid(uid);
    it->set_filter(filter);
    it->set_output_file(output);
  }

  parsed = true;
  streamable = is_streamable();
  buffer = need_buffer();
  read_payload = need_points();

  for (auto&& stage : pipeline)
  {
    stage->set_ncpu(ncpu);
    stage->set_verbose(verbose);
  }

  if (build_catalog)
  {
    // Check that the first stage is a reader
    if (pipeline.front()->get_name().substr(0, 6) != "reader")
    {
      last_error = "The pipeline must start with a readers";
      return false;
    }

    // We parse the pipeline so we know if we need a buffer
    catalog->set_buffer(need_buffer());

    // The calog is build, we know the CRS of the collection
    set_crs(catalog->epsg);
    set_crs(catalog->wkt);

    // Write lax is the very first algorithm. Even before read_las. It is called
    // only if needed.
    if (!catalog->check_spatial_index())
    {
      auto v = std::make_shared<LASRlaxwriter>();
      pipeline.push_front(v);
      print("%d files do not have a spatial index. Spatial indexing speeds up tile buffering and spatial queries drastically.\nFiles will be indexed on-the-fly. This will take some extra time now but will speed up everything later.\n",
             catalog->get_number_files()-catalog->get_number_indexed_files());
    }
  }

  return true;
}