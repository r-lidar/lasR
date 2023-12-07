#include "readdataframe.h"

#include <algorithm>

LASRdataframereader::LASRdataframereader(double xmin, double ymin, double xmax, double ymax, SEXP dataframe, const std::vector<double>& accuracy)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->dataframe = dataframe;
  scale[0] = accuracy[0];
  scale[1] = accuracy[1];
  scale[2] = accuracy[2];
  offset[0] = xmin;
  offset[1] = ymin;
  offset[2] = 0;
  has_gps = false;
  has_nir = false;
  has_rgb = false;
  is_extended = false;
  current_point = 0;
  num_extrabytes = 0;
  npoints = Rf_length(VECTOR_ELT(dataframe, 0));
}

bool LASRdataframereader::set_chunk(const Chunk& chunk)
{
  const char* tmp = filter.c_str();
  int n = strlen(tmp)+1;
  char* filtercpy = (char*)malloc(n); memcpy(filtercpy, tmp, n);

  lasfilter.clean();
  lasfilter.parse(filtercpy);

  if (chunk.shape == ShapeType::RECTANGLE)
    lasfilter.addClipBox(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, F64_MIN, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON, F64_MAX);
  else if (chunk.shape == ShapeType::CIRCLE)
    lasfilter.addClipCircle((chunk.xmin+chunk.xmax)/2, (chunk.ymin+chunk.ymax)/2,  (chunk.xmax-chunk.xmin)/2 + chunk.buffer + EPSILON);
  else
    lasfilter.addClipBox(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, F64_MIN, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON, F64_MAX);

  return true;
}

bool LASRdataframereader::process(LASheader*& header)
{
  col_names.resize(Rf_length(dataframe));
  SEXP names_attr = Rf_getAttrib(dataframe, R_NamesSymbol);

  // Check the name and find to which LAS attributes it corresponds
  for (int i = 0; i <  Rf_length(dataframe); i++)
  {
    std::string sname(CHAR(STRING_ELT(names_attr, i)));
    if (sname == "X") col_names[i] = attributes::X;
    else if (sname == "Y") col_names[i] = attributes::Y;
    else if (sname == "Z") col_names[i] = attributes::Z;
    else if (sname == "Intensity") col_names[i] = attributes::I;
    else if (sname == "gpstime") { col_names[i] = attributes::T; has_gps = true; }
    else if (sname == "ReturnNumber") col_names[i] = attributes::RN;
    else if (sname == "NumberOfReturns") col_names[i] = attributes::NOR;
    else if (sname == "ScanDirectionFlag") col_names[i] = attributes::SDF;
    else if (sname == "EdgeOfFlightline") col_names[i] = attributes::EoF;
    else if (sname == "Classification") col_names[i] = attributes::CLASS;
    else if (sname == "Synthetic") col_names[i] = attributes::SYNT;
    else if (sname == "Keypoint") col_names[i] = attributes::KEYP;
    else if (sname == "Withheld") col_names[i] = attributes::WITH;
    else if (sname == "Overlap") { col_names[i] = attributes::OVER; is_extended = true; }
    else if (sname == "ScanAngle") col_names[i] = attributes::SA;
    else if (sname == "UserData") col_names[i] = attributes::UD;
    else if (sname == "PointSourceID") col_names[i] = attributes::PSID;
    else if (sname == "Red") { col_names[i] = attributes::R; has_rgb = true; }
    else if (sname == "Blue") { col_names[i] = attributes::G; has_rgb = true; }
    else if (sname == "Green") { col_names[i] = attributes::B; has_rgb = true; }
    else if (sname == "NIR") { col_names[i] = attributes::NIR; has_nir = true; }
    else if (sname == "Channel") { col_names[i] = attributes::CHAN; is_extended = true; }
    else if (sname == "Buffer") col_names[i] = attributes::BUFF;
    // Compatibility with lidR and rlas
    else if (sname == "ScanAngleRank") col_names[i] = attributes::SAR; // # nocov
    else if (sname == "Synthetic_flag") col_names[i] = attributes::SYNT; // # nocov
    else if (sname == "Keypoint_flag") col_names[i] = attributes::KEYP; // # nocov
    else if (sname == "Withheld_flag") col_names[i] = attributes::WITH;// # nocov
    else if (sname == "Overlap_flag") { col_names[i] = attributes::OVER; is_extended = true; } // # nocov
    else
    {
      col_names[i] = num_extrabytes+100;
      num_extrabytes++;
    }
  }

  if (scale[0] == 0)
  {
    for (int j = 0 ; j < Rf_length(dataframe) ; j++)
    {
      //print("current point %d col %d column %d\n", current_point, j, col_names[j]);
      SEXP vector = VECTOR_ELT(dataframe, j);
      int type = TYPEOF(vector);

      switch(col_names[j])
      {
        case attributes::X: scale[0] = guess_accuracy(vector); break;
        case attributes::Y: scale[1] = guess_accuracy(vector); break;
        case attributes::Z: scale[2] = guess_accuracy(vector); break;
        default: break;
      }
    }
  }

  int version_minor = 2;
  if (is_extended) version_minor = 4;

  lasheader.file_source_ID       = 0;
  lasheader.version_major        = 1;
  lasheader.version_minor        = version_minor;
  lasheader.header_size          = get_header_size(version_minor);
  lasheader.offset_to_point_data = get_header_size(version_minor);
  lasheader.file_creation_year   = 0;
  lasheader.file_creation_day    = 0;
  lasheader.point_data_format    = guess_point_data_format();
  lasheader.x_scale_factor       = scale[0];
  lasheader.y_scale_factor       = scale[1];
  lasheader.z_scale_factor       = scale[2];
  lasheader.x_offset             = offset[0];
  lasheader.y_offset             = offset[1];
  lasheader.z_offset             = offset[2];
  lasheader.number_of_point_records = npoints;
  lasheader.min_x                = xmin;
  lasheader.min_y                = ymin;
  lasheader.max_x                = xmax;
  lasheader.max_y                = ymax;
  lasheader.point_data_record_length = get_point_data_record_length(lasheader.point_data_format);

  // Add extrabytes
  for (int i = 0; i <  Rf_length(dataframe); i++)
  {
    if (col_names[i] >= 100)
    {
      int data_type = LAS::LONG;
      SEXP vector = VECTOR_ELT(dataframe, i);
      if (TYPEOF(vector) == REALSXP) data_type = LAS::DOUBLE;
      const char* name = CHAR(STRING_ELT(names_attr, i));
      LASattribute lasattribute(data_type-1, name, name);
      lasheader.add_attribute(lasattribute);
      lasheader.update_extra_bytes_vlr();
      lasheader.point_data_record_length += lasattribute.get_size();
    }
  }

  //lasheader.set_global_encoding_bit(0); // GPS Time Type
  //lasheader.set_global_encoding_bit(1); // Waveform Data Packets Internal
  //lasheader.set_global_encoding_bit(2); // Waveform Data Packets External
  //lasheader.set_global_encoding_bit(3); // Synthetic Return Numbers
  //lasheader.set_global_encoding_bit(4); // WKT crs
  //lasheader.set_global_encoding_bit(5); // Aggregate Model

  strcpy(lasheader.generating_software, "lasR R package");

  laspoint.init(&lasheader, lasheader.point_data_format, lasheader.point_data_record_length, &lasheader);

  header = &lasheader;
  return true;
}

