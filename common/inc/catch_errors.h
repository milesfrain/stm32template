/*
 * Functions for catching errors.
 * Note that these are not inlined in order to conserve HW breakpoints.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

  // Checks if value is true.
  // Crashes if value is false.
  void assert(bool value);

  // For trapping critical errors
  void critical();

  // For trapping nonCritical errors / warnings
  void nonCritical();

  // This is a timeout to indicate non-optimal behavior,
  // such as waiting to write to a full buffer.
  void timeout();

  // This is a timeout to indicate expected blocking,
  // such as waiting to read from an empty buffer.
  void benignTimeout();

#ifdef __cplusplus
}
#endif
