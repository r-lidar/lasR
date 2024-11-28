#include "aggregate.h"

#ifdef USING_R

LASRaggregate::LASRaggregate()
{
  expected_type = ANYSXP;
}

bool LASRaggregate::set_parameters(const nlohmann::json& stage)
{
  std::string address_call_str = stage.at("call");
  std::string address_env_str = stage.at("env");
  call = string_address_to_sexp(address_call_str);
  env = string_address_to_sexp(address_env_str);

  double res = stage.at("res");
  window = stage.at("window");
  window = (window > res) ? (window-res)/2 : 0;

  // This is the expected number of metrics. We need to know it to be able to create
  // the appropriate number of layers in the raster. We need to record it to check, in
  // in each pixel, if the user-defined function returns the proper number of metrics
  nmetrics = stage.at("nmetrics");

  raster = Raster(xmin, ymin, xmax, ymax, res, nmetrics);

  return true;
}

bool LASRaggregate::process(LAS*& las)
{
  // Pre-compute the groups for each point
  std::vector<int> cells;
  while (las->read_point(true)) // Need to include withheld points to do not mess grouper indexes
  {
    double x = las->p.get_x();
    double y = las->p.get_y();

    if (window)
      raster.get_cells(x-window,y-window, x+window,y+window, cells);
    else
      cells.push_back(raster.cell_from_xy(x,y));

    grouper.insert(cells);
    cells.clear();
  }

  int error = 0;  // Error handling
  int nattr = las->newheader->schema.attributes.size();
  int nalloc = grouper.largest_group_size();   // Size of the largest group (i.e. the pixel with most numerous points)

  #pragma omp critical (RAPI)
  {
  // Create environments in which the call takes place
  SEXP list = PROTECT(Rf_allocVector(VECSXP, nattr)); nsexpprotected++;
  SEXP list_names = PROTECT(Rf_allocVector(STRSXP, nattr)); nsexpprotected++;

  // Configure accessors
  std::vector<AttributeHandler> accessors; accessors.resize(nattr);
  std::vector<int> sexp_types; sexp_types.resize(nattr);

  // Populate the list by allocating vectors of size nalloc to store points data.
  for (int i = 0 ; i < nattr ; i++)
  {
    const Attribute& attribute = las->newheader->schema.attributes[i];
    std::string name = attribute.name;
    int sexp_type = (attribute.type >= AttributeType::FLOAT || attribute.scale_factor != 1 || attribute.value_offset != 0) ? REALSXP : INTSXP;

    SEXP v = PROTECT(Rf_allocVector(sexp_type, nalloc)); nsexpprotected++;
    SET_VECTOR_ELT(list, i, v);
    SET_STRING_ELT(list_names, i, Rf_mkChar(name.c_str()));

    accessors[i] = AttributeHandler(name);
    sexp_types[i] = sexp_type;
  }

  // Assign names to the list
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
      if (pointfilter.filter(&las->p)) continue;

      for (int i = 0 ; i < Rf_length(list) ; i++)
      {
        SEXP vector = VECTOR_ELT(list, i);

        if (sexp_types[i] == REALSXP)
          REAL(vector)[j] = accessors[i](&las->p);
        else
          INTEGER(vector)[j] = accessors[i](&las->p);
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

      // How many metrics are supposed to be returned. Check if it matches the declaration
      if (Rf_length(res) != nmetrics)
      {
        error = 1;
        last_error = "user expression does not return the number of items declared (" + std::to_string(Rf_length(res)) + " vs. " +  std::to_string(nmetrics) + ")";
        break;
      }

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
    // This is not the first times we evaluate the user defined expression. Check if
    // it matches with previous evaluations
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

  } // end omp critical

  return (error == 0);
}

void LASRaggregate::clear(bool last)
{
  grouper.clear();
}

#endif