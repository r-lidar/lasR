#include "aggregate.h"

#ifdef USING_R

LASRaggregate::LASRaggregate(double xmin, double ymin, double xmax, double ymax, double res, double window, SEXP call, SEXP env)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->call = call;
  this->env = env;
  this->window = (window > res) ? (window-res)/2 : 0;

  nmetrics = 1;
  expected_type = ANYSXP;

  raster = Raster(xmin, ymin, xmax, ymax, res);
}

bool LASRaggregate::process(LAS*& las)
{
  // Pre-compute the groups for each point
  std::vector<int> cells;
  while (las->read_point(true)) // Need to include withheld points to do not mess grouper indexes
  {
    if (lasfilter.filter(&las->point)) continue; // TODO: check why and if we need that one

    double x = las->point.get_x();
    double y = las->point.get_y();

    if (window)
      raster.get_cells(x-window,y-window, x+window,y+window, cells);
    else
      cells.push_back(raster.cell_from_xy(x,y));

    grouper.insert(cells);
    cells.clear();
  }

  // LAS format
  bool extended = (las->header->version_minor >= 4) && (las->header->point_data_format >= 6);
  bool has_gps = las->point.have_gps_time;
  bool has_rgb = las->point.have_rgb;
  bool has_nir = las->point.have_nir;

  // Error handling
  int error = 0;

  // List all existing attributes
  std::vector<int> names = {X, Y, Z, I, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, SA, UD, PSID};
  if (has_gps) names.push_back(T);
  if (has_rgb) { names.push_back(R); names.push_back(G); names.push_back(B); }
  if (has_nir) { names.push_back(NIR); }
  if (las->header->number_attributes > 0)
  {
    for (int i = 0 ; i < las->header->number_attributes ; ++i)
    {
      if (las->is_attribute_loadable(i))
        names.push_back(i+100);
    }
  }
  int nattr = names.size(); // Number of attribute for this point format

  // Size of the largest group (i.e. the pixel with most numerous points)
  int nalloc = grouper.largest_group_size(); // Size of the biggest group

  // Create environments in which the call takes place
  SEXP list = PROTECT(Rf_allocVector(VECSXP, nattr)); nsexpprotected++;

  // Populate the list by allocating vectors of size nalloc to store lidar data.
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
    else if (name == SDF ||  name ==  SYNT || name == KEYP || name == WITH || name== OVER)
      type = LGLSXP;
    else
      type = INTSXP;

    SEXP v = PROTECT(Rf_allocVector(type, nalloc)); nsexpprotected++;
    SET_VECTOR_ELT(list, i, v);
  }

  // Assign names to the list
  SEXP list_names = PROTECT(Rf_allocVector(STRSXP, Rf_length(list))); nsexpprotected++;
  for (int i = 0; i < Rf_length(list); i++)
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
    case EoF:   SET_STRING_ELT(list_names, i, Rf_mkChar("EdgeOfFlightline")); break;
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
    default:    SET_STRING_ELT(list_names, i, Rf_mkChar(las->header->attributes[names[i]-100].name)); break;
    }
  }
  Rf_setAttrib(list, R_NamesSymbol, list_names);
  UNPROTECT(1); nsexpprotected--; // Why ??

  // Create an environment with pointers to the vectors allocated in the list (no copy)
  // So, updating the list updates the environment
  // (I was not able to assign and loop directly into an environment)
  for (int i = 0; i < Rf_length(list); i++)
  {
    SEXP value = VECTOR_ELT(list, i);
    Rf_defineVar(Rf_install(CHAR(STRING_ELT(list_names, i))), value, env);
  }

  // Loop through each group on which we want to apply the call
  auto map = grouper.map;

  progress->reset();
  progress->set_prefix("Rasterize");
  progress->set_total(map.size());

  for (const auto& pair : map)
  {
    int group = pair.first;
    las->set_intervals_to_read(pair.second);

    // Read the points of the query and populate the list
    int j = 0;
    while (las->read_point())
    {
      if (lasfilter.filter(&las->point)) continue;

      for (int i = 0 ; i < Rf_length(list) ; i++)
      {
        SEXP vector = VECTOR_ELT(list, i);

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

    if (j == 0) continue;

    // Set the correct length to R's vectors
    for (int i = 0; i < Rf_length(list); i++)
    {
      SEXP vector = VECTOR_ELT(list, i);
      SETLENGTH(vector, j);
    }

    // Eval user defined expression
    SEXP res = R_tryEvalSilent(call, env, &error);

    // There is an error while evaluating user expression: exit
    if (error)
    {
      last_error = R_curErrorBuf();
      break;
    }

    if (TYPEOF(res) != REALSXP && TYPEOF(res) != INTSXP && TYPEOF(res) != VECSXP)
    {
      error = 1;
      last_error = "user expression must return a vector of numbers or a list of atomic numbers";
      break;
    }

    // The output is a vector: check if it is atomic otherwise exit
    /*if ((TYPEOF(res) == REALSXP || TYPEOF(res) == INTSXP) && Rf_length(res) > 1)
    {
      error = 1;
      last_error = "user expression must return an atomic number or a list of atomic numbers";
      break;
    }*/

    // The output is a list: check if each element is atomic otherwise exit
    if (TYPEOF(res) == VECSXP)
    {
      for (int i = 0; i < Rf_length(res); i++)
      {
        SEXP v = VECTOR_ELT(res, i);

        if (TYPEOF(v) != REALSXP && TYPEOF(v) != INTSXP)
        {
          error = 1;
          last_error = "user expression must only return numbers";
          break;
        }

        if (Rf_length(v) > 1)
        {
          error = 1;
          last_error = "user expression must only return a vector of numbers or a list of atomic numbers";
          break;
        }
      }

      if (error) break;
    }

    // This is the first time we evaluate the user-expression: set-up objects to save the result
    // and record some properties of the result because next results must match the first one
    if (expected_type == ANYSXP)
    {
      expected_type = TYPEOF(res);

      // how many metrics are supposed to be returned
      nmetrics = Rf_length(res);
      if (nmetrics > 1) raster.set_nbands(nmetrics);

      SEXP names = Rf_getAttrib(res, R_NamesSymbol);

      if (names != R_NilValue)
      {
        for (int i = 0 ; i < Rf_length(names) ; ++i)
        {
          const char*  name = CHAR(STRING_ELT(names, i));
          raster.set_band_name(std::string(name), i);
        }
      }
    }
    else
    {
      if (Rf_length(res) != nmetrics)
      {
        error = 1;
        last_error = "user expression returned an inconsistant number of items";
        break;
      }
    }

    // Assign the values. We are now sure everything is ok
    if (TYPEOF(res) == REALSXP)
    {
      for (int i = 0; i < Rf_length(res); i++)
      {
        double value = REAL(res)[i];
        raster.set_value(group, (float)value, i+1);
      }
    }
    else if (TYPEOF(res) == INTSXP)
    {
      for (int i = 0; i < Rf_length(res); i++)
      {
        int value = INTEGER(res)[i];
        raster.set_value(group, (float)value, i+1);
      }
    }
    else if (TYPEOF(res) == VECSXP)
    {
      for (int i = 0; i < Rf_length(res); i++)
      {
        SEXP element = VECTOR_ELT(res, i);
        if (TYPEOF(element) == REALSXP)
        {
          double value = REAL(element)[0];
          raster.set_value(group, (float)value, i+1);
        }
        else if (TYPEOF(element) == INTSXP)
        {
          int value = INTEGER(element)[0];
          raster.set_value(group, (float)value, i+1);
        }
      }
    }

    (*progress)++;
    progress->show();
  }

  // Restore the true length otherwise memory leak (??)
  for (int i = 0; i < Rf_length(list); i++)
  {
    SEXP vector = VECTOR_ELT(list, i);
    SETLENGTH(vector, nalloc);
  }

  UNPROTECT(nsexpprotected); nsexpprotected = 0;

  return (error == 0);
}

void LASRaggregate::clear(bool last)
{
  grouper.clear();
}

#endif