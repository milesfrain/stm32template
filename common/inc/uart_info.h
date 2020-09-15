/*
 * Clusters each UART instance with its associated DMA instances.
 */

#pragma once

#include "instance_enums.h"

class UartInfo
{
public:
  UartInfo(Uart uartNum, DmaCh dmaTxChNum, DmaStream dmaTxStream, DmaCh dmaRxChNum, DmaStream dmaRxStream)
    : uartNum{ uartNum }
    , uartReg{ getUartReg(uartNum) }
    , dmaTxChNum{ dmaTxChNum }
    , dmaTxChReg{ getDmaChReg(dmaTxChNum) }
    , dmaTxStream{ dmaTxStream }
    , dmaRxChNum{ dmaRxChNum }
    , dmaRxChReg{ getDmaChReg(dmaRxChNum) }
    , dmaRxStream{ dmaRxStream }
  {}
  Uart uartNum;
  USART_TypeDef* uartReg;
  DmaCh dmaTxChNum;
  DMA_TypeDef* dmaTxChReg;
  DmaStream dmaTxStream;
  DmaCh dmaRxChNum;
  DMA_TypeDef* dmaRxChReg;
  DmaStream dmaRxStream;
};

// Other chips will likely have different capabilities and dma mappings,
// so these ifdefs will force the developer to double-check things when
// changing hardware.
#if defined(STM32F413xx)
const UartInfo uartInfo4(uart4, dmaCh1, dmaStream4, dmaCh1, dmaStream2);
const UartInfo uartInfo5(uart5, dmaCh1, dmaStream7, dmaCh1, dmaStream0); // uart 5 rx and uart 8 tx conflict on dma1 stream 0
const UartInfo uartInfo7(uart7, dmaCh1, dmaStream1, dmaCh1, dmaStream3);
const UartInfo uartInfo8(uart8, dmaCh1, dmaStream0, dmaCh1, dmaStream6); // uart 5 rx and uart 8 tx conflict on dma1 stream 0
const UartInfo uartInfo9(uart9, dmaCh2, dmaStream0, dmaCh2, dmaStream7);
// Note - add more as needed
#elif defined(STM32F423xx)
// definitions for other chips
#endif
