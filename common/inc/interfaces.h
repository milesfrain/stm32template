/*
 * Simplified interfaces for working with rtos wrapper classes
 */

#pragma once

#include "FreeRTOS.h"

class Writable
{
public:
  virtual size_t write(const uint8_t*, size_t, TickType_t = portMAX_DELAY) = 0;
};

class Readable
{
public:
  virtual size_t read(uint8_t*, size_t, TickType_t = portMAX_DELAY) = 0;
};
