/*
 * Wrapper to allow executing ISR callbacks belonging to C++ objects.
 */

#pragma once

#include "instance_enums.h"

// The C API contains the handle* functions
#ifdef __cplusplus
extern "C"
{
#endif

  // Handle an interrupt for a given DMA channel and stream
  // by calling the function that was registered in registerDmaCallback().
  void handleDmaInterrupt(enum DmaInstance, enum DmaStream);

  // Same as above, but for uart.
  void handleUartInterrupt(enum Uart);

#ifdef __cplusplus
} // extern C
#endif

// The below code is unnecessary for the C API, and so we're only including it for C++

#ifdef __cplusplus

// A function that lets us pass in a context.
// The context usually represents a C++ object,
// but it could be anything.
typedef void (*IsrCallbackFunc)(void*);

// Register a callback for a given DMA channel and stream.
// Then invoke this callback with handleDmaInterrupt. It will
// call the previously registered function as:
// f(arg);
void registerDmaCallback(enum DmaInstance, enum DmaStream, IsrCallbackFunc, void*);

// Same as above, but for uart.
void registerUartCallback(enum Uart, IsrCallbackFunc, void*);

#endif
