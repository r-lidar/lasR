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

/* ==============
 *  WRITER
 *  ============= */

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
  merged = false;
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

void LASRalgorithmRaster::set_input_file_name(std::string file)
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
void LASRalgorithmRaster::set_output_file(std::string file)
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

bool LASRalgorithmRaster::set_crs(std::string wkt)
{
  return raster.set_crs(wkt);
}

bool LASRalgorithmRaster::write()
{
  #pragma omp critical (write_raster)
  {
    if (ofile.empty()) return true;
    if (raster.write()) return true;
    return false; // # nocov
  }
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
  merged = false;
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

void LASRalgorithmVector::set_input_file_name(std::string file)
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

void LASRalgorithmVector::set_output_file(std::string file)
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

bool LASRalgorithmVector::set_crs(std::string wkt)
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

