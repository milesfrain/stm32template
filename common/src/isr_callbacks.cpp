/*
 * Wrapper to allow executing ISR callbacks belonging to C++ objects.
 */

#include "isr_callbacks.h"

// -------- Generic helpers ---------

struct CallbackData
{
  IsrCallbackFunc f;
  void* arg;
};

void registerCallback(CallbackData& cb, IsrCallbackFunc f, void* arg)
{
  // Check if callback is already registered
  if (cb.f) {
    // Callback is registered.
    // todo - print ITM message (need a non-task version)
    // Trigger a major failure so this issue is obvious.
    while (1)
      ;
  } else {
    // Callback is not registered.
    // Register it.
    cb.f = f;
    cb.arg = arg;
  }
}

void handleInterrupt(CallbackData& cb)
{
  // Check if callback is registered
  if (cb.f) {
    // Callback is registered.
    // Call it.
    cb.f(cb.arg);
  } else {
    // Callback is not registered.
    // todo - print ITM message (need a from-isr version)
    // Trigger a major failure so this issue is obvious.
    while (1)
      ;
  }
}

// -------- DMA specific ---------

CallbackData dmaCallbacks[numDmaCh][numDmaStream];

void registerDmaCallback(DmaCh ch, DmaStream stream, IsrCallbackFunc f, void* arg)
{
  CallbackData& cb = dmaCallbacks[ch][stream];
  registerCallback(cb, f, arg);
}

extern "C" void handleDmaInterrupt(enum DmaCh ch, enum DmaStream stream)
{
  CallbackData& cb = dmaCallbacks[ch][stream];
  handleInterrupt(cb);
}

// -------- UART specific ---------

CallbackData uartCallbacks[numUart];

void registerUartCallback(Uart uart, IsrCallbackFunc f, void* arg)
{
  CallbackData& cb = uartCallbacks[uart];
  registerCallback(cb, f, arg);
}

extern "C" void handleUartInterrupt(Uart uart)
{
  CallbackData& cb = uartCallbacks[uart];
  handleInterrupt(cb);
}
