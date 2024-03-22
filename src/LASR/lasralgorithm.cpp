#include "lasralgorithm.h"

/* ==============
 *  VIRTUAL
 *  ============= */

LASRalgorithm::LASRalgorithm()
{
  xmin = 0;
  ymin = 0;
  xmax = 0;
  ymax = 0;
  verbose = false;
  circular = false;
  progress = nullptr;

  #ifdef USING_R
  nsexpprotected = 0;
  #endif
}

LASRalgorithm::LASRalgorithm(const LASRalgorithm& other)
{
  ncpu = other.ncpu;
  xmin = other.xmin;
  ymin = other.ymin;
  xmax = other.xmax;
  ymax = other.ymax;
  circular = other.circular;
  verbose = other.verbose;
  ifile = other.ifile;
  uid = other.uid;
  progress = other.progress;
  connections = other.connections;

  // Special treatment for LASfilter which is why we need a copy constructor
  // LASfilter is full of pointer, pointer on pointers without copy constructor.
  // We are better to build a new one for each copy.
  lasfilter = LASfilter();
  set_filter(other.filter);

  #ifdef USING_R
  nsexpprotected = 0;
  #endif
}

LASRalgorithm::~LASRalgorithm()
{
  #ifdef USING_R
  if (nsexpprotected > 0) UNPROTECT(nsexpprotected);
  #endif
}

bool LASRalgorithm::set_chunk(const Chunk& chunk)
{
  set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  if (chunk.shape == ShapeType::CIRCLE) circular = true;
  return true;
}

void LASRalgorithm::set_filter(const std::string& f)
{
  filter = f;
  std::string cpy = f;
  char* s = const_cast<char*>(cpy.c_str());
  lasfilter.parse(s);
}

#ifdef USING_R
SEXP LASRalgorithm::to_R()
{
  if (ofile.empty()) return R_NilValue;
  SEXP R_string = PROTECT(Rf_mkChar(ofile.c_str()));
  SEXP R_string_vector = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_STRING_ELT(R_string_vector, 0, R_string);
  nsexpprotected += 2;
  return R_string_vector;
}
#endif

void LASRalgorithm::set_connection(LASRalgorithm* stage)
{
  if (stage) connections[stage->get_uid()] = stage;
}

void LASRalgorithm::update_connection(LASRalgorithm* stage)
{
  if (!stage) return;
  if (connections.empty()) return;

  // Check if the key exists
  auto key = stage->get_uid();
  auto it = connections.find(key);
  if (it != connections.end()) it->second = stage;
}

/* ==============
 *  WRITER
 *  ============= */

LASRalgorithmWriter::LASRalgorithmWriter()
{
  merged = false;
}

LASRalgorithmWriter::LASRalgorithmWriter(const LASRalgorithmWriter& other) : LASRalgorithm(other)
{
  merged = other.merged;
  template_filename = other.template_filename;
  written = other.written;
  if (!merged) written.clear();
}

void LASRalgorithmWriter::sort(const std::vector<int>& order)
{
  if (merged) return;
  if (written.size() == 0) return;

  std::vector<std::string> ordered(written.size());
  for (size_t i = 0 ; i < order.size() ; ++i)
    ordered[order[i]] = written[i];

  written.swap(ordered);
  return;
}

void LASRalgorithmWriter::merge(const LASRalgorithm* other)
{
  const LASRalgorithmWriter* o = dynamic_cast<const LASRalgorithmWriter*>(other);

  if (!merged)
    written.insert(written.end(), o->written.begin(), o->written.end());
  else
    written = o->written;
}

#ifdef USING_R
SEXP LASRalgorithmWriter::to_R()
{
  if (written.size() == 0) return R_NilValue;

  SEXP R_string_vector = PROTECT(Rf_allocVector(STRSXP, written.size()));

  for (size_t i = 0 ; i < written.size() ; i++)
  {
    SEXP R_string = PROTECT(Rf_mkChar(written[i].c_str()));
    SET_STRING_ELT(R_string_vector, i, R_string);
  }

  UNPROTECT(written.size() + 1);

  return R_string_vector;
}
#endif


/* ==============
 *  RASTER
 *  ============= */

LASRalgorithmRaster::LASRalgorithmRaster()
{
  GDALdataset::initialize_gdal();
}

