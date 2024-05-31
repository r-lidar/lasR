#include "pipeline.h"
#include "LAScatalog.h"

#include "addattribute.h"
#include "addrgb.h"
#include "boundaries.h"
#include "breakif.h"
#include "filter.h"
#include "loadraster.h"
#include "localmaximum.h"
#include "noiseivf.h"
#include "nothing.h"
#include "pitfill.h"
#include "rasterize.h"
#include "sampling.h"
#include "readlas.h"
#include "regiongrowing.h"
#include "setcrs.h"
#include "summary.h"
#include "svd.h"
#include "triangulate.h"
#include "transformwith.h"
#include "writelas.h"
#include "writelax.h"
#include "writevpc.h"

#ifdef USING_R
#include "R2cpp.h"
#include "aggregate.h"
#include "callback.h"
#include "readdataframe.h"
#endif

#include <algorithm>

// To parse JSON
#include <iostream>
#include <fstream>
#include "nlohmann/json.hpp"

static std::vector<std::string> get_strings(const nlohmann::json& element);
static std::vector<double> get_doubles(const nlohmann::json& element);

bool Pipeline::parse(const std::string& file, bool progress)
{
  // Open the JSON file
  std::ifstream fjson(file);
  if (!fjson.is_open())
  {
    last_error = "Could not open the json file containing the pipeline";
    return false;
  }
  nlohmann::json json;
  fjson >> json;
  int num_stages = json.size();

  // This is the extent of the coverage.
  // We don't know it yet. We need to parse a reader first
  double xmin = 0;
  double ymin = 0;
  double xmax = 0;
  double ymax = 0;

  bool reader = false;
  bool indexer = false;

  parsed = false;
  pipeline.clear();

  try
  {
    for (auto& [key, stage] : json.items())
    {
      std::string name = stage.at("algoname");
      std::string uid = stage.at("uid");

      // This stage is a placeholder stage added at the R level to carry the file paths and processing options
      // it adds no stage in the pipeline
      if (name == "build_catalog")
      {
        // This is the buffer provided by the user. The actual buffer may be larger
        // depending on the stages in the pipeline. User may provide 0 or 5 but the triangulation
        // stage tells us 50.
        buffer = stage.value("buffer", 0);

        // No element 'dataframe'? We are processing some files. Otherwise with have a compatibility layer
        // with lidR to process LAS objects
        if (!stage.contains("dataframe"))
        {
          std::vector<std::string> files = get_strings(stage["files"]);

          catalog = std::make_shared<LAScatalog>();
          if (!catalog->read(files, progress))
          {
            last_error = "In the parser while reading the file collection: " + last_error; // # nocov
            return false; // # nocov
          }

          if (stage.contains("noprocess"))
          {
            std::vector<bool> noproces = stage.at("noprocess");
            if (!catalog->set_noprocess(noproces))
            {
              last_error = "In the parser while reading the file collection: " + last_error; // # nocov
              return false; // # nocov
            }
          }

          // The catalog read all the files, we now know the extent of the coverage
          xmin = catalog->get_xmin();
          ymin = catalog->get_ymin();
          xmax = catalog->get_xmax();
          ymax = catalog->get_ymax();
        }
        else
        {
          std::string address_dataframe_str = stage.at("dataframe");
          SEXP dataframe = string_address_to_sexp(address_dataframe_str);

          std::string wkt = stage.at("crs");

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

          catalog = std::make_shared<LAScatalog>();
          catalog->add_bbox(xmin, ymin, xmax, ymax, Rf_length(X));
          catalog->set_crs(CRS(wkt));
        }
      }
      else if (name.substr(0,6) == "reader")
      {
        if (reader)
        {
          last_error = "The pipeline can only have a single reader stage";
          return false;
        }
        reader = true;

        if (name == "reader_las")
        {
          auto v = std::make_unique<LASRlasreader>();
          pipeline.push_back(std::move(v));
        }

        if (name == "reader_dataframe")
        {
          std::string address_dataframe_str = stage.at("dataframe");
          uintptr_t address_dataframe = strtoull(address_dataframe_str.c_str(), NULL, 16);
          SEXP dataframe = (SEXP)address_dataframe;
          std::vector<double> accuracy = stage.at("accuracy");
          std::string wkt = stage.at("crs");
          auto v = std::make_unique<LASRdataframereader>(xmin, ymin, xmax, ymax, dataframe, accuracy, wkt);
          pipeline.push_back(std::move(v));
        }

        if (catalog != nullptr)
        {
          // Special treatment of the reader to find the potential queries in the catalog
          if (stage.contains("xcenter"))
          {
            std::vector<double> xcenter = get_doubles(stage["xcenter"]);
            std::vector<double> ycenter = get_doubles(stage["ycenter"]);
            std::vector<double> radius = get_doubles(stage["radius"]);
            for (size_t j = 0 ; j <  xcenter.size() ; ++j) catalog->add_query(xcenter[j], ycenter[j], radius[j]);
          }

          if (stage.contains("xmin"))
          {
            std::vector<double> xmin = get_doubles(stage["xmin"]);
            std::vector<double> ymin = get_doubles(stage["ymin"]);
            std::vector<double> xmax = get_doubles(stage["xmax"]);
            std::vector<double> ymax = get_doubles(stage["ymax"]);
            for (size_t j = 0 ; j <  xmin.size() ; ++j) catalog->add_query(xmin[j], ymin[j], xmax[j], ymax[j]);
          }
        }
      }
      else if (name == "rasterize")
      {
        double res = stage.at("res");

        if (!stage.contains("connect"))
        {
          double window = stage.at("window");
          float default_value = stage.value("default_value", NA_F32_RASTER);
          std::vector<std::string> methods = get_strings(stage["method"]);

          auto v = std::make_unique<LASRrasterize>(xmin, ymin, xmax, ymax, res, window, methods, default_value);
          pipeline.push_back(std::move(v));
        }
        else
        {
          std::string uid = stage.at("connect");
          auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid; });
          if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

          LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->get());
          if (p)
          {
            auto v = std::make_unique<LASRrasterize>(xmin, ymin, xmax, ymax, res, p);
            pipeline.push_back(std::move(v));
          }
          else
          {
            last_error = "Incompatible stage combination for rasterize"; // # nocov
            return false; // # nocov
          }
        }
      }
      else if (name == "load_raster")
      {
        std::string file = stage.at("file");
        int band = stage.at("band");
        auto v = std::make_unique<LASRloadraster>(file, band);
        pipeline.push_back(std::move(v));
      }
      else if (name == "filter")
      {
        auto v = std::make_unique<LASRfilter>();
        pipeline.push_back(std::move(v));
      }
      else if (name == "local_maximum")
      {
        double ws = stage.at("ws");
        double min_height = stage.at("min_height");

        if (!stage.contains("connect"))
        {
          std::string use_attribute = stage.at("use_attribute");
          bool record_attributes = stage.at("record_attributes");
          auto v = std::make_unique<LASRlocalmaximum>(xmin, ymin, xmax, ymax, ws, min_height, use_attribute, record_attributes);
          pipeline.push_back(std::move(v));
        }
        else
        {
          std::string uid = stage.at("connect");
          auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid; });
          if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

          StageRaster* p = dynamic_cast<StageRaster*>(it->get());
          if (p)
          {
            auto v = std::make_unique<LASRlocalmaximum>(xmin, ymin, xmax, ymax, ws, min_height, p);
            pipeline.push_back(std::move(v));
          }
          else
          {
            last_error = "Incompatible stage combination for local_maximum"; // # nocov
            return false; // # nocov
          }
        }
      }
      else if (name == "summarise")
      {
        double zwbin = stage.at("zwbin");
        double iwbin = stage.at("iwbin");
        auto v = std::make_unique<LASRsummary>(xmin, ymin, xmax, ymax, zwbin, iwbin);
        pipeline.push_back(std::move(v));
      }
      else if (name == "triangulate")
      {
        double max_edge = stage.at("max_edge");
        std::string use_attribute = stage.at("use_attribute");
        auto v = std::make_unique<LASRtriangulate>(xmin, ymin, xmax, ymax, max_edge, use_attribute);
        pipeline.push_back(std::move(v));
      }
      else if (name == "write_las")
      {
        bool keep_buffer = stage.at("keep_buffer");
        auto v = std::make_unique<LASRlaswriter>(xmin, xmax, ymin, ymax, keep_buffer);
        pipeline.push_back(std::move(v));
      }
      else if (name == "write_lax")
      {
        indexer = true;
        bool embedded = stage.at("embedded");
        bool overwrite = stage.at("overwrite");
        auto v = std::make_unique<LASRlaxwriter>(embedded, overwrite, false);
        pipeline.push_back(std::move(v));
      }
      else if (name == "write_vpc")
      {
        bool absolute_path = stage.at("absolute");
        auto v = std::make_unique<LASRvpcwriter>(absolute_path);
        pipeline.push_back(std::move(v));
      }
      else if (name == "transform_with")
      {
        std::string uid = stage.at("connect");
        std::string op = stage.at("operator");
        std::string attr = stage.at("store_in_attribute");
        auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid; });
        if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

        LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->get());
        StageRaster* q = dynamic_cast<StageRaster*>(it->get());
        if (p)
        {
          auto v = std::make_unique<LASRtransformwith>(xmin, ymin, xmax, ymax, p, op, attr);
          pipeline.push_back(std::move(v));
        }
        else if(q)
        {
          auto v = std::make_unique<LASRtransformwith>(xmin, ymin, xmax, ymax, q, op, attr);
          pipeline.push_back(std::move(v));
        }
        else
        {
          last_error = "Incompatible stage combination for 'rasterize'"; // # nocov
          return false; // # nocov
        }
      }
      else if (name == "pit_fill")
      {
        std::string uid = stage.at("connect");
        int lap_size = stage.at("lap_size");
        float thr_lap = (float)stage.at("thr_lap");
        float thr_spk = (float)stage.at("thr_spk");
        int med_size = stage.at("med_size");
        int dil_radius = stage.at("dil_radius");

        auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid; });
        if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }

        StageRaster* p = dynamic_cast<StageRaster*>(it->get());
        if (p)
        {
          auto v = std::make_unique<LASRpitfill>(xmin, ymin, xmax, ymax, lap_size, thr_lap, thr_spk, med_size, dil_radius, p);
          pipeline.push_back(std::move(v));
        }
        else
        {
          last_error = "Incompatible stage combination for 'pit_fill'"; // # nocov
          return false; // # nocov
        }
      }
      else if (name  == "sampling_voxel")
      {
        double res = stage.at("res");
        auto v = std::make_unique<LASRsamplingvoxels>(xmin, ymin, xmax, ymax, res);
        pipeline.push_back(std::move(v));
      }
      else if (name  == "sampling_pixel")
      {
        double res = stage.at("res");
        auto v = std::make_unique<LASRsamplingpixels>(xmin, ymin, xmax, ymax, res);
        pipeline.push_back(std::move(v));
      }
      else if (name  == "set_crs")
      {
        int epsg = stage.at("epsg");
        std::string wkt = stage.at("wkt");
        if (epsg > 0)
        {
          auto v = std::make_unique<LASRsetcrs>(epsg);
          pipeline.push_back(std::move(v));
        }
        else
        {
          auto v = std::make_unique<LASRsetcrs>(wkt);
          pipeline.push_back(std::move(v));
        }
      }
      else if (name  == "svd")
      {
        int k = stage.at("k");
        double r = stage.at("r");
        std::string features = stage.at("features");
        auto v = std::make_unique<LASRsvd>(k, r, features);
        pipeline.push_back(std::move(v));
      }
      else if (name  == "region_growing")
      {
        double th_tree = stage.at("th_tree");
        double th_seed = stage.at("th_seed");
        double th_cr = stage.at("th_cr");
        double max_cr = stage.at("max_cr");

        std::string uid1 = stage.at("connect1");
        std::string uid2 = stage.at("connect2");
        auto it1 = std::find_if(pipeline.begin(), pipeline.end(), [&uid1](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid1; });
        if (it1 == pipeline.end()) { last_error = "Cannot find stage with this uid";  return false; }
        auto it2 = std::find_if(pipeline.begin(), pipeline.end(), [&uid2](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid2; });
        if (it2 == pipeline.end()) { last_error = "Cannot find stage with this uid";  return false; }

        LASRlocalmaximum* p = dynamic_cast<LASRlocalmaximum*>(it1->get());
        StageRaster* q = dynamic_cast<StageRaster*>(it2->get());
        if (p && q)
        {
          auto v = std::make_unique<LASRregiongrowing>(xmin, ymin, xmax, ymax,th_tree, th_seed, th_cr, max_cr, q, p);
          pipeline.push_back(std::move(v));
        }
        else
        {
          last_error = "Incompatible stage combination for 'region_growing'"; // # nocov
          return false; // # nocov
        }
      }
      else if (name == "hulls")
      {
        LASRtriangulate* p = nullptr;
        if (stage.contains("connect"))
        {
          std::string uid = stage.at("connect");
          auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid; });
          if (it == pipeline.end()) { last_error = "Cannot find stage with this uid"; return false; }
          p = dynamic_cast<LASRtriangulate*>(it->get());
          if (p == nullptr)
          {
            last_error = "Incompatible stage combination for 'hulls'"; // # nocov
            return false; // # nocov
          }
        }

        auto v = std::make_unique<LASRboundaries>(xmin, ymin, xmax, ymax, p);
        pipeline.push_back(std::move(v));
      }
      else if (name == "classify_isolated_points")
      {
        double res = stage.at("res");
        int n = stage.at("n");
        int classification = stage.at("class");
        auto v = std::make_unique<LASRnoiseivf>(xmin, ymin, xmax, ymax, res, n, classification);
        pipeline.push_back(std::move(v));
      }
      else if (name == "add_extrabytes")
      {
        std::string data_type = stage.at("data_type");
        std::string name = stage.at("name");
        std::string desc = stage.at("description");
        double scale = stage.at("scale");
        double offset = stage.at("offset");
        auto v = std::make_unique<LASRaddattribute>(data_type, name, desc, scale, offset);
        pipeline.push_back(std::move(v));
      }
      else if (name == "add_rgb")
      {
        auto v = std::make_unique<LASRaddrgb>();
        pipeline.push_back(std::move(v));
      }
      else if (name == "stop_if")
      {
        std::string condition = stage.at("condition");
        if (condition == "outside_bbox")
        {
          double minx = stage.at("xmin");
          double miny = stage.at("ymin");
          double maxx = stage.at("xmax");
          double maxy = stage.at("ymax");
          auto v = std::make_unique<LASRbreakoutsidebbox>(minx, miny, maxx, maxy);
          pipeline.push_back(std::move(v));
        }
        else
        {
          last_error = "Invalid condition in break_if";
          return false;
        }
      }
      else if (name == "nothing")
      {
        bool read = stage.at("read");
        bool stream = stage.at("stream");
        bool loop = stage.at("loop");

        auto v = std::make_unique<LASRnothing>(read, stream, loop);
        pipeline.push_back(std::move(v));
      }
      #ifdef USING_R
      else if (name == "aggregate")
      {
        std::string address_call_str = stage.at("call");
        std::string address_env_str = stage.at("env");
        SEXP call = string_address_to_sexp(address_call_str);
        SEXP env = string_address_to_sexp(address_env_str);

        double res = stage.at("res");
        int nmetrics = stage.at("nmetrics");
        double win = stage.at("window");
        auto v = std::make_unique<LASRaggregate>(xmin, ymin, xmax, ymax, res, nmetrics, win, call, env);
        pipeline.push_back(std::move(v));
      }
      else if (name  == "callback")
      {
        std::string expose = stage.at("expose");
        bool modify = !stage.at("no_las_update");
        bool drop_buffer = stage.at("drop_buffer");

        std::string address_fun_str = stage.at("fun");
        std::string address_args_str = stage.at("args");
        SEXP fun = string_address_to_sexp(address_fun_str);
        SEXP args = string_address_to_sexp(address_args_str);

        auto v = std::make_unique<LASRcallback>(xmin, ymin, xmax, ymax, expose, fun, args, modify, drop_buffer);
        pipeline.push_back(std::move(v));
      }
      #endif
      else
      {
        last_error = "Unsupported stage: " + std::string(name.c_str()); // # nocov
        return false; // # nocov
      }

      if (pipeline.size() > 0)
      {
        auto& it = pipeline.back();
        it->set_uid(uid);

        // If we intend to actually to process the point cloud we check that a reader stage is present if needed
        if (catalog != nullptr)
        {
          if (it->need_points() && !reader)
          {
            last_error = "The stage " + it->get_name() + " processes the point cloud but is not preceded by a reader stage";
            return false;
          }
        }
      }
    }

    parsed = true;

    for (auto&& stage : pipeline)
    {
      stage->set_ncpu(ncpu);
      stage->set_verbose(verbose);
    }

    // If build catalog == false we do not build a catalog and thus
    // it means that we do not have access to the files. The pipeline is parsed
    // but can't be executed and won't be executed. This happens only in get_pipeline_info()
    // that parses the pipeline in order to know if a buffer is needed or if the pipeline is
    // streamable.
    if (catalog != nullptr)
    {
      if (!reader)
      {
        last_error = "The pipeline must have a readers stage";
        return false;
      }

      num_stages--;
      catalog->set_buffer(need_buffer()); // We parsed the pipeline so we know if we need a buffer
      catalog->build_index(); // The catalog is built, we have the bbox of all the LAS files. We can build a spatial index

      // We iterate over all the stage again to assign the filter, the crs and the output file.
      // This is done here because set_output_file() does create a file on disk and we want
      // it to happen only if we plan to actually process something
      CRS current_crs = catalog->get_crs();
      auto it = pipeline.begin();
      int i = 0;
      for (auto& [key, stage] : json.items())
      {
        // Skip first stage that is build_catalog
        if (i == 0)
        {
          i++;
          continue;
        }

        std::string filter = stage.at("filter");
        std::string output = stage.at("output");

        // We set the CRS from the CRS of the catalog but then we get back the CRS. If we have
        // the 'set_crs' stage this updates the CRS assigned to the next stages
        const auto p = it->get();
        p->set_crs(current_crs);
        current_crs = p->get_crs();

        p->set_filter(filter);

        // Create empty files that will be filled later during the processing
        if (!p->set_output_file(output)) return false;

        it++;
      }

      // Write lax is the very first algorithm. Even before read_las. It is called
      // only if needed.
      if (!catalog->check_spatial_index() && !indexer)
      {
        auto v = std::make_unique<LASRlaxwriter>(false, false, true);
        pipeline.push_front(std::move(v));

        if (catalog->is_source_vpc())
          print("Impossible to determine if the point cloud is spatially indexed from this VPC file. Spatial indexing speeds up tile buffering and spatial queries drastically.\nFiles will be indexed on-the-fly if they are not. This may take some extra time now but will speed up everything later.\n");
        else
          print("%d files do not have a spatial index. Spatial indexing speeds up tile buffering and spatial queries drastically.\nFiles will be indexed on-the-fly. This will take some extra time now but will speed up everything later.\n", catalog->get_number_files()-catalog->get_number_indexed_files());
      }
    }
  }
  catch (const std::exception& e)
  {
    last_error = std::string("Error while parsing JSON pipeline: ") + e.what();
    return false;
  }

  streamable = is_streamable();
  buffer = MAX(buffer, need_buffer());
  read_payload = need_points();
  parallelizable = is_parallelizable();

  return true;
}

static std::vector<std::string> get_strings(const nlohmann::json& element)
{
  std::vector<std::string> result;
  if (element.is_array())
  {
    for (const auto& item : element)
    {
      result.push_back(item.get<std::string>());
    }
  }
  else
  {
    result.push_back(element.get<std::string>());
  }
  return result;
}

static std::vector<double> get_doubles(const nlohmann::json& element)
{
  std::vector<double> result;
  if (element.is_array())
  {
    for (const auto& item : element)
    {
      result.push_back(item.get<double>());
    }
  }
  else
  {
    result.push_back(element.get<double>());
  }
  return result;
}