/*
 * Some basic helper functions
 */

#pragma once
#include "string.h"

// Just putting this here, so we don't need to include all of <algorithm>
template<class T>
inline constexpr const T& min(const T& a, const T& b)
{
  return a < b ? a : b;
}

template<class T>
inline constexpr const T& max(const T& a, const T& b)
{
  return a > b ? a : b;
}

// Rounds up, rather than performing truncating integer division.
// Assumes positive integers.
// roundUpDiv(12, 7) == 2
template<class T>
inline constexpr T roundUpDiv(const T& n, const T& d)
{
  return (n + d - 1) / d;
}

// Helper function for concatenating prefix to names
// Note that the existing strncat is not exactly what we need here.
inline char* concat(char* dst, const char* s1, const char* s2, size_t len)
{
  size_t lenS1 = min(strlen(s1), len);
  strncpy(dst, s1, len);
  strncpy(dst + lenS1, s1, len - lenS1);
  return dst;
}

// Could combine this macro with enum definition,
// but might lose some IDE autocompletion
#define ENUM_STRING(enumType, enumName) \
  case enumType::enumName: return #enumName;
