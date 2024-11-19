#include "lasfilter.hpp"
#include <functional>
#include <set>

class LASRcriterion : public LAScriterion
{
public:
  LASRcriterion() { accessor = nullptr; attribute_index = -1; attribute_name = 0; }
  ~LASRcriterion() { if (attribute_name) free(attribute_name); }


  LASRcriterion(const char* attribute_name) : LASRcriterion()
  {
    this->attribute_name = strdup(attribute_name);

    // Assign the accessor function based on attribute_name
    if (strcmp(attribute_name, "x") == 0)  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_x()); };
    else if (strcmp(attribute_name, "y") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_y()); };
    else if (strcmp(attribute_name, "z") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_z()); };
    else if (strcmp(attribute_name, "intensity") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_intensity()); };
    else if (strcmp(attribute_name, "return") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_return_number()); };
    else if (strcmp(attribute_name, "class") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_classification()); };
    else if (strcmp(attribute_name, "psid") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_point_source_ID()); };
    else if (strcmp(attribute_name, "gpstime") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_gps_time()); };
    else if (strcmp(attribute_name, "angle") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_scan_angle()); };
    else if (strcmp(attribute_name, "userdata") == 0) accessor = [](const LASpoint* point) { return static_cast<double>(point->get_user_data()); };
    else
      accessor = [this](const LASpoint* point)
      {
        if (this->attribute_index == -1)
        {
          this->attribute_index = point->attributer->get_attribute_index(this->attribute_name);
          warning("No attribute '%s' for this point cloud. Returning 0 for this attribute may affect the filter\n", this->attribute_name);
          if (this->attribute_index == -1)  this->attribute_index = -2;
        }

        if (this->attribute_index == -2)
        {
          return 0.0;
        }

        return static_cast<double>(point->get_attribute_as_float(this->attribute_index));
      };
  };

  inline U32 get_decompress_selective() const { return LASZIP_DECOMPRESS_SELECTIVE_ALL; };

protected:
  std::function<double(const LASpoint*)> accessor;
  char* attribute_name;
  I32 attribute_index;
};



class LASRcriterionKeepBelow : public LASRcriterion
{
public:
  LASRcriterionKeepBelow(const char* attribute_name, double threshold) : LASRcriterion(attribute_name) { this->threshold = threshold; }
  inline const CHAR* name() const { return "keep_below"; }
  inline I32 get_command(char* string) const  {  return snprintf(string, 256, "-%s %.2f ", name(), threshold); }
  inline BOOL filter(const LASpoint* point) { return accessor(point) >= threshold; }
private:
  double threshold;
};

class LASRcriterionKeepBelowEqual : public LASRcriterion
{
public:
  LASRcriterionKeepBelowEqual(const char* attribute_name, double threshold) : LASRcriterion(attribute_name) { this->threshold = threshold; }
  inline const CHAR* name() const { return "keep_beloweq"; }
  inline I32 get_command(char* string) const  {  return snprintf(string, 256, "-%s %.2f ", name(), threshold); }
  inline BOOL filter(const LASpoint* point) { return accessor(point) > threshold; }
private:
  double threshold;
};

class LASRcriterionKeepAbove : public LASRcriterion
{
public:
  LASRcriterionKeepAbove(const char* attribute_name, double threshold) : LASRcriterion(attribute_name) { this->threshold = threshold; }
  inline const CHAR* name() const { return "keep_above"; }
  inline I32 get_command(char* string) const  {  return snprintf(string, 256, "-%s %.2f ", name(), threshold); }
  inline BOOL filter(const LASpoint* point) { return accessor(point) <= threshold; }
private:
  double threshold;
};

class LASRcriterionKeepAboveEqual : public LASRcriterion
{
public:
  LASRcriterionKeepAboveEqual(const char* attribute_name, double threshold) : LASRcriterion(attribute_name) { this->threshold = threshold; }
  inline const CHAR* name() const { return "keep_aboveeg"; }
  inline I32 get_command(char* string) const  {  return snprintf(string, 256, "-%s %.2f ", name(), threshold); }
  inline BOOL filter(const LASpoint* point) { return accessor(point) < threshold; }
private:
  double threshold;
};

