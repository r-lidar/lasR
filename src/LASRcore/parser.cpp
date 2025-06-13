#include <algorithm>
#include <limits>

#include "Engine.h"
#include "FileCollection.h"

#include "addattribute.h"
#include "addrgb.h"
#include "boundaries.h"
#include "breakif.h"
#include "csf.h"
#include "edit.h"
#include "filter.h"
#include "focal.h"
#include "info.h"
#include "ivf.h"
#include "loadmatrix.h"
#include "loadraster.h"
#include "localmaximum.h"
#include "nnmetrics.h"
#include "nothing.h"
#include "pitfill.h"
#include "rasterize.h"
#include "sampling.h"
#include "readlas.h"
#include "readpcd.h"
#include "regiongrowing.h"
#include "setcrs.h"
#include "sor.h"
#include "sort.h"
#include "summary.h"
#include "svd.h"
#include "triangulate.h"
#include "transformwith.h"
#include "writelas.h"
#include "writelax.h"
#include "writevpc.h"
#include "writepcd.h"

// If compiled as an R package include R's header, special R stages and helper functions
#ifdef USING_R

#define R_NO_REMAP 1
#include <R.h>
#include <Rinternals.h>

#include "aggregate.h"
#include "callback.h"
#include "readdataframe.h"
#include "xptr.h"

static SEXP get_element(SEXP list, const char *str)
{
  SEXP elmt = R_NilValue;
  SEXP names = Rf_getAttrib(list, R_NamesSymbol);
  for (int i = 0 ; i < Rf_length(list) ; i++) { if(strcmp(CHAR(STRING_ELT(names, i)), str) == 0)  { elmt = VECTOR_ELT(list, i); break; }}
  if (Rf_isNull(elmt)) throw std::string("element '") + str +  "' not found"; // # nocov
  return elmt;
}
#endif

template <typename T>
static std::unique_ptr<Stage> create_instance()
{
  return std::make_unique<T>();
}

