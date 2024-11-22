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
  ncpu = 1;
  ncpu_concurrent_files = 1;

  #ifdef USING_R
  nsexpprotected = 0;
  #endif
}

Stage::Stage(const Stage& other)
{
  ncpu = other.ncpu;
  ncpu_concurrent_files = other.ncpu_concurrent_files;
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
  crs = other.crs;
  filter = other.filter;

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

bool Stage::set_chunk(Chunk& chunk)
{
  set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  if (chunk.shape == ShapeType::CIRCLE) circular = true;
  return true;
}

void Stage::set_filter(const std::vector<std::string>& f)
{
  for (auto c : f) pointfilter.add_condition(c);
}

/*void Stage::set_filter(const std::string& f)
{
  filter = f;
  std::string cpy = f;
  char* s = const_cast<char*>(cpy.c_str());
  if (!lasfilter.parse(s)) throw std::string("Invalid filter detected");
}*/

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

nlohmann::json Stage::to_json() const
{
  nlohmann::json ans;
  if (ofile.empty()) return ans;
  return {{get_name(), ofile}};
}

void Stage::set_connection(Stage* stage)
{
  if (stage) connections[stage->get_uid()] = stage;
}

Stage* Stage::search_connection(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  auto it = std::find_if(pipeline.begin(), pipeline.end(), [&uid](const std::unique_ptr<Stage>& obj) { return obj->get_uid() == uid; });
  if (it == pipeline.end())
  {
    last_error = "Cannot find stage with uid " + uid;
    return nullptr;
  }
  return it->get();
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

bool Stage::is_valid_pointer(void* p) const
{
  if (p == nullptr)
  {
    last_error = "invalid memory allocation. A 'reader' stage is missing or is at an incorrect position in the pipeline";
    return false;
  }

  return true;
}

double Stage::convert_units(double x) const
{
  double u = crs.get_linear_units();
  return x*u;
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

nlohmann::json StageWriter::to_json() const
{
  nlohmann::json ans;
  if (written.empty()) return ans;
  for (size_t i = 0; i < written.size(); i++) ans.push_back(written[i]);
  return {{get_name(), ans}};
}


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

bool StageRaster::set_chunk(Chunk& chunk)
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

void StageRaster::set_crs(const CRS& crs)
{
  Stage::set_crs(crs);
  raster.set_crs(crs);
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

bool StageVector::set_chunk(Chunk& chunk)
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

void StageVector::set_crs(const CRS& crs)
{
  Stage::set_crs(crs);
  vector.set_crs(crs);
}

/*void StageVector::clear(bool last)
{
  if (!merged || last)
  {
    vector.close();
  }
}*/

/* ==============
 *  MATRIX
 * ============= */

StageMatrix::StageMatrix()
{
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      matrix[i][j] = 0;
    }
  }
}

StageMatrix::StageMatrix(const StageMatrix& other)
{
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      matrix[i][j] = other.matrix[i][j];
    }
  }
}

void StageMatrix::transform(double& x, double& y, double& z)
{
  double xt,yt,zt;
  double point[4];
  double tpoint[4];

  point[0] = x;
  point[1] = y;
  point[2] = z;
  point[3] = 1;

  tpoint[0] = 0;
  tpoint[1] = 0;
  tpoint[2] = 0;
  tpoint[3] = 0;

  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      tpoint[i] += matrix[i][j] * point[j];
    }
  }

  x = tpoint[0];
  y = tpoint[1];
  z = tpoint[2];
}

void StageMatrix::merge(const Stage* other)
{
  const StageMatrix* o = dynamic_cast<const StageMatrix*>(other);

  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      matrix[i][j] = o->matrix[i][j];
    }
  }
}

#ifdef USING_R
SEXP StageMatrix::to_R()
{
  SEXP H = PROTECT(Rf_allocVector(REALSXP, 16)); nsexpprotected++;

  double* Hptr = REAL(H);
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      Hptr[i + 4 * j] = matrix[i][j];
    }
  }

  SEXP dim = PROTECT(Rf_allocVector(INTSXP, 2)); nsexpprotected++;
  INTEGER(dim)[0] = 4;
  INTEGER(dim)[1] = 4;
  Rf_setAttrib(H, R_DimSymbol, dim);

  return H;
}
#endif

nlohmann::json StageMatrix::to_json() const
{
  nlohmann::json j_matrix = nlohmann::json::array();

  for (int i = 0; i < 4; ++i)
  {
    nlohmann::json row = nlohmann::json::array();
    for (int j = 0; j < 4; ++j)
    {
      row.push_back(matrix[i][j]);
    }
    j_matrix.push_back(row);
  }
  return j_matrix;
}

#ifdef USING_R
SEXP string_address_to_sexp(const std::string& addr)
{
  uintptr_t ptr = strtoull(addr.c_str(), NULL, 16);
  SEXP s = (SEXP)ptr;
  return s;
}
#endif

