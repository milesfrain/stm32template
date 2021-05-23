/*
 * Simplified interfaces for working with rtos wrapper classes
 */

#pragma once

#include "FreeRTOS.h"

// Objects with this interface must implement `write`
class Writable
{
public:
  // Attempt to write n bytes of data from buffer.
  // Returns how many bytes were actually written, 0 if timeout.
  virtual size_t write(const void* buf, size_t len, TickType_t ticks) = 0;
};

// Objects with this interface must implement `read`
class Readable
{
public:
  // Attempt to read n bytes of data into buffer.
  // Returns how many bytes were actually read, 0 if timeout.
  virtual size_t read(void* buf, size_t len, TickType_t ticks) = 0;
};
