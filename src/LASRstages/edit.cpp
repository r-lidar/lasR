#include "edit.h"

LASRedit::LASRedit()
{
  first = true;
  value = 0;
}

bool LASRedit::process(Point*& p)
{
  if (first)
  {
    if (p->schema->find_attribute(attribute) == nullptr)
    {
      last_error = "No attribute '" + attribute + "' found";
      return false;
    }

    first = false;
  }

  if (!pointfilter.filter(p))
    accessor(p, value);

  return true;
}

bool LASRedit::process(PointCloud*& las)
{
  while(las->read_point())
  {
    Point* p = &las->point;
    process(p);
  }

  return true;
}

bool LASRedit::set_parameters(const nlohmann::json& stage)
{
  attribute = stage.value("attribute", "");
  if (attribute == "x" || attribute == "X" ||
      attribute == "y" || attribute == "Y" ||
      attribute == "z" || attribute == "Z")
  {
    last_error = "Editing point coordinates is not allowed";
    return false;
  }
  value = stage.value("value", 0);
  accessor = AttributeAccessor(attribute);
  return true;
}