class LASRcriterionKeepBetween : public LASRcriterion
{
public:
  LASRcriterionKeepBetween(const char* attribute_name, double below, double above) : LASRcriterion(attribute_name)
  {
    if (below > above)
    {
      this->below = above;
      this->above = below;
    }
    else
    {
      this->below = below;
      this->above = above;
    }
  }
  inline const CHAR* name() const { return "keep_between"; }
  inline I32 get_command(char* string) const  { return snprintf(string, 256, "-%s %.2f %.2f ", name(), below, above); }
  inline BOOL filter(const LASpoint* point) { double v = accessor(point); return (v < below) || (v >= above);  }
private:
  double below;
  double above;
};

class LASRcriterionKeepEqual : public LASRcriterion
{
public:
  LASRcriterionKeepEqual(const char* attribute_name, double value) : LASRcriterion(attribute_name) { this->value = value; }
  inline const CHAR* name() const { return "keep_above"; }
  inline I32 get_command(char* string) const  {  return snprintf(string, 256, "-%s %.2f ", name(), value); }
  inline BOOL filter(const LASpoint* point) { return accessor(point) != value; }
private:
  double value;
};

class LASRcriterionKeepDifferent : public LASRcriterion
{
public:
  LASRcriterionKeepDifferent(const char* attribute_name, double value) : LASRcriterion(attribute_name) { this->value = value; }
  inline const CHAR* name() const { return "keep_different"; }
  inline I32 get_command(char* string) const  { return snprintf(string, 256, "-%s %.2f", name(), value); }
  inline BOOL filter(const LASpoint* point) { return accessor(point) == value; }
private:
  double value;
};

class LASRcriterionKeepIn : public LASRcriterion
{
public:
  LASRcriterionKeepIn(const char* attribute_name, double* values, int size) : LASRcriterion(attribute_name)
  {
    memcpy(this->values, values, 64 *sizeof(double));
    this->size = size;
  }
  inline const CHAR* name() const { return "keep_in"; }
  inline I32 get_command(char* string) const  { return snprintf(string, 256, "-%s", name()); }
  inline BOOL filter(const LASpoint* point)
  {
    double v = accessor(point);
    for (int i = 0 ; i < size ; i++)
    {
      if (values[i] == v)
        return false;
    }
    return true;
  }
private:
  double values[64];
  int size;
};

class LASRcriterionKeepOut : public LASRcriterion
{
public:
  LASRcriterionKeepOut(const char* attribute_name, double* values, int size) : LASRcriterion(attribute_name)
  {
    memcpy(this->values, values, 64 *sizeof(double));
    this->size = size;
  }
  inline const CHAR* name() const { return "keep_out"; }
  inline I32 get_command(char* string) const  { return snprintf(string, 256, "-%s", name()); }
  inline BOOL filter(const LASpoint* point)
  {
    double v = accessor(point);
    for (int i = 0 ; i < size ; i++)
    {
      if (values[i] == v)
        return true;
    }
    return false;
  }
private:
  double values[64];
  int size;
};

class LAScriterionDropDuplicates : public LAScriterion
{
  typedef std::array<I32,3> Triplet;

public:
  inline const CHAR* name() const { return "drop_duplicate"; };
  inline I32 get_command(CHAR* string) const { return snprintf(string, 256, "-%s ", name()); };
  inline U32 get_decompress_selective() const { return LASZIP_DECOMPRESS_SELECTIVE_CHANNEL_RETURNS_XY | LASZIP_DECOMPRESS_SELECTIVE_Z; };
  inline BOOL filter(const LASpoint* point)
  {
    Triplet key = {point->get_Z(), point->get_Y(), point->get_Z()};
    return !registry.insert(key).second;
  };
  void reset()
  {
    registry.clear();
  };
  LAScriterionDropDuplicates()
  {
  };
  ~LAScriterionDropDuplicates(){ reset(); };

private:
  struct ArrayCompare
  {
    bool operator()(const Triplet& a, const Triplet& b) const
    {
      if (a[0] < b[0]) return true;
      if (a[0] > b[0]) return false;

      if (a[1] < b[1]) return true;
      if (a[1] > b[1]) return false;

      return a[2] < b[2];
    }
  };

  std::set<Triplet, ArrayCompare> registry;
};
