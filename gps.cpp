#include "gps.h"

bool isValidUkraineRange(float lat, float lon)
{
  if (lat < 44.0f || lat > 53.0f)
    return false;
  if (lon < 22.0f || lon > 42.0f)
    return false;
  return true;
}