bool LASRdataframereader::process(LASpoint*& point)
{
  if (point == nullptr)
  {
    point = &laspoint;
  }

  if (current_point >= npoints)
  {
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

      switch(col_names[j])
      {
        case attributes::X: laspoint.set_x(REAL(vector)[current_point]); break;
        case attributes::Y: laspoint.set_y(REAL(vector)[current_point]); break;
        case attributes::Z: laspoint.set_z(REAL(vector)[current_point]); break;
        case attributes::I: laspoint.set_intensity(INTEGER(vector)[current_point]); break;
        case attributes::T: laspoint.set_gps_time(REAL(vector)[current_point]); break;
        case attributes::RN: laspoint.set_return_number(INTEGER(vector)[current_point]); break;
        case attributes::NOR: laspoint.set_number_of_returns(INTEGER(vector)[current_point]); break;
        case attributes::EoF: laspoint.set_edge_of_flight_line(INTEGER(vector)[current_point]); break;
        case attributes::SDF:
        {
          // For compatibility with lidR and rlas
          if (TYPEOF(vector) == INTSXP)
            laspoint.set_scan_direction_flag(INTEGER(vector)[current_point]);
          else
            laspoint.set_scan_direction_flag(LOGICAL(vector)[current_point]);

          break;
        }
        case attributes::CLASS: laspoint.set_classification(INTEGER(vector)[current_point]); break;
        case attributes::SYNT: laspoint.set_synthetic_flag(LOGICAL(vector)[current_point]); break;
        case attributes::KEYP: laspoint.set_keypoint_flag(LOGICAL(vector)[current_point]); break;
        case attributes::WITH: laspoint.set_withheld_flag(LOGICAL(vector)[current_point]); break;
        case attributes::OVER: laspoint.set_extended_overlap_flag(LOGICAL(vector)[current_point]); break;
        case attributes::SA: laspoint.set_scan_angle(REAL(vector)[current_point]); break;
        case attributes::SAR: laspoint.set_scan_angle(INTEGER(vector)[current_point]); break;
        case attributes::UD: laspoint.set_user_data(INTEGER(vector)[current_point]); break;
        case attributes::PSID: laspoint.set_point_source_ID(INTEGER(vector)[current_point]); break;
        case attributes::R: laspoint.set_R(INTEGER(vector)[current_point]); break;
        case attributes::G: laspoint.set_G(INTEGER(vector)[current_point]); break;
        case attributes::B: laspoint.set_B(INTEGER(vector)[current_point]); break;
        case attributes::NIR: laspoint.set_NIR(INTEGER(vector)[current_point]); break;
        case attributes::CHAN: laspoint.set_extended_scanner_channel(INTEGER(vector)[current_point]); break;
        case attributes::BUFF: break;
        case -1: break;
        default: //extrabytes
        {
          int attr_index = col_names[j]-100;
          LASattribute attr = laspoint.attributer->attributes[attr_index];

          if (type ==  INTSXP)
          {
            int val = INTEGER(vector)[current_point];

            switch(attr.data_type)
            {
            case LAS::UCHAR:  { U8 u  = U8_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::CHAR:   { I8 u  = I8_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::USHORT: { U16 u = U16_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::SHORT:  { I16 u = I16_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::LONG:   { I32 u = I32_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            }
          }
          else if (type ==  REALSXP)
          {
            double val = REAL(vector)[current_point];
            val = (val - attr.offset[0])/attr.scale[0];

            switch(attr.data_type)
            {
            case LAS::UCHAR:  { U8  u = U8_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::CHAR:   { I8  u = I8_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::USHORT: { U16 u = U16_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::SHORT:  { I16 u = I16_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::LONG:   { I32 u = I32_CLAMP(val); laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::FLOAT:  { F32 u = (F32)val; laspoint.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::DOUBLE: { F64 u = (F64)val; laspoint.set_attribute(attr_index, (U8*)&u); break; }
            }
          }

          break;
        }
      }
    }

    current_point++;

    if (!lasfilter.filter(&laspoint))
      return true;
  }

  point = nullptr;
  return true;
}

bool LASRdataframereader::process(LAS*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new LAS(&lasheader);

  LASpoint* p = nullptr;
  while (process(p))
  {
    if (p == nullptr) break;
    las->add_point(*p);
  }
  return true;
}

int LASRdataframereader::get_point_data_record_length(int point_data_format)
{
  switch (point_data_format)
  {
    case 0: return 20 + num_extrabytes; break;
    case 1: return 28 + num_extrabytes; break;
    case 2: return 26 + num_extrabytes; break;
    case 3: return 34 + num_extrabytes; break;
    case 4: return 57 + num_extrabytes; break;
    case 5: return 63 + num_extrabytes; break;
    case 6: return 30 + num_extrabytes; break;
    case 7: return 36 + num_extrabytes; break;
    case 8: return 38 + num_extrabytes; break;
    case 9: return 59 + num_extrabytes; break;
    case 10: return 67 + num_extrabytes; break;
    default: return 0; break;
  }
}

int LASRdataframereader::guess_point_data_format()
{
  std::vector<int> formats = {0,1,2,3,6,7,8};
  int format;

  if (has_nir) // format 8 or 10
    return 8;

  if (has_gps) // format 1,3:10
  {
    auto end = std::remove(formats.begin(), formats.end(), 0);
    formats.erase(end, formats.end());
    end = std::remove(formats.begin(), formats.end(), 2);
    formats.erase(end, formats.end());
  }

  if (has_rgb)  // format 3, 5, 7, 8
  {
    auto end = std::remove(formats.begin(), formats.end(), 0);
    formats.erase(end, formats.end());
    end = std::remove(formats.begin(), formats.end(), 1);
    formats.erase(end, formats.end());
    end = std::remove(formats.begin(), formats.end(), 6);
    formats.erase(end, formats.end());
  }

  return formats[0];
}

int LASRdataframereader::get_header_size(int minor_version)
{
  int header_size = 0;

  switch (minor_version)
  {
    case 0:
    case 1:
    case 2:
      header_size = 227;
      break;
    case 3:
      header_size = 235;
      break;
    case 4:
      header_size = 375;
      break;
    default:
      header_size = -1;
    break;
  }

  return header_size;
}

double LASRdataframereader::guess_accuracy(SEXP x)
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