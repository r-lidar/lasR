#ifdef USING_R
#include "Rversion.h"
#include "callback.h"
#include <unordered_set>

namespace{
  // From data.table https://github.com/Rdatatable/data.table/pull/7451/changes
  #if R_VERSION < R_Version(4, 6, 0)
  # define BACKPORT_RESIZABLE_API
  # define R_allocResizableVector(type, maxlen) R_allocResizableVector_(type, maxlen)
  # define R_duplicateAsResizable(x) R_duplicateAsResizable_(x)
  # define R_maxLength(x) R_maxLength_(x)
  static inline R_xlen_t R_maxLength_(SEXP x) {
    return IS_GROWABLE(x) ? TRUELENGTH(x) : XLENGTH(x);
  }
  # define R_isResizable(x) R_isResizable_(x)
  static inline bool R_isResizable_(SEXP x) {
    // IS_GROWABLE checks for XLENGTH < TRUELENGTH instead
    return (LEVELS(x) & 0x20) && XLENGTH(x) <= TRUELENGTH(x);
  }
  # define R_resizeVector(x, newlen) R_resizeVector_(x, newlen)
  #endif

  #ifdef BACKPORT_RESIZABLE_API
  SEXP R_allocResizableVector_(SEXPTYPE type, R_xlen_t maxlen) {
    SEXP ret = Rf_allocVector(type, maxlen);
    SET_TRUELENGTH(ret, maxlen);
    SET_GROWABLE_BIT(ret);
    return ret;
  }

  void R_resizeVector_(SEXP x, R_xlen_t newlen) {
    if (!R_isResizable(x))
      throw std::runtime_error("attempt to resize a non-resizable vector"); // # nocov
    if (newlen > XTRUELENGTH(x))
      throw std::runtime_error("newlen exceeds maxlen"); // # nocov
    SETLENGTH(x, newlen);
  }
  #endif
}


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

