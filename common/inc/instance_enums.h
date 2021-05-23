/*
 * Peripheral instances as enums, and functions to convert these back to
 * register typedefs.
 * These are useful for building peripheral lookup arrays, such as callbacks
 * for simplified and generic ISR handling.
 */

#pragma once

#include "stm32f4xx.h"

#ifdef __cplusplus
extern "C"
{
#endif

  // Could reuse existing DMAx defines,
  // but safer, easier, and faster to use an enum

  // Using C-friendly enums (not scoped C++ enums) for use in C isr code.
  enum DmaInstance
  {
    dma1, // one-based index, but letting be zero for array access
    dma2,
    numDmaInstance
  };

  enum DmaStream
  {
    dmaStream0,
    dmaStream1,
    dmaStream2,
    dmaStream3,
    dmaStream4,
    dmaStream5,
    dmaStream6,
    dmaStream7,
    numDmaStream
  };

  // naming everything uart, even though some are usart
  enum Uart
  {
    uart1, // one-based index, but letting be zero for array access
    uart2,
    uart3,
    uart4,
    uart5,
    uart6,
    uart7,
    uart8,
    uart9,
    uart10,
    numUart
  };

#ifdef __cplusplus
} // extern C
#endif

// This section is only compatible with C++

#ifdef __cplusplus
// Helper function to convert DMA instance enum to register def
constexpr DMA_TypeDef* getDmaReg(enum DmaInstance inst)
{
  switch (inst) {
    case dma1: return DMA1;
    case dma2: return DMA2;
    case numDmaInstance: return DMA2; // to suppress compiler warning
  }
  return DMA2; // to suppress compiler warning
}

// Helper function to convert UART/USART instance enum to register def
constexpr USART_TypeDef* getUartReg(enum Uart uart)
{
  switch (uart) {
    case uart1: return USART1;
    case uart2: return USART2;
    case uart3: return USART3;
    case uart4: return UART4;
    case uart5: return UART5;
    case uart6: return USART6;
    case uart7: return UART7;
    case uart8: return UART8;
    case uart9: return UART9;
    case uart10: return UART10;
    case numUart: return UART10; // to suppress compiler warning
  }
  return UART10; // to suppress compiler warning
}
#endif
