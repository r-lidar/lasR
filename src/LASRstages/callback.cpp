#include "callback.h"
#include <unordered_set>

#ifdef USING_R

LASRcallback::LASRcallback()
{
  ans = R_NilValue;
  ans = PROTECT(Rf_allocVector(VECSXP, 0)); nsexpprotected++;
}

bool LASRcallback::set_parameters(const nlohmann::json& stage)
{
  select = stage.at("expose");
  modify = !stage.at("no_las_update");
  drop_buffer = stage.at("drop_buffer");

  std::string address_fun_str = stage.at("fun");
  std::string address_args_str = stage.at("args");
  fun = string_address_to_sexp(address_fun_str);
  args = string_address_to_sexp(address_args_str);

  // Parse select = "*"
  std::string all = "xyzitrndecskwoaupRGBNCbE";
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

  select = parsed;

  return true;
}

bool LASRcallback::process(LAS*& las)
{
  int error = 0;

  int withheld_index = -1; // for backward compatibility
  int buffered_index = -1; // for backward compatibility

  // List all selected attributes by index
  std::vector<std::string> names;
  std::vector<AttributeAccessor> accessors;
  std::vector<Attribute> attributes;
  int k = 0;
  for(char c : select)
  {
    std::string name;

    // Handle extrabytes in a (not elegant) backward compatible way
    if (c >= '0' && c <= '9')
    {
      int i = c - '0';
      int j = 0;
      for (auto attribute : las->header->schema.attributes)
      {
        if (lascoreattributes.count(attribute.name) == 0)
        {
          if (j == i)
          {
            name = attribute.name;
            break;
          }
          else
          {
            j++;
          }
        }
      }
    }
    // Handle extrabytes in a (not elegant) backward compatible way
    else if (c == 'E')
    {
      for (auto attribute : las->header->schema.attributes)
      {
        if (lascoreattributes.count(attribute.name) == 0)
        {
          name = attribute.name;
          names.push_back(name);
          accessors.push_back(AttributeAccessor(name));
          attributes.push_back(attribute);
        }
      }
      continue;
    }
    // Backward compatibility
    else if (c == 'b')
    {
      name = "Buffer";
      buffered_index = k;
      names.push_back(name);
      accessors.push_back(AttributeAccessor(name));
      attributes.push_back(Attribute(name, AttributeType::NOTYPE));
    }
    else
    {
      name = std::string(1, c);
      name = map_attribute(name);
    }

    int index = las->header->schema.get_attribute_index(name);
    if (index < 0) continue;
    name = las->header->schema.attributes[index].name;
    names.push_back(name);
    accessors.push_back(AttributeAccessor(name));
    attributes.push_back(las->header->schema.attributes[index]);

    k++;
  }

  int nattr = names.size(); // Number of attribute to expose

  #pragma omp critical (RAPI)
  {
  // Create environments in which the call takes place
  SEXP data_frame = PROTECT(Rf_allocVector(VECSXP, nattr)); nsexpprotected++;

  // Populate the list by allocating vectors to store lidar data.
  for (int i = 0 ; i < nattr ; i++)
  {
    std::string name = names[i];
    int type = (attributes[i].type >= AttributeType::FLOAT || attributes[i].scale_factor != 1 || attributes[i].value_offset != 0) ? REALSXP : INTSXP;

    SEXP v = PROTECT(Rf_allocVector(type, las->npoints)); nsexpprotected++;
    SET_VECTOR_ELT(data_frame, i, v);
  }

  // Assign names to the data_frame
  SEXP list_names = PROTECT(Rf_allocVector(STRSXP, nattr)); nsexpprotected++;
  for (int i = 0; i < nattr; i++)
  {
    std::string name = names[i];
    SET_STRING_ELT(list_names, i, Rf_mkChar(name.c_str()));
  }

  // Protecting the data.frame, protects its content
  UNPROTECT(nsexpprotected); nsexpprotected = 0;
  PROTECT(data_frame); nsexpprotected++;

  int j = 0;
  while (las->read_point())
  {
    bool buffer = las->p.get_buffered();
    if (drop_buffer && buffer) continue;

    for (int i = 0 ; i < nattr ; i++)
    {
      double value;
      bool use_realsexp;

      SEXP vector = VECTOR_ELT(data_frame, i);

      if (i != buffered_index)
      {
        use_realsexp = attributes[i].type >= AttributeType::FLOAT || attributes[i].scale_factor != 1 || attributes[i].value_offset != 0;
        value = accessors[i](&las->p);
      }
      else
      {
        use_realsexp = false;
        value = (double)las->p.get_buffered();
      }


      if (use_realsexp)
        REAL(vector)[j] = value;
      else
        INTEGER(vector)[j] = (int)value;
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
  SEXP res = PROTECT(R_tryEvalSilent(lang, R_GlobalEnv, &error));  nsexpprotected++;
  if (error)
  {
    last_error = R_curErrorBuf();
  }

  // If modify = false or res is not a data.frame we can already push the result and return
  if (!error &&  (!modify || (TYPEOF(res) != VECSXP) || Rf_length(VECTOR_ELT(res, 0)) != las->npoints))
  {
    UNPROTECT(nsexpprotected); // unprotect all the vectors (ncols) + 5 objects created in this function;
    nsexpprotected = 0;
    PROTECT(res); nsexpprotected++;
    PROTECT(ans); nsexpprotected++;

    // push_back res in ans
    int i = Rf_length(ans);
    ans = Rf_lengthgets(ans, i+1);
    SET_VECTOR_ELT(ans, i, res);
  }
  // Else it is the original data_frame. We update las with it.
  else if (!error)
  {
    // for each element of the list get the name
    std::vector<int> col_names(Rf_length(res));
    SEXP names_attr = Rf_getAttrib(res, R_NamesSymbol);
    if (Rf_isNull(names_attr))
    {
      last_error = "the data.frame has no names";
      UNPROTECT(nsexpprotected);
      nsexpprotected = 0;
      error = 1;
    }

    if (!error)
    {
      // Check the name and find to which LAS attributes it corresponds
      accessors.clear();
      for (int i = 0; i < Rf_length(res); i++)
      {
        std::string name(CHAR(STRING_ELT(names_attr, i)));
        if (name == "Withheld")
        {
          withheld_index = i;
        }
        else if (name == "Buffer")
        {
          buffered_index = i;
        }
        else if (!las->header->schema.has_attribute(name))
        {
          last_error = "non supported column '" + name +"'";
          UNPROTECT(nsexpprotected);
          nsexpprotected = 0;
          error = 1;
          break;
        }

        accessors.push_back(name);
      }
    }

    if (!error)
    {
      // Update the LAS
      while (las->read_point())
      {
        bool buffer = las->p.inside_buffer(xmin, ymin, xmax, ymax, circular);
        if (drop_buffer && buffer) continue;

        int i = las->current_point;

        // for each element of the list
        for (int j = 0 ; j < Rf_length(res) ; j++)
        {
          SEXP vector = VECTOR_ELT(res, j);
          int type = TYPEOF(vector);

          // Backward compatibility
          if (j == withheld_index)
          {
            double val;
            if (type == REALSXP)
             val = REAL(vector)[i];
            else if (type == LGLSXP)
             val = (double)LOGICAL(vector)[i];
            else
             val = (double)INTEGER(vector)[i];

            las->p.set_deleted(val > 0);
            continue;
          }

          // Backward compatibility
          if (j == buffered_index)
          {
            double val;
            if (type == REALSXP)
              val = REAL(vector)[i];
            else if (type == LGLSXP)
              val = (double)LOGICAL(vector)[i];
            else
              val = (double)INTEGER(vector)[i];

            las->p.set_buffered(val > 0);
            continue;
          }

          if (type == REALSXP)
            accessors[j](&las->p, REAL(vector)[i]);
          else if (type == LGLSXP)
            accessors[j](&las->p, (double)LOGICAL(vector)[i]);
          else
            accessors[j](&las->p, (double)INTEGER(vector)[i]);
        }

        las->update_point();
      }
    }

    // unprotect all the vectors (ncols) + 5 objects created in this function;
    UNPROTECT(nsexpprotected); nsexpprotected = 0;
    PROTECT(ans); nsexpprotected++;
  }

  } // end omp critical

  las->update_header();

  return error == 0;
}

void LASRcallback::merge(const Stage* other)
{
  const LASRcallback* o = dynamic_cast<const LASRcallback*>(other);

  #pragma omp critical (RAPI)
  {
    int nnew = Rf_length(o->ans);
    int nmain = Rf_length(this->ans);
    this->ans = Rf_lengthgets(this->ans, nmain+nnew);
    for (int i = 0 ; i < nnew ; i++)
      SET_VECTOR_ELT(this->ans, nmain+i, VECTOR_ELT(o->ans, i));
  }
}


void LASRcallback::sort(const std::vector<int>& order)
{
  int n = Rf_length(ans);

  if (n <= 1) return;

  SEXP tmp = PROTECT(Rf_allocVector(VECSXP, n)); nsexpprotected++;

  for (size_t i = 0 ; i < order.size() ; ++i)
    SET_VECTOR_ELT(tmp, order[i], VECTOR_ELT(ans, i));

  UNPROTECT(1); nsexpprotected--;

  std::swap(tmp, ans);
}

SEXP LASRcallback::to_R()
{
  if (Rf_length(ans) == 0)
    return R_NilValue;

  if (Rf_length(ans) == 1)
    return VECTOR_ELT(ans, 0);

  return ans;
}

#endif