bool LASRcallback::process(PointCloud*& las)
{
  int error = 0;
  int np = las->header->number_of_point_records;

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
      attributes.push_back(Attribute(name, AttributeType::UINT8));
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

  //#pragma omp critical (RAPI)
  //{

  // Create environments in which the call takes place
  SEXP data_frame = PROTECT(Rf_allocVector(VECSXP, nattr)); nsexpprotected++;

  // Populate the list by allocating vectors to store lidar data.
  for (int i = 0 ; i < nattr ; i++)
  {
    std::string name = names[i];

    // Choose if we allocate R REALSXP OR INTSXP vector
    int type = (attributes[i].type >= AttributeType::FLOAT || attributes[i].scale_factor != 1 || attributes[i].value_offset != 0) ? REALSXP : INTSXP;

    // We overallocate npoints. We may actually have less than that.
    SEXP v = PROTECT(R_allocResizableVector(type, las->npoints)); nsexpprotected++;
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

  int p_count = 0;
  int skipped = 0;
  while (las->read_point())
  {
    bool buffer = las->point.get_buffered();
    if (drop_buffer && buffer)
    {
      skipped++;
      continue;
    }

    for (int i = 0 ; i < nattr ; i++)
    {
      double value;
      bool use_realsexp;

      SEXP vector = VECTOR_ELT(data_frame, i);

      if (i != buffered_index)
      {
        use_realsexp = attributes[i].type >= AttributeType::FLOAT || attributes[i].scale_factor != 1 || attributes[i].value_offset != 0;
        value = accessors[i](&las->point);
      }
      else
      {
        use_realsexp = false;
        value = (double)las->point.get_buffered();
      }


      if (use_realsexp)
        REAL(vector)[p_count] = value;
      else
        INTEGER(vector)[p_count] = (int)value;
    }

    p_count++;
  }

  if (p_count + skipped != np)
  {
    // This should not happen but actually happened because of a uninitialized memory bug
    // Now we check for it anyway.
    last_error = "Internal error: unexpected difference in point count";
    return false;
  }

  // All the points were not exposed
  // Set the correct length to R's vectors
  if (p_count != las->npoints)
  {
    for (int i = 0 ; i < nattr ; i++)
    {
      SEXP vector = VECTOR_ELT(data_frame, i);
      R_resizeVector(vector, p_count);
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
  INTEGER(rnms)[1] = -p_count;
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
  if (!error &&  (!modify || (TYPEOF(res) != VECSXP) || Rf_length(VECTOR_ELT(res, 0)) != p_count))
  {
    UNPROTECT(nsexpprotected); // unprotect all the vectors (ncols) + 5 objects created in this function;
    nsexpprotected = 0;
    PROTECT(res); nsexpprotected++;
    PROTECT(ans); nsexpprotected++;

    if (verbose)  print(" Output the result\n");

    // push_back res in ans
    int i = Rf_length(ans);
    ans = Rf_lengthgets(ans, i+1);
    SET_VECTOR_ELT(ans, i, res);
  }
  // Else it is the original data_frame. We update las with it.
  else if (!error)
  {

    if (verbose) print(" Edit the point cloud\n");

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
      int i = 0;
      while (las->read_point())
      {
        bool buffer = las->point.inside_buffer(xmin, ymin, xmax, ymax, circular);
        if (drop_buffer && buffer) continue;

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

            las->point.set_deleted(val > 0);
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

            las->point.set_buffered(val > 0);
            continue;
          }

          if (type == REALSXP)
            accessors[j](&las->point, REAL(vector)[i]);
          else if (type == LGLSXP)
            accessors[j](&las->point, (double)LOGICAL(vector)[i]);
          else
            accessors[j](&las->point, (double)INTEGER(vector)[i]);
        }

        i++;
      }
    }

    // unprotect all the vectors (ncols) + 5 objects created in this function;
    UNPROTECT(nsexpprotected); nsexpprotected = 0;
    PROTECT(ans); nsexpprotected++;
  }

  //} // end omp critical

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

#ifdef USING_PYTHON

#include "callback.h"

#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace
{
  struct PyCallback
  {
    py::object callable;
    py::tuple args;
  };

  std::mutex& callback_registry_mutex()
  {
    static auto* mutex = new std::mutex();
    return *mutex;
  }

  std::unordered_map<std::string, PyCallback>& callback_registry()
  {
    static auto* registry = new std::unordered_map<std::string, PyCallback>();
    return *registry;
  }

  unsigned long long& callback_registry_counter()
  {
    static auto* counter = new unsigned long long(0);
    return *counter;
  }

  PyCallback get_python_callback(const std::string& id)
  {
    std::lock_guard<std::mutex> lock(callback_registry_mutex());
    auto& registry = callback_registry();
    auto it = registry.find(id);
    if (it == registry.end())
      throw std::runtime_error("unknown Python callback id: " + id);

    return it->second;
  }

  std::string normalize_select(std::string select)
  {
    std::string all = "xyzitrndecskwoaupRGBNCbE";
    size_t pos = select.find('*');
    if (pos != std::string::npos) select = all;

    std::string parsed;
    std::unordered_set<char> seen;
    for (char c : select)
    {
      if (seen.insert(c).second)
        parsed += c;
    }

    return parsed;
  }

  struct CallbackColumn
  {
    std::string name;
    AttributeAccessor accessor;
    Attribute attribute = Attribute("", AttributeType::NOTYPE);
    bool is_buffer = false;
    std::vector<double> values;
  };

  void add_column(
    PointCloud* las,
    const std::string& name,
    std::vector<CallbackColumn>& columns,
    bool is_buffer = false)
  {
    CallbackColumn column;
    column.name = name;
    column.is_buffer = is_buffer;
    column.accessor = AttributeAccessor(name);

    if (is_buffer)
    {
      column.attribute = Attribute(name, AttributeType::UINT8);
    }
    else
    {
      int index = las->header->schema.get_attribute_index(name);
      if (index < 0) return;
      column.name = las->header->schema.attributes[index].name;
      column.attribute = las->header->schema.attributes[index];
    }

    column.values.reserve(las->npoints);
    columns.push_back(column);
  }

  py::array_t<double> to_numpy_array(const std::vector<double>& values)
  {
    py::array_t<double> array(values.size());
    auto out = array.mutable_unchecked<1>();
    for (py::ssize_t i = 0; i < static_cast<py::ssize_t>(values.size()); ++i)
      out(i) = values[i];
    return array;
  }

  py::tuple make_python_call_args(const py::dict& data, const py::tuple& args)
  {
    py::tuple call_args(args.size() + 1);
    call_args[0] = data;
    for (py::ssize_t i = 0; i < args.size(); ++i)
      call_args[i + 1] = args[i];
    return call_args;
  }

  std::string py_object_to_string(const py::handle& object)
  {
    return py::str(object).cast<std::string>();
  }
}

namespace lasr_python_callback
{
  std::string register_callback(py::object callable, py::tuple args)
  {
    std::lock_guard<std::mutex> lock(callback_registry_mutex());
    std::string id = "py_callback_" + std::to_string(++callback_registry_counter());
    callback_registry().emplace(id, PyCallback{std::move(callable), std::move(args)});
    return id;
  }
}

bool LASRcallback::set_parameters(const nlohmann::json& stage)
{
  select = normalize_select(stage.value("expose", "xyz"));
  modify = !stage.value("no_las_update", false);
  drop_buffer = stage.value("drop_buffer", false);
  callback_id = stage.at("fun").get<std::string>();
  return true;
}

bool LASRcallback::process(PointCloud*& las)
{
  if (!las)
  {
    last_error = "Uninitialized pointer to LAS object";
    return false;
  }

  py::gil_scoped_acquire gil;

  try
  {
    PyCallback callback = get_python_callback(callback_id);

    std::vector<CallbackColumn> columns;
    for (char c : select)
    {
      if (c >= '0' && c <= '9')
      {
        int requested_extra = c - '0';
        int current_extra = 0;
        for (const auto& attribute : las->header->schema.attributes)
        {
          if (lascoreattributes.count(attribute.name) == 0)
          {
            if (current_extra == requested_extra)
            {
              add_column(las, attribute.name, columns);
              break;
            }
            current_extra++;
          }
        }
      }
      else if (c == 'E')
      {
        for (const auto& attribute : las->header->schema.attributes)
        {
          if (lascoreattributes.count(attribute.name) == 0)
            add_column(las, attribute.name, columns);
        }
      }
      else if (c == 'b')
      {
        add_column(las, "Buffer", columns, true);
      }
      else
      {
        add_column(las, map_attribute(std::string(1, c)), columns);
      }
    }

    while (las->read_point())
    {
      bool buffer = las->point.get_buffered();
      if (drop_buffer && buffer)
        continue;

      for (auto& column : columns)
      {
        double value = column.is_buffer ? static_cast<double>(buffer) : column.accessor(&las->point);
        column.values.push_back(value);
      }
    }

    py::dict data;
    for (const auto& column : columns)
      data[py::str(column.name)] = to_numpy_array(column.values);

    py::tuple call_args = make_python_call_args(data, callback.args);
    py::object result = py::reinterpret_steal<py::object>(PyObject_CallObject(callback.callable.ptr(), call_args.ptr()));
    if (!result)
      throw py::error_already_set();

    if (!modify || result.is_none() || !py::isinstance<py::dict>(result))
    {
      las->update_header();
      return true;
    }

    py::dict output = result.cast<py::dict>();
    struct OutputColumn
    {
      std::string name;
      AttributeAccessor accessor;
      py::array_t<double, py::array::c_style | py::array::forcecast> values;
      bool is_buffer = false;
      bool is_withheld = false;
    };

    std::vector<OutputColumn> output_columns;
    for (auto item : output)
    {
      std::string name = py_object_to_string(item.first);

      if (name == "Buffer")
      {
        OutputColumn column;
        column.name = name;
        column.is_buffer = true;
        column.values = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(item.second);
        if (!column.values)
          throw std::runtime_error("callback column '" + name + "' cannot be converted to a numeric array");
        output_columns.push_back(column);
      }
      else if (name == "Withheld")
      {
        OutputColumn column;
        column.name = name;
        column.is_withheld = true;
        column.values = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(item.second);
        if (!column.values)
          throw std::runtime_error("callback column '" + name + "' cannot be converted to a numeric array");
        output_columns.push_back(column);
      }
      else
      {
        if (!las->header->schema.has_attribute(name))
          throw std::runtime_error("non supported column '" + name + "'");

        OutputColumn column;
        column.name = name;
        column.accessor = AttributeAccessor(name);
        column.values = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(item.second);
        if (!column.values)
          throw std::runtime_error("callback column '" + name + "' cannot be converted to a numeric array");
        output_columns.push_back(column);
      }
    }

    py::ssize_t npoints = columns.empty() ? static_cast<py::ssize_t>(las->npoints) : static_cast<py::ssize_t>(columns[0].values.size());
    for (const auto& column : output_columns)
    {
      if (column.values.ndim() != 1)
        throw std::runtime_error("callback column '" + column.name + "' must be a one-dimensional array");
      if (column.values.shape(0) != npoints)
      {
        std::ostringstream oss;
        oss << "callback column '" << column.name << "' has length " << column.values.shape(0)
            << " but expected " << npoints;
        throw std::runtime_error(oss.str());
      }
    }

    py::ssize_t i = 0;
    while (las->read_point())
    {
      bool buffer = las->point.get_buffered();
      if (drop_buffer && buffer)
        continue;

      for (auto& column : output_columns)
      {
        auto values = column.values.unchecked<1>();
        double value = values(i);
        if (column.is_buffer)
          las->point.set_buffered(value != 0.0);
        else if (column.is_withheld)
          las->point.set_deleted(value != 0.0);
        else
          column.accessor(&las->point, value);
      }
      i++;
    }

    las->update_header();
    return true;
  }
  catch (const py::error_already_set& e)
  {
    last_error = e.what();
    return false;
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }
}

#endif
