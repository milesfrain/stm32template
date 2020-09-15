/*
 * Some basic helper functions
 */

#pragma once
#include "string.h"

// Just putting this here, so we don't need to include all of <algorithm>
template<class T>
const T& min(const T& a, const T& b)
{
  return a < b ? a : b;
}

// Helper function for concatenating prefix to names
// Note that the existing strncat is not exactly what we need here.
inline char* concat(char* dst, const char* s1, const char* s2, size_t len)
{
  size_t lenS1 = min(strlen(s1), len);
  strncpy(dst, s1, len);
  strncpy(dst + lenS1, s2, len - lenS1);
  return dst;
}
