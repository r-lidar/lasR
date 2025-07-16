#ifdef USING_R

#include "readdataframe.h"

#include <algorithm>

LASRdataframereader::LASRdataframereader(const LASRdataframereader& other) : Stage(other)
{
  xmin = other.xmin;
  ymin = other.ymin;
  xmax = other.xmax;
  ymax = other.ymax;
  dataframe = other.dataframe;
  wkt = other.wkt;
  scale[0] = other.scale[0];
  scale[1] = other.scale[1];
  scale[2] = other.scale[2];
  offset[0] = other.offset[0];
  offset[1] = other.offset[1];
  offset[2] = other.offset[2];
  current_point = other.current_point;
  npoints = other.npoints;
  header = nullptr;
  streaming = true;
}

bool LASRdataframereader::set_parameters(const nlohmann::json& stage)
{
  std::string address_dataframe_str = stage.at("dataframe");
  dataframe = (SEXP)string_address_to_sexp(address_dataframe_str);

  std::vector<double> accuracy = stage.at("accuracy");
  scale[0] = accuracy[0];
  scale[1] = accuracy[1];
  scale[2] = accuracy[2];
  offset[0] = xmin;
  offset[1] = ymin;
  offset[2] = 0;
  current_point = 0;
  npoints = Rf_length(VECTOR_ELT(dataframe, 0));

  wkt = stage.at("crs");

  return true;
}

bool LASRdataframereader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);
  pointfilter.add_clip(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON, circular);
  return true;
}

