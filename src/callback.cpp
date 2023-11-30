#include "callback.h"
#include <unordered_set>

#ifdef USING_R

LASRcallback::LASRcallback(double xmin, double ymin, double xmax, double ymax, std::string select, SEXP fun, SEXP args, bool modify, bool drop_buffer)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->fun = fun;
  this->args = args;
  this->ans = R_NilValue;

  // Parse select = "*"
  std::string all = "xyzitrndecskwoaupRGBNCb0123456789";
  size_t pos = select.find('*');
  if (pos != std::string::npos) select = all;

  // Check if not duplicated flag
  std::string parsed;
  std::unordered_set<char> seen;
  for (char c : select)
  {
    if (seen.find(c) == seen.end())
    {
      parsed += c;
      seen.insert(c);
    }
  }

  this->select = parsed;
  this->modify = modify;
  this->drop_buffer = drop_buffer;

  ans = PROTECT(Rf_allocVector(VECSXP, 0)); nsexpprotected++;
}

void LASRcallback::set_input_file_name(std::string file)
{
  filenames.push_back(file);
}

bool LASRcallback::process(LAS*& las)
{
  // LAS format
  bool extended = (las->header->version_minor >= 4) && (las->header->point_data_format >= 6);
  bool has_gps = las->point.have_gps_time;
  bool has_rgb = las->point.have_rgb;
  bool has_nir = las->point.have_nir;

  // List all existing attributes
  std::vector<int> names;
  for(const char& c : select)
  {
    switch(c)
    {
      case 'x': names.push_back(X); break;
      case 'y': names.push_back(Y); break;
      case 'z': names.push_back(Z); break;
      case 'i': names.push_back(I); break;
      case 't': if (has_gps) names.push_back(T); break;
      case 'r': names.push_back(RN); break;
      case 'n': names.push_back(NOR); break;
      case 'd': names.push_back(SDF); break;
      case 'e': names.push_back(EoF); break;
      case 'c': names.push_back(CLASS); break;
      case 's': names.push_back(SYNT); break;
      case 'k': names.push_back(KEYP); break;
      case 'w': names.push_back(WITH); break;
      case 'o': if (extended) { names.push_back(OVER); } break;
      case 'a': names.push_back(SA); break;
      case 'u': names.push_back(UD); break;
      case 'p': names.push_back(PSID); break;
      case 'R': if (has_rgb) { names.push_back(R); } break;
      case 'G': if (has_rgb) { names.push_back(G); } break;
      case 'B': if (has_rgb) { names.push_back(B);} break;
      case 'N': if (has_nir) { names.push_back(NIR); } break;
      case 'C': if (extended) { names.push_back(CHAN); } break;
      case 'b': names.push_back(BUFF) ; break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      {
        int index = c - '0';
        if (las->is_attribute_loadable(index)) names.push_back(100 + index);
        break;
      }
      default: warning("Flag '%c' does not correspond to an existing attribute", c);
    }
  }

  int nattr = names.size(); // Number of attribute to expose

  // Create environments in which the call takes place
  SEXP data_frame = PROTECT(Rf_allocVector(VECSXP, nattr)); nsexpprotected++;

  // Populate the list by allocating vectors to store lidar data.
  for (int i = 0 ; i < nattr ; i++)
  {
    int type;
    int name = names[i];

    if (name >= 100) // Extra bytes
    {
      LASattribute attr = las->header->attributes[name-100];
      if (attr.data_type < LAS::FLOAT)
      {
        if (attr.has_scale() || attr.has_offset())
          type = REALSXP;
        else
          type = INTSXP;
      }
      else
        type = REALSXP;
    }
    else if (name == X || name == Y || name == Z || name == SA || name == T)
      type = REALSXP;
    else if (name == SDF ||  name ==  SYNT || name == KEYP || name == WITH || name== OVER || name == BUFF)
      type = LGLSXP;
    else
      type = INTSXP;

    SEXP v = PROTECT(Rf_allocVector(type, las->npoints)); nsexpprotected++;
    SET_VECTOR_ELT(data_frame, i, v);
  }

  // Assign names to the data_frame
  SEXP list_names = PROTECT(Rf_allocVector(STRSXP, nattr)); nsexpprotected++;
  for (int i = 0; i < nattr; i++)
  {
    switch (names[i])
    {
    case X:     SET_STRING_ELT(list_names, i, Rf_mkChar("X")); break;
    case Y:     SET_STRING_ELT(list_names, i, Rf_mkChar("Y")); break;
    case Z:     SET_STRING_ELT(list_names, i, Rf_mkChar("Z")); break;
    case I:     SET_STRING_ELT(list_names, i, Rf_mkChar("Intensity")); break;
    case T:     SET_STRING_ELT(list_names, i, Rf_mkChar("gpstime")); break;
    case RN:    SET_STRING_ELT(list_names, i, Rf_mkChar("ReturnNumber")); break;
    case NOR:   SET_STRING_ELT(list_names, i, Rf_mkChar("NumberOfReturns")); break;
    case SDF:   SET_STRING_ELT(list_names, i, Rf_mkChar("ScanDirectionFlag")); break;
    case EoF:   SET_STRING_ELT(list_names, i, Rf_mkChar("NumberOfReturns")); break;
    case CLASS: SET_STRING_ELT(list_names, i, Rf_mkChar("Classification")); break;
    case SYNT:  SET_STRING_ELT(list_names, i, Rf_mkChar("Synthetic")); break;
    case KEYP:  SET_STRING_ELT(list_names, i, Rf_mkChar("Keypoint")); break;
    case WITH:  SET_STRING_ELT(list_names, i, Rf_mkChar("Withheld")); break;
    case OVER:  SET_STRING_ELT(list_names, i, Rf_mkChar("Overlap")); break;
    case UD:    SET_STRING_ELT(list_names, i, Rf_mkChar("UserData")); break;
    case PSID:  SET_STRING_ELT(list_names, i, Rf_mkChar("PointSourceID")); break;
    case SA:    SET_STRING_ELT(list_names, i, Rf_mkChar("ScanAngle")); break;
    case R:     SET_STRING_ELT(list_names, i, Rf_mkChar("R")); break;
    case G:     SET_STRING_ELT(list_names, i, Rf_mkChar("G")); break;
    case B:     SET_STRING_ELT(list_names, i, Rf_mkChar("B")); break;
    case NIR:   SET_STRING_ELT(list_names, i, Rf_mkChar("NIR")); break;
    case CHAN:  SET_STRING_ELT(list_names, i, Rf_mkChar("Channel")); break;
    case BUFF:  SET_STRING_ELT(list_names, i, Rf_mkChar("Buffer")); break;
    default:    SET_STRING_ELT(list_names, i, Rf_mkChar(las->header->attributes[names[i]-100].name)); break;
    }
  }

  // Protecting the data.frame, protects its content
  UNPROTECT(nsexpprotected); nsexpprotected = 0;
  PROTECT(data_frame); nsexpprotected++;

  int j = 0;
  while (las->read_point())
  {
    bool buffer = las->point.inside_buffer(xmin, ymin, xmax, ymax, circular);
    if (drop_buffer && buffer) continue;

    for (int i = 0 ; i < nattr ; i++)
    {
      SEXP vector = VECTOR_ELT(data_frame, i);

      switch (names[i])
      {
        case X:     REAL(vector)[j] = las->point.get_x(); break;
        case Y:     REAL(vector)[j] = las->point.get_y(); break;
        case Z:     REAL(vector)[j] = las->point.get_z(); break;
        case I:     INTEGER(vector)[j] = las->point.get_intensity(); break;
        case T:     REAL(vector)[j] = las->point.get_gps_time(); break;
        case RN:    INTEGER(vector)[j] = (extended) ? (int)las->point.get_extended_return_number() : las->point.get_return_number(); break;
        case NOR:   INTEGER(vector)[j] = las->point.get_number_of_returns(); break;
        case SDF:   INTEGER(vector)[j] = las->point.get_scan_direction_flag(); break;
        case EoF:   INTEGER(vector)[j] = las->point.get_edge_of_flight_line(); break;
        case CLASS: INTEGER(vector)[j] = (extended) ? las->point.get_extended_classification() : las->point.get_classification(); break;
        case SYNT:  LOGICAL(vector)[j] = las->point.get_synthetic_flag(); break;
        case KEYP:  LOGICAL(vector)[j] = las->point.get_keypoint_flag(); break;
        case WITH:  LOGICAL(vector)[j] = las->point.get_withheld_flag(); break;
        case OVER:  LOGICAL(vector)[j] = las->point.get_extended_overlap_flag(); break;
        case UD:    INTEGER(vector)[j] = las->point.get_user_data(); break;
        case PSID:  INTEGER(vector)[j] = las->point.get_point_source_ID(); break;
        case SA:    REAL(vector)[j] = (extended) ? las->point.get_extended_scan_angle() : las->point.get_scan_angle_rank(); break;
        case R:     INTEGER(vector)[j] = las->point.get_R(); break;
        case G:     INTEGER(vector)[j] = las->point.get_G(); break;
        case B:     INTEGER(vector)[j] = las->point.get_B(); break;
        case NIR:   INTEGER(vector)[j] = las->point.get_NIR(); break;
        case CHAN:  INTEGER(vector)[j] = las->point.get_extended_scanner_channel(); break;
        case BUFF:  LOGICAL(vector)[j] = buffer; break;
        default: // extra bytes
        {
          int attr_index = names[i]-100;

          if (TYPEOF(vector) == INTSXP)
            INTEGER(vector)[j] = (int)las->point.get_attribute_as_float(attr_index);
          else
            REAL(vector)[j] = las->point.get_attribute_as_float(attr_index);
        }
      }
    }

    j++;
  }

  // All the points were not exposed
  // Set the correct length to R's vectors
  if (j != las->npoints)
  {
    for (int i = 0 ; i < nattr ; i++)
    {
      SEXP vector = VECTOR_ELT(data_frame, i);
      SETLENGTH(vector, j);
    }
  }

  // Create a data.frame with R attributes
  SEXP bbox = PROTECT(Rf_allocVector(REALSXP, 4)); nsexpprotected++;
  REAL(bbox)[0] = xmin; REAL(bbox)[1] = ymin; REAL(bbox)[2] = xmax; REAL(bbox)[3] = ymax;
  Rf_setAttrib(data_frame, R_ClassSymbol, Rf_ScalarString(Rf_mkChar("data.frame")));
  Rf_setAttrib(data_frame, R_NamesSymbol, list_names);
  Rf_setAttrib(data_frame, Rf_install("bbox"), bbox);

  // Name the rows (see https://stackoverflow.com/questions/37069149/creating-a-r-data-frame-in-c-c)
  SEXP rnms = PROTECT(Rf_allocVector(INTSXP, 2)); nsexpprotected++;
  INTEGER(rnms)[0] = NA_INTEGER;
  INTEGER(rnms)[1] = -j;
  Rf_setAttrib(data_frame, R_RowNamesSymbol, rnms);

  // Create a language object representing the function call
  SEXP lang = PROTECT(Rf_allocVector(LANGSXP,  Rf_length(args) + 2)); nsexpprotected++;
  SEXP elem = lang;
  SETCAR(elem, fun);
  SETCAR(CDR(elem), data_frame);
  for (int i = 0; i < Rf_length(args); ++i)
  {
    SETCAR(CDR(CDR(elem)), VECTOR_ELT(args, i));
    elem = CDR(elem);
  }

  // Evaluate the input R function within the data frame's environment
  SEXP res = PROTECT(Rf_eval(lang, R_GlobalEnv));  nsexpprotected++;

  // If modify = false or res is not a data.frame we can already push the result and return
  if (!modify || (TYPEOF(res) != VECSXP) || Rf_length(VECTOR_ELT(res, 0)) != las->npoints)
  {
    UNPROTECT(nsexpprotected); // unprotect all the vectors (ncols) + 5 objects created in this function;
    nsexpprotected = 0;
    PROTECT(res); nsexpprotected++;
    PROTECT(ans); nsexpprotected++;

    // push_back res in ans
    int i = Rf_length(ans);
    ans = Rf_lengthgets(ans, i+1);
    SET_VECTOR_ELT(ans, i, res);
    return true;
  }

  // Else it is the original data_frame. We update las with it.

  // for each element of the list get the name
  std::vector<int> col_names(Rf_length(res));
  SEXP names_attr = Rf_getAttrib(res, R_NamesSymbol);
  if (Rf_isNull(names_attr))
  {
    last_error = "the data.frame has no names";
    UNPROTECT(nsexpprotected);
    nsexpprotected = 0;
    return false;
  }

  // Check the name and find to which LAS attributes it coresponds
  for (int i = 0; i <  Rf_length(res); i++)
  {
    std::string sname(CHAR(STRING_ELT(names_attr, i)));
    if (sname == "X") col_names[i] = attributes::X;
    else if (sname == "Y") col_names[i] = attributes::Y;
    else if (sname == "Z") col_names[i] = attributes::Z;
    else if (sname == "Intensity") col_names[i] = attributes::I;
    else if (sname == "gpstime") col_names[i] = attributes::T;
    else if (sname == "ReturnNumber") col_names[i] = attributes::RN;
    else if (sname == "NumberOfReturns") col_names[i] = attributes::NOR;
    else if (sname == "ScanDirectionFlag") col_names[i] = attributes::SDF;
    else if (sname == "Classification") col_names[i] = attributes::CLASS;
    else if (sname == "Synthetic") col_names[i] = attributes::SYNT;
    else if (sname == "Keypoint") col_names[i] = attributes::KEYP;
    else if (sname == "Withheld") col_names[i] = attributes::WITH;
    else if (sname == "Overlap") col_names[i] = attributes::OVER;
    else if (sname == "ScanAngle") col_names[i] = attributes::SA;
    else if (sname == "UserData") col_names[i] = attributes::UD;
    else if (sname == "PointSourceID") col_names[i] = attributes::PSID;
    else if (sname == "Red") col_names[i] = attributes::R;
    else if (sname == "Blue") col_names[i] = attributes::G;
    else if (sname == "Green") col_names[i] = attributes::B;
    else if (sname == "NIR") col_names[i] = attributes::NIR;
    else if (sname == "Channel") col_names[i] = attributes::CHAN;
    else if (sname == "Buffer") col_names[i] = attributes::BUFF;
    else if (las->header->get_attribute_index(sname.c_str()) != -1) col_names[i] = las->header->get_attribute_index(sname.c_str()) + 100;
    else
    {
      col_names[i] = -1;
      warning("non supported column '%s'\n", sname.c_str());
    }
  }

  // for each element of the list check the TYPE is correct before to update some points
  for (int j = 0 ; j < Rf_length(res) ; j++)
  {
    SEXP vector = VECTOR_ELT(res, j);
    int type = TYPEOF(vector);

    switch(col_names[j])
    {
      case attributes::X: if (type != REALSXP) last_error = "X is expected to be numeric"; break;
      case attributes::Y: if (type != REALSXP) last_error = "Y is expected to be numeric"; break;
      case attributes::Z: if (type != REALSXP) last_error = "Z is expected to be numeric"; break;
      case attributes::I: if (type != INTSXP) last_error = "Intensity is expected to be integer"; break;
      case attributes::T: if (type != REALSXP) last_error = "gpstime is expected to be numeric"; break;
      case attributes::RN: if (type != INTSXP) last_error = "ReturnNumber is expected to be integer"; break;
      case attributes::NOR: if (type != INTSXP) last_error = "NumberOfReturns is expected to be integer"; break;
      case attributes::SDF: if (type != LGLSXP) last_error = "ScanDirectionFlag is expected to be logical"; break;
      case attributes::CLASS: if (type != INTSXP) last_error = "Classification is expected to be integer"; break;
      case attributes::SYNT: if (type != LGLSXP) last_error = "Synthetic is expected to be logical"; break;
      case attributes::KEYP: if (type != LGLSXP) last_error = "Keypoint is expected to be logical"; break;
      case attributes::WITH: if (type != LGLSXP) last_error = "Withheld is expected to be logical"; break;
      case attributes::OVER: if (type != LGLSXP) last_error = "Overlap is expected to be logical"; break;
      case attributes::SA: if (type != REALSXP) last_error = "ScanAngle is expected to be numeric"; break;
      case attributes::UD: if (type != INTSXP) last_error = "UserData is expected to be integer"; break;
      case attributes::PSID: if (type != INTSXP) last_error = "PointSourceID is expected to be integer"; break;
      case attributes::R: if (type != INTSXP) last_error = "R is expected to be integer"; break;
      case attributes::G: if (type != INTSXP) last_error = "G is expected to be integer"; break;
      case attributes::B: if (type != INTSXP) last_error = "B is expected to be integer"; break;
      case attributes::NIR: if (type != INTSXP) last_error = "NIR is expected to be integer"; break;
      case attributes::CHAN: if (type != INTSXP) last_error = "Channel is expected to be integer"; break;
      case attributes::BUFF: break;
      case -1: break;
      default: //exra bytes
      {
        int attr_index = col_names[j]-100;
        LASattribute attr = las->point.attributer->attributes[attr_index];

        if (type ==  INTSXP)
        {
          if (attr.data_type == LAS::FLOAT || attr.data_type == LAS::DOUBLE)
          {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s is expected to be numeric", attr.name);
            last_error = std::string(buffer);
          }
        }

        break;
      }
    }

    if (!last_error.empty())
    {
      UNPROTECT(nsexpprotected); nsexpprotected = 0;
      return false;
    }
  }

  // Update the LAS
  while (las->read_point())
  {
    bool buffer = las->point.inside_buffer(xmin, ymin, xmax, ymax, circular);
    if (drop_buffer && buffer) continue;

    int i = las->current_point;

    // for each element of the list
    for (int j = 0 ; j < Rf_length(res) ; j++)
    {
      SEXP vector = VECTOR_ELT(res, j);
      int type = TYPEOF(vector);

      switch(col_names[j])
      {
        case attributes::X: las->point.set_x(REAL(vector)[i]); break;
        case attributes::Y: las->point.set_y(REAL(vector)[i]); break;
        case attributes::Z: las->point.set_z(REAL(vector)[i]); break;
        case attributes::I: las->point.set_intensity(INTEGER(vector)[i]); break;
        case attributes::T: las->point.set_gps_time(REAL(vector)[i]); break;
        case attributes::RN: las->point.set_return_number(INTEGER(vector)[i]); break;
        case attributes::NOR: las->point.set_number_of_returns(INTEGER(vector)[i]); break;
        case attributes::SDF: las->point.set_scan_direction_flag(LOGICAL(vector)[i]); break;
        case attributes::CLASS: las->point.set_classification(INTEGER(vector)[i]); break;
        case attributes::SYNT: las->point.set_synthetic_flag(LOGICAL(vector)[i]); break;
        case attributes::KEYP: las->point.set_keypoint_flag(LOGICAL(vector)[i]); break;
        case attributes::WITH: las->point.set_withheld_flag(LOGICAL(vector)[i]); break;
        case attributes::OVER: las->point.set_extended_overlap_flag(LOGICAL(vector)[i]); break;
        case attributes::SA: las->point.set_scan_angle(REAL(vector)[i]); break;
        case attributes::UD: las->point.set_user_data(INTEGER(vector)[i]); break;
        case attributes::PSID: las->point.set_point_source_ID(INTEGER(vector)[i]); break;
        case attributes::R: las->point.set_R(INTEGER(vector)[i]); break;
        case attributes::G: las->point.set_G(INTEGER(vector)[i]); break;
        case attributes::B: las->point.set_B(INTEGER(vector)[i]); break;
        case attributes::NIR: las->point.set_NIR(INTEGER(vector)[i]); break;
        case attributes::CHAN: las->point.set_extended_scanner_channel(INTEGER(vector)[i]); break;
        case attributes::BUFF: break;
        case -1: break;
        default: //exra bytes
        {
          int attr_index = col_names[j]-100;
          LASattribute attr = las->point.attributer->attributes[attr_index];

          if (type ==  INTSXP)
          {
            int val = INTEGER(vector)[i];

            switch(attr.data_type)
            {
              case LAS::UCHAR:  { U8 u  = U8_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
              case LAS::CHAR:   { I8 u  = I8_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
              case LAS::USHORT: { U16 u = U16_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
              case LAS::SHORT:  { I16 u = I16_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
              case LAS::LONG:   { I32 u = I32_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
            }
          }
          else if (type ==  REALSXP)
          {
            double val = REAL(vector)[i];
            val = (val - attr.offset[0])/attr.scale[0];

            switch(attr.data_type)
            {
            case LAS::UCHAR:  { U8  u = U8_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::CHAR:   { I8  u = I8_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::USHORT: { U16 u = U16_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::SHORT:  { I16 u = I16_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::LONG:   { I32 u = I32_CLAMP(val); las->point.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::FLOAT:  { F32 u = (F32)val; las->point.set_attribute(attr_index, (U8*)&u); break; }
            case LAS::DOUBLE: { F64 u = (F64)val; las->point.set_attribute(attr_index, (U8*)&u); break; }
            }
          }

          break;
        }
      }
    }

    las->update_point();
  }

  // unprotect all the vectors (ncols) + 5 objects created in this function;
  UNPROTECT(nsexpprotected); nsexpprotected = 0;
  PROTECT(ans); nsexpprotected++;

  return true;
}

SEXP LASRcallback::to_R()
{
  if (Rf_length(ans) == 0)
    return R_NilValue;

  if (Rf_length(ans) == 1)
    return VECTOR_ELT(ans, 0);

  if (Rf_length(ans) == (int)filenames.size())
  {
    SEXP names = PROTECT(Rf_allocVector(STRSXP, filenames.size())); nsexpprotected++;
    for (int i = 0 ; i < (int)filenames.size() ; ++i) SET_STRING_ELT(names, i, Rf_mkChar(filenames[i].c_str()));
    Rf_setAttrib(ans, R_NamesSymbol, names);
  }

  return ans;
}

#endif