LASRalgorithmRaster::LASRalgorithmRaster(const LASRalgorithmRaster& other) : LASRalgorithmWriter(other)
{
  raster = other.raster;
}

LASRalgorithmRaster::~LASRalgorithmRaster()
{

}

bool LASRalgorithmRaster::set_chunk(const Chunk& chunk)
{
  LASRalgorithm::set_chunk(chunk);

  // merged = true: there is only one master raster that cover the file collection. Each chunk
  //    is written to this master file
  // merged = false: each chunk is written in a new file. We must destroy the previous raster object
  //    and create a new one with the same properties (CRS, band names, resolution, etc...).
  //    Destroying a raster closes the underlying file.
  if (merged)
    raster.set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  else
    raster = Raster(raster, chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);

  return true;
}

void LASRalgorithmRaster::set_input_file_name(const std::string& file)
{
  if (template_filename.empty()) return;

  ofile = template_filename;

  ifile = file;
  size_t pos = ofile.find('*');

  if (pos != std::string::npos)
  {
    ofile.replace(pos, 1, ifile);
    raster.set_file(ofile);
    if (!raster.create_file())
    {
      throw last_error;
    }
    written.push_back(ofile);
  }
}

// Called in the parser before any process. It assigns the name of the raster file in
// which the data will be written. The string may contain a wildcard in this case
// the string is a template and a new raster file is created for each chunk
void LASRalgorithmRaster::set_output_file(const std::string& file)
{
  if (file.empty()) return;

  template_filename = file;
  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
    raster.set_file(file);
    if (!raster.create_file())
    {
      throw last_error;
    }
    written.push_back(file);
  }
}

bool LASRalgorithmRaster::set_crs(int epsg)
{
  return raster.set_crs(epsg);
}

bool LASRalgorithmRaster::set_crs(const std::string& wkt)
{
  return raster.set_crs(wkt);
}

bool LASRalgorithmRaster::write()
{
  if (ofile.empty()) return true;

  bool success;

  #pragma omp critical (write_raster)
  {
    success = raster.write();
  }

  return success;
}

/*void LASRalgorithmRaster::clear(bool last)
{
  if (!merged || last)
  {
    raster.close();
  }
}*/

/* ==============
 *  VECTOR
 * ============= */

LASRalgorithmVector::LASRalgorithmVector()
{
  GDALdataset::initialize_gdal();
}

LASRalgorithmVector::LASRalgorithmVector(const LASRalgorithmVector& other) : LASRalgorithmWriter(other)
{
  vector = other.vector;
}

LASRalgorithmVector::~LASRalgorithmVector()
{

}

bool LASRalgorithmVector::set_chunk(const Chunk& chunk)
{
  LASRalgorithm::set_chunk(chunk);

  // merged = true: there is only one master vector file that cover the file collection. Each chunk
  //    is written to this master file
  // merged = false: each chunk is written in a new file. We must destroy the previous vector object
  //    and create a new one with the same properties (CRS, number of attributes, etc...).
  //    Destroying a vector closes the underlying file.
  if (merged)
    vector.set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  else
    vector = Vector(vector, chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);

  return true;
}

void LASRalgorithmVector::set_input_file_name(const std::string& file)
{
  if (template_filename.empty()) return;

  ofile = template_filename;
  ifile = file;
  size_t pos = ofile.find('*');

  if (pos != std::string::npos)
  {
    ofile.replace(pos, 1, ifile);
    vector.set_file(ofile);
    if (!vector.create_file())
    {
      throw last_error;
    }
    written.push_back(ofile);
  }
}

void LASRalgorithmVector::set_output_file(const std::string& file)
{
  if (file.empty()) return;

  template_filename = file;
  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
    vector.set_file(file);
    if (!vector.create_file())
    {
      throw last_error;
    }
    written.push_back(file);
  }
}

bool LASRalgorithmVector::set_crs(int epsg)
{
  return vector.set_crs(epsg);
}

bool LASRalgorithmVector::set_crs(const std::string& wkt)
{
  return vector.set_crs(wkt);
}

/*void LASRalgorithmVector::clear(bool last)
{
  if (!merged || last)
  {
    vector.close();
  }
}*/