bool Engine::parse(const nlohmann::json& json, bool progress)
{
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

  // Create a map of type names to functions that create instances
  std::unordered_map<std::string, std::function<std::unique_ptr<Stage>()>> factory_map =
  {
    {"add_attribute",        create_instance<LASRaddattribute>},
    {"add_rgb",              create_instance<LASRaddrgb>},
    {"classify_with_csf",    create_instance<LASRcsf>},
    {"classify_with_ivf",    create_instance<LASRivf>},
    {"classify_with_sor",    create_instance<LASRsor>},
    {"edit_attribute",       create_instance<LASRedit>},
    {"filter",               create_instance<LASRfilter>},
    {"filter_grid",          create_instance<LASRfiltergrid>},
    {"focal",                create_instance<LASRfocal>},
    {"hulls",                create_instance<LASRboundaries>},
    {"info",                 create_instance<LASRinfo>},
    {"load_matrix",          create_instance<LASRloadmatrix>},
    {"load_raster",          create_instance<LASRloadraster>},
    {"local_maximum",        create_instance<LASRlocalmaximum>},
    {"neighborhood_metrics", create_instance<LASRnnmetrics>},
    {"nothing",              create_instance<LASRnothing>},
    {"pit_fill",             create_instance<LASRpitfill>},
    {"rasterize",            create_instance<LASRrasterize>},
    {"remove_attribute",     create_instance<LASRremoveattribute>},
    {"sampling_pixel",       create_instance<LASRsamplingpixels>},
    {"sampling_poisson",     create_instance<LASRsamplingpoisson>},
    {"sampling_voxel",       create_instance<LASRsamplingvoxels>},
    {"set_crs",              create_instance<LASRsetcrs>},
    {"sort",                 create_instance<LASRsort>},
    {"summarise",            create_instance<LASRsummary>},
    {"svd",                  create_instance<LASRsvd>},
    {"transform_with",       create_instance<LASRtransformwith>},
    {"triangulate",          create_instance<LASRtriangulate>},
    {"write_las",            create_instance<LASRlaswriter>},
    {"write_vpc",            create_instance<LASRvpcwriter>},
    {"write_pcd",            create_instance<LASRpcdwriter>}
    #ifdef USING_R
    ,{"aggregate",           create_instance<LASRaggregate>},
    {"callback",             create_instance<LASRcallback>},
    {"xptr",                 create_instance<LASRxptr>}
    #endif
  };

  try
  {
    for (auto& [key, stage] : json.items())
    {
      std::string name = stage.at("algoname");
      std::string uid = stage.value("uid", "xxx-xxx");

      if (name == "reader_las") name = "reader"; // for backward compatibility with Drawflow

      auto iter = factory_map.find(name);
      if (iter != factory_map.end())
      {
        pipeline.push_back(iter->second());

        if (name == "xptr") point_cloud_ownership_transfered = true;
      }
      else if (name == "build_catalog")
      {
        // This stage is a placeholder stage added at the R level to carry the file paths and processing options
        // it adds no stage in the pipeline

        // This is the buffer provided by the user. The actual buffer may be larger
        // depending on the stages in the pipeline. User may provide 0 or 5 but the triangulation
        // stage tells us 50.
        buffer = stage.value("buffer", 0.0);
        chunk_size = stage.value("chunk", 0.0);
        std::string type = stage.value("type", "files");

        // We are processing some files. Otherwise with have a compatibility layer
        // with lidR to process LAS objects
        if (type == "files")
        {
          std::vector<std::string> files = get_vector<std::string>(stage["files"]);

          catalog = std::make_shared<FileCollection>();
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
        #ifdef USING_R
        else if (type == "dataframe")
        {
          std::string address_dataframe_str = stage.at("dataframe");
          SEXP dataframe = string_address_to_sexp(address_dataframe_str);

          std::string wkt = stage.at("crs");

          // Compute the bounding box. We assume that the data.frame has element named X and Y.
          // This is not checked at R level. Anyway get_element() will throw an exception
          SEXP X = get_element(dataframe, "X");
          SEXP Y = get_element(dataframe, "Y");
          xmin = std::numeric_limits<double>::max();
          ymin = std::numeric_limits<double>::max();
          xmax = -std::numeric_limits<double>::max();
          ymax = -std::numeric_limits<double>::max();
          for (int k = 0 ; k < Rf_length(X) ; ++k)
          {
            if (REAL(X)[k] < xmin) xmin = REAL(X)[k];
            if (REAL(Y)[k] < ymin) ymin = REAL(Y)[k];
            if (REAL(X)[k] > xmax) xmax = REAL(X)[k];
            if (REAL(Y)[k] > ymax) ymax = REAL(Y)[k];
          }

          catalog = std::make_shared<FileCollection>();
          catalog->add_dataframe(xmin, ymin, xmax, ymax, Rf_length(X));
          catalog->set_crs(CRS(wkt));
        }
        else if (type == "externalptr")
        {
          std::string address_ptr = stage.at("externalptr");
          SEXP sexplas = string_address_to_sexp(address_ptr);
          las = static_cast<PointCloud*>(R_ExternalPtrAddr(sexplas));
          if (las == nullptr)
          {
            last_error = "invalid external pointer";
            return false;
          }
          xmin = las->header->min_x;
          ymin = las->header->min_y;
          xmax = las->header->max_x;
          ymax = las->header->max_y;

          catalog = std::make_shared<FileCollection>();
          catalog->add_xptr(*las->header);
        }
        #endif
        else
        {
          last_error = "Internal error: bad build_catalog stage";
          return false;
        }
      }
      else if (name == "reader")
      {
        if (reader)
        {
          last_error = "The pipeline can only have a single reader stage";
          return false;
        }
        reader = true;

        if (catalog != nullptr)
        {
          switch(catalog->get_format())
          {
            case LASFILE:
            {
              auto v = std::make_unique<LASRlasreader>();
              pipeline.push_back(std::move(v));
              break;
            }
            case PCDFILE:
            {
              auto v = std::make_unique<LASRpcdreader>();
              pipeline.push_back(std::move(v));
              break;
            }
            #ifdef USING_R
            case DATAFRAME:
            {
              auto v = std::make_unique<LASRdataframereader>();
              pipeline.push_back(std::move(v));
              break;
            }
            case XPTR:
            {
              std::string address_ptr = stage.at("externalptr");
              SEXP sexplas = string_address_to_sexp(address_ptr);
              las = static_cast<PointCloud*>(R_ExternalPtrAddr(sexplas));
              if (las == nullptr)
              {
                last_error = "invalid external pointer";
                return false;
              }
              auto v = std::make_unique<LASRreaderxptr>(las);
              pipeline.push_back(std::move(v));
              point_cloud_ownership_transfered = true;
              break;
            }
            #endif
            default:
            {
              last_error = "Invalid catalog";
              return false;
            }
          }
          double temp_xmin = std::numeric_limits<double>::max();
          double temp_ymin = std::numeric_limits<double>::max();
          double temp_xmax = std::numeric_limits<double>::lowest();
          double temp_ymax = std::numeric_limits<double>::lowest();

          // Special treatment of the reader to find the potential queries in the catalog
          // If we have query we also recompute the bounding box in order to create outputs
          // with the minimal bounding box
          if (stage.contains("xcenter"))
          {
            std::vector<double> xcenter = get_vector<double>(stage["xcenter"]);
            std::vector<double> ycenter = get_vector<double>(stage["ycenter"]);
            std::vector<double> radius = get_vector<double>(stage["radius"]);

            for (size_t j = 0 ; j <  xcenter.size() ; ++j)
            {
              double x = xcenter[j];
              double y = ycenter[j];
              double r = radius[j];

              catalog->add_query(x, y, r);

              temp_xmin = MIN(temp_xmin, x-r);
              temp_ymin = MIN(temp_ymin, y-r);
              temp_xmax = MAX(temp_xmax, x+r);
              temp_ymax = MAX(temp_ymax, y+r);
            }

            xmin = MAX(xmin, temp_xmin);
            ymin = MAX(ymin, temp_ymin);
            xmax = MIN(xmax, temp_xmax);
            ymax = MIN(ymax, temp_ymax);
          }

          if (stage.contains("xmin"))
          {
            std::vector<double> bbxmin = get_vector<double>(stage["xmin"]);
            std::vector<double> bbymin = get_vector<double>(stage["ymin"]);
            std::vector<double> bbxmax = get_vector<double>(stage["xmax"]);
            std::vector<double> bbymax = get_vector<double>(stage["ymax"]);

            for (size_t j = 0 ; j <  bbxmin.size() ; ++j)
            {
              catalog->add_query(bbxmin[j], bbymin[j], bbxmax[j], bbymax[j]);

              temp_xmin = MIN(temp_xmin, bbxmin[j]);
              temp_ymin = MIN(temp_ymin, bbymin[j]);
              temp_xmax = MAX(temp_xmax, bbxmax[j]);
              temp_ymax = MAX(temp_ymax, bbymax[j]);
            }

            xmin = MAX(xmin, temp_xmin);
            ymin = MAX(ymin, temp_ymin);
            xmax = MIN(xmax, temp_xmax);
            ymax = MIN(ymax, temp_ymax);
          }
        }
      }
      else if (name  == "region_growing")
      {
        std::string uid1 = stage.at("connect1");
        std::string uid2 = stage.at("connect2");
        auto v = std::make_unique<LASRregiongrowing>();
        bool b1 = v->connect(pipeline, uid1);
        bool b2 = v->connect(pipeline, uid2);
        if (!b1 || !b2) return false;
        pipeline.push_back(std::move(v));
      }
      else if (name == "stop_if")
      {
        std::string condition = stage.at("condition");

        if (condition == "outside_bbox")
        {
          auto v = std::make_unique<LASRbreakoutsidebbox>();
          pipeline.push_back(std::move(v));
        }
        else if (condition == "chunk_id_below")
        {
          auto v = std::make_unique<LASRbreakbeforechunk>();
          pipeline.push_back(std::move(v));
        }
        else
        {
          last_error = "Invalid condition in break_if";
          return false;
        }
      }
      else if (name == "write_lax")
      {
        indexer = true;
        auto v = std::make_unique<LASRlaxwriter>();
        pipeline.push_back(std::move(v));
      }
      else
      {
        last_error = "Unsupported stage: " + std::string(name.c_str()); // # nocov
        return false; // # nocov
      }

      if (pipeline.size() > 0)
      {
        auto& it = pipeline.back();

        it->set_uid(uid);
        it->set_ncpu(ncpu);
        it->set_verbose(verbose);
        it->set_extent(xmin, ymin, xmax, ymax);

        if (stage.contains("connect"))
        {
          std::string uid = stage.at("connect");
          bool b = it->connect(pipeline, uid);
          if (!b) return false;
        }

        if (!it->set_parameters(stage))
        {
          last_error = "Invalid parameters in stage " + it->get_name() + ": " + last_error;
          return false;
        }

        it->get_extent(xmin, ymin, xmax, ymax);

        // If we intend to actually process the point cloud we check that a reader stage is present if needed
        if (catalog != nullptr && it->need_points() && !reader)
        {
          last_error = "The stage " + it->get_name() + " processes the point cloud but is not preceded by a reader stage";
          return false;
        }
      }
    }

    parsed = true;

    // If catalog == nullptr we did not build a catalog and thus
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
      //catalog->build_index(); // The catalog is built, we have the bbox of all the LAS files. We can build a spatial index
      if (!catalog->set_chunk_size(chunk_size)) return false;

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

        std::vector<std::string> filters;
        if (stage.contains("filter"))
        {
          if (stage["filter"].is_array())
          {
            // If 'filter' is an array of strings, iterate over the array
            for (const auto& item : stage["filter"])
            {
              if (item.is_string()) {
                filters.push_back(item.get<std::string>());
              }
            }
          }
          else
          {
            filters.push_back(stage["filter"].get<std::string>());
          }
        }
        else
        {
          filters.push_back("");
        }

        std::string output = stage.value("output", "");

        if (catalog->file_exists(output))
        {
          last_error = "Cannot override a file used as a source of point-cloud: " + output;
          return false;
        }

        // We set the CRS from the CRS of the catalog but then we get back the CRS. If we have
        // the 'set_crs' stage this updates the CRS assigned to the next stages
        const auto p = it->get();
        p->set_crs(current_crs);
        current_crs = p->get_crs();
        p->set_filter(filters);

        // Create empty files that will be filled later during the processing
        if (!p->set_output_file(output)) return false;

        i++;
        it++;
      }

      // Write lax is the very first stage. Even before read_las. It is called
      // only if needed.
      if (!catalog->check_spatial_index() && !indexer && catalog->get_format() == LASFILE)
      {
        bool onthefly = catalog->get_number_files() > 1;
        auto v = std::make_unique<LASRlaxwriter>(false, false, onthefly);
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