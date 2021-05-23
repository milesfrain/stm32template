/*
 * Clusters each UART instance with its associated DMA instances.
 */

#pragma once

#include "instance_enums.h"

class UartInfo
{
public:
  UartInfo(Uart uartNum, DmaInstance dmaTxInstNum, DmaStream dmaTxStream, DmaInstance dmaRxInstNum, DmaStream dmaRxStream)
    : uartNum{ uartNum }
    , uartReg{ getUartReg(uartNum) }
    , dmaTxInstNum{ dmaTxInstNum }
    , dmaTxReg{ getDmaReg(dmaTxInstNum) }
    , dmaTxStream{ dmaTxStream }
    , dmaRxInstNum{ dmaRxInstNum }
    , dmaRxReg{ getDmaReg(dmaRxInstNum) }
    , dmaRxStream{ dmaRxStream }
  {}
  Uart uartNum;
  USART_TypeDef* uartReg;
  DmaInstance dmaTxInstNum;
  DMA_TypeDef* dmaTxReg;
  DmaStream dmaTxStream;
  DmaInstance dmaRxInstNum;
  DMA_TypeDef* dmaRxReg;
  DmaStream dmaRxStream;
};

// Other chips will likely have different capabilities and dma mappings,
// so these ifdefs will force the developer to double-check things when
// changing hardware.
#if defined(STM32F413xx)
const UartInfo uartInfo4(uart4, dma1, dmaStream4, dma1, dmaStream2);
const UartInfo uartInfo5(uart5, dma1, dmaStream7, dma1, dmaStream0); // uart 5 rx and uart 8 tx conflict on dma1 stream 0
const UartInfo uartInfo7(uart7, dma1, dmaStream1, dma1, dmaStream3);
const UartInfo uartInfo8(uart8, dma1, dmaStream0, dma1, dmaStream6); // uart 5 rx and uart 8 tx conflict on dma1 stream 0
const UartInfo uartInfo9(uart9, dma2, dmaStream0, dma2, dmaStream7);
// Note - add more as needed
#elif defined(STM32F423xx)
// definitions for other chips
#endif