bool LASRdataframereader::process(Header*& header)
{
  if (header != nullptr) return true;

  SEXP names_attr = Rf_getAttrib(dataframe, R_NamesSymbol);

  header = new Header;
  header->signature = "data.frame";
  header->min_x = xmin;
  header->min_y = ymin;
  header->max_x = xmax;
  header->max_y = ymax;
  header->number_of_point_records = npoints;
  header->crs = CRS(wkt);

  if (scale[0] == 0)
  {
    for (int j = 0 ; j < Rf_length(dataframe) ; j++)
    {
      SEXP vector = VECTOR_ELT(dataframe, j);

      std::string sname(CHAR(STRING_ELT(names_attr, j)));

      if (sname == "X") scale[0] = guess_accuracy(vector);
      if (sname == "Y") scale[1] = guess_accuracy(vector);
      if (sname == "Z") scale[2] = guess_accuracy(vector);
    }
  }

  Attribute attrf("flags", AttributeType::INT8);
  header->add_attribute(attrf);

  Attribute attrx("X", AttributeType::INT32, scale[0], offset[0]);
  header->add_attribute(attrx);
  accessors.push_back(AttributeAccessor("X"));

  Attribute attry("Y", AttributeType::INT32, scale[1], offset[1]);
  header->add_attribute(attry);
  accessors.push_back(AttributeAccessor("Y"));

  Attribute attrz("Z", AttributeType::INT32, scale[2], offset[2]);
  header->add_attribute(attrz);
  accessors.push_back(AttributeAccessor("Z"));

  // Check the name and find to which LAS attributes it corresponds
  for (int i = 0; i <  Rf_length(dataframe); i++)
  {
    std::string sname(CHAR(STRING_ELT(names_attr, i)));
    SEXP vector = VECTOR_ELT(dataframe, i);
    int type = TYPEOF(vector);


    if (sname == "X")
    {
      continue;
    }
    else if (sname == "Y")
    {
      continue;
    }
    else if (sname == "Z")
    {
      continue;
    }
    else if (sname == "Intensity")
    {
      Attribute attr("Intensity", AttributeType::UINT16);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "gpstime")
    {
      Attribute attr("gpstime", AttributeType::DOUBLE);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "ReturnNumber")
    {
      Attribute attr("ReturnNumber", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "NumberOfReturns")
    {
      Attribute attr("NumberOfReturns", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "ScanDirectionFlag")
    {
      Attribute attr("ScanDirectionFlag", AttributeType::BIT);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "EdgeOfFlightline")
    {
      Attribute attr("EdgeOfFlightline", AttributeType::BIT);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Classification")
    {
      Attribute attr("Classification", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Synthetic")
    {
      Attribute attr("Synthetic", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Keypoint")
    {
      Attribute attr("Keypoint", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Withheld")
    {
      Attribute attr("Withheld", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Overlap")
    {
      Attribute attr("Overlap", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "ScanAngle")
    {
      Attribute attr("ScanAngle", AttributeType::FLOAT);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "UserData")
    {
      Attribute attr("UserData", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "PointSourceID")
    {
      Attribute attr("PointSourceID", AttributeType::UINT16);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "R")
    {
      Attribute attr("R", AttributeType::UINT16);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "G")
    {
      Attribute attr("G", AttributeType::UINT16);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "B")
    {
      Attribute attr("B", AttributeType::UINT16);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "NIR")
    {
      Attribute attr("NIR", AttributeType::UINT16);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Channel")
    {
      Attribute attr("Channel", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Buffer")
    {
      Attribute attr("Buffer", AttributeType::UINT8);
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "ScanAngleRank") {
      Attribute attr("ScanAngleRank", AttributeType::INT8); // # nocov
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Synthetic_flag")
    {
      Attribute attr("Synthetic", AttributeType::BIT); // # nocov
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Keypoint_flag") {
      Attribute attr("Keypoint", AttributeType::BIT); // # nocov
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Withheld_flag")
    {
      Attribute attr("Withheld", AttributeType::BIT); // # nocov
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else if (sname == "Overlap_flag")
    {
      Attribute attr("Overlap", AttributeType::BIT); // # nocov
      header->add_attribute(attr);
      accessors.push_back(AttributeAccessor(sname));
    }
    else
    {
      if (type == REALSXP)
      {
        Attribute attr(sname, AttributeType::DOUBLE);
        header->add_attribute(attr);
        accessors.push_back(AttributeAccessor(sname));
      }
      else if (type == INTSXP)
      {
        Attribute attr(sname, AttributeType::INT32);
        header->add_attribute(attr);
        accessors.push_back(AttributeAccessor(sname));
      }
      else
      {
        Attribute attr(sname, AttributeType::INT8);
        header->add_attribute(attr);
        accessors.push_back(AttributeAccessor(sname));
      }
    }
  }

  this->header = header;

  return true;
}

bool LASRdataframereader::process(Point*& point)
{
  if (point == nullptr)
    point = new Point(&header->schema);
  else
    point->zero();

  if (current_point >= npoints)
  {
    delete point;
    point = nullptr;
    return true;
  }

  while (current_point < npoints)
  {
    for (int j = 0 ; j < Rf_length(dataframe) ; j++)
    {
      //print("current point %d col %d column %d\n", current_point, j, col_names[j]);
      SEXP vector = VECTOR_ELT(dataframe, j);
      int type = TYPEOF(vector);

      if (type == REALSXP)
        accessors[j](point, REAL(vector)[current_point]);
      else if (type == INTSXP)
        accessors[j](point, INTEGER(vector)[current_point]);
      else
        accessors[j](point, LOGICAL(vector)[current_point]);
    }

    current_point++;

    if (!pointfilter.filter(point))
    {
      return true;
    }
  }

  // The last point
  if (current_point == npoints && pointfilter.filter(point))
  {
    delete point;
    point = nullptr;
  }

  return true;
}

bool LASRdataframereader::process(PointCloud*& las)
{
  streaming = false;

  if (las != nullptr)
  {
    delete las;
    las = nullptr;
  }

  if (las == nullptr)
    las = new PointCloud(header);

  Point* p = nullptr;
  while (process(p))
  {
    if (p == nullptr) break;
    las->add_point(*p);
  }

  if (verbose) print("Building a spatial index\n");
  las->update_header();
  las->build_spatialindex();

  return true;
}

void LASRdataframereader::clear(bool)
{
  // Called at the end of the pipeline. We can delete the header
  if (streaming && header)
  {
    delete header;
    header = nullptr;
  }
}

double LASRdataframereader::guess_accuracy(SEXP x) const
{
  std::vector<int> table = {0,0,0,0,0,0,0,0,0,0};

  for (int i = 0 ; i < Rf_length(x) ; ++i)
  {
    // modified from https://stackoverflow.com/a/63215955/8442410
    unsigned int count = 0;
    double v = std::abs(REAL(x)[i]);
    double c = v - std::floor(v);
    double factor = 10;
    double eps = std::numeric_limits<double>::epsilon() * c;

    while ((c > eps && c < (1 - eps)) && count < 8)
    {
      c = v * factor;
      c = c - std::floor(c);
      factor *= 10;
      eps = std::numeric_limits<double>::epsilon() * v * factor;
      count++;
    }

    if(count < 10) table[count]++;
  }

  int n = 0;
  int max = table[0];
  for (int i = 1 ; i < 10 ; ++i)
  {
    if (table[i] > max)
    {
      max = table[i];
      n = i;
    }
  }

  return 1/std::pow(10,n);
}

#endif