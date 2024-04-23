#include "Stage.h"

/* ==============
 *  VIRTUAL
 *  ============= */

Stage::Stage()
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

Stage::Stage(const Stage& other)
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

Stage::~Stage()
{
  #ifdef USING_R
  if (nsexpprotected > 0) UNPROTECT(nsexpprotected);
  #endif
}

bool Stage::set_chunk(const Chunk& chunk)
{
  set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  if (chunk.shape == ShapeType::CIRCLE) circular = true;
  return true;
}

void Stage::set_filter(const std::string& f)
{
  filter = f;
  std::string cpy = f;
  char* s = const_cast<char*>(cpy.c_str());
  lasfilter.parse(s);
}

#ifdef USING_R
SEXP Stage::to_R()
{
  if (ofile.empty()) return R_NilValue;
  SEXP R_string = PROTECT(Rf_mkChar(ofile.c_str()));
  SEXP R_string_vector = PROTECT(Rf_allocVector(STRSXP, 1));
  SET_STRING_ELT(R_string_vector, 0, R_string);
  nsexpprotected += 2;
  return R_string_vector;
}
#endif

void Stage::set_connection(Stage* stage)
{
  if (stage) connections[stage->get_uid()] = stage;
}

void Stage::update_connection(Stage* stage)
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

StageWriter::StageWriter()
{
  merged = false;
}

StageWriter::StageWriter(const StageWriter& other) : Stage(other)
{
  merged = other.merged;
  template_filename = other.template_filename;
  written = other.written;
  if (!merged) written.clear();
}

void StageWriter::sort(const std::vector<int>& order)
{
  if (merged) return;
  if (written.size() == 0) return;

  std::vector<std::string> ordered(written.size());
  for (size_t i = 0 ; i < order.size() ; ++i)
    ordered[order[i]] = written[i];

  written.swap(ordered);
  return;
}

void StageWriter::merge(const Stage* other)
{
  const StageWriter* o = dynamic_cast<const StageWriter*>(other);

  if (!merged)
    written.insert(written.end(), o->written.begin(), o->written.end());
  else
    written = o->written;
}

#ifdef USING_R
SEXP StageWriter::to_R()
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

StageRaster::StageRaster()
{
  GDALdataset::initialize_gdal();
}

StageRaster::StageRaster(const StageRaster& other) : StageWriter(other)
{
  raster = other.raster;
}

StageRaster::~StageRaster()
{

}

bool StageRaster::set_chunk(const Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // merged = true: there is only one master raster that cover the file collection. Each chunk
  //    is written to this master file
  // merged = false: each chunk is written in a new file. We must destroy the previous raster object
  //    and create a new one with the same properties (CRS, band names, resolution, etc...).
  //    Destroying a raster closes the underlying file.
  if (merged)
    raster.set_chunk(chunk); // Same gdal dataset but new location
  else
    raster = Raster(raster, chunk); // New gdal datset and new location

  return true;
}

bool StageRaster::set_input_file_name(const std::string& file)
{
  if (template_filename.empty()) return true;

  ofile = template_filename;

  ifile = file;
  size_t pos = ofile.find('*');
  if (pos != std::string::npos)
  {
    ofile.replace(pos, 1, ifile);
    raster.set_file(ofile);
    if (!raster.create_file()) return false;
    written.push_back(ofile);
  }

  return true;
}

// Called in the parser before any process. It assigns the name of the raster file in
// which the data will be written. The string may contain a wildcard in this case
// the string is a template and a new raster file is created for each chunk
bool StageRaster::set_output_file(const std::string& file)
{
  if (file.empty()) return true;

  template_filename = file;
  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
    raster.set_file(file);
    if (!raster.create_file()) return false;
    written.push_back(file);
  }

  return true;
}

bool StageRaster::set_crs(int epsg)
{
  return raster.set_crs(epsg);
}

bool StageRaster::set_crs(const std::string& wkt)
{
  return raster.set_crs(wkt);
}

bool StageRaster::write()
{
  if (ofile.empty()) return true;

  bool success;

  #pragma omp critical (write_raster)
  {
    success = raster.write();
  }

  return success;
}

/* ==============
 *  VECTOR
 * ============= */

StageVector::StageVector()
{
  GDALdataset::initialize_gdal();
}

StageVector::StageVector(const StageVector& other) : StageWriter(other)
{
  vector = other.vector;
}

StageVector::~StageVector()
{

}

bool StageVector::set_chunk(const Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // merged = true: there is only one master vector file that cover the file collection. Each chunk
  //    is written to this master file
  // merged = false: each chunk is written in a new file. We must destroy the previous vector object
  //    and create a new one with the same properties (CRS, number of attributes, etc...).
  //    Destroying a vector closes the underlying file.
  if (merged)
    vector.set_chunk(chunk);
  else
    vector = Vector(vector, chunk);

  return true;
}

bool StageVector::set_input_file_name(const std::string& file)
{
  if (template_filename.empty()) return true;

  ofile = template_filename;
  ifile = file;
  size_t pos = ofile.find('*');

  if (pos != std::string::npos)
  {
    ofile.replace(pos, 1, ifile);
    vector.set_file(ofile);
    if (!vector.create_file()) return false;
    written.push_back(ofile);
  }

  return true;
}

bool StageVector::set_output_file(const std::string& file)
{
  if (file.empty()) return true;

  template_filename = file;
  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
    vector.set_file(file);
    if (!vector.create_file()) return false;
    written.push_back(file);
  }

  return true;
}

bool StageVector::set_crs(int epsg)
{
  return vector.set_crs(epsg);
}

bool StageVector::set_crs(const std::string& wkt)
{
  return vector.set_crs(wkt);
}

/*void StageVector::clear(bool last)
{
  if (!merged || last)
  {
    vector.close();
  }
}*/

