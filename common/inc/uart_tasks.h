/*

Wrapper for a UART instance.
Provides read() and write() interfaces.
Handles DMA coordination.
Statically allocates two tasks and two stream buffers.

HW config checklist:

In .ioc config:
 - Pinout & Configuration
    - Connectivity > UARTn
      - DMA Settings
        - Enable TX
        - Enable RX (set mode to circular)
      - NVIC settings
        - Enable UARTn global interrupt
    - System Core -> NVIC
      - NVIC tab
        - For all UART and accompanying DMA streams
          - Set preemption priority to 5 (lowest number / highest priority compatible with freeRTOS)
          - Check "Uses FreeRTOS functions" to enforce the above
      - Code generation tab
        - For all UART and accompanying DMA streams
          - Check "Generate IRQ handler"
          - Uncheck "Call HAL handler"
 - Project Manager > Advanced Settings > Driver Selector
    - For UART and DMA, select LL

In stm32f4xx_it.h:
  - Include "isr_callbacks.h"
  - Add "USER CODE" to UART and DMA functions according to the following patten:
      void UART7_IRQHandler(void)
      {
        handleUartInterrupt(uart7);
      }
      void DMA1_Stream2_IRQHandler(void)
      {
        handleDmaInterrupt(dma1, dmaStream2);
      }
  - Configure DMA streams in uart_info.h according to the table in
    DMAn "requst mapping". Table 30 in RM0430 Rev 8
*/

#pragma once

#include "interfaces.h"
#include "isr_callbacks.h"
#include "static_rtos.h"
#include "stm32f4xx_ll_gpio.h"
#include "task_utilities.h"
#include "uart_info.h"

/*
 * Pass one of these HalfDuplexCallbacks objects to
 * UartTasks constructor to enable half-duplex mode.
 * Sets pin high for transmit, and low for receive.
 */
struct HalfDuplexCallbacks
{
  HalfDuplexCallbacks(GPIO_TypeDef* GPIOx, uint32_t PinMask)
    : GPIOx{ GPIOx }
    , PinMask{ PinMask } {};

  void txMode() { LL_GPIO_SetOutputPin(GPIOx, PinMask); }
  void rxMode() { LL_GPIO_ResetOutputPin(GPIOx, PinMask); }

private:
  GPIO_TypeDef* GPIOx;
  uint32_t PinMask;
};

// Could alternatively make zero assumptions about
// what these callbacks do (e.g. toggling pins with
// active high). But then construction is more involved.
// For example:
/*
struct HalfDuplexCallbacks
{
  void (*txMode)(void);
  void (*rxMode)(void);
};

// Example of instantiation
HalfDuplexCallbacks uart7halfDuplexCallbacks
{
  .txMode = [](){ LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_15);},
  .rxMode = [](){ LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_15);}
};
*/

/*
 * Note that it's possible to improve efficiency by creating
 * a custom StreamBuffer that's more tightly coupled with
 * DMA and could share memory directly.
 * We'll cross that optimization bridge later if necessary.
 */
class UartTasks
  : public Writable
  , public Readable
{
public:
  UartTasks(const char* name, // base name of both threads - not sure this can be passed through statically
            const UartInfo ui,
            TaskUtilitiesArg& utilArg,                       // common utilities
            HalfDuplexCallbacks* halfDuplexCallbacks = NULL, // Full duplex by default
            UBaseType_t txPriority = osPriorityAboveNormal,  // tx task priority
            UBaseType_t rxPriority = osPriorityNormal        // rx task priority
  );

  // Blocking reads and writes. Simple wrapper on xStreamBuffer API.
  // Reads are from uart rx.
  // Writes are to uart tx.
  size_t read(void* buf, size_t len, TickType_t ticks);
  size_t write(const void* buf, size_t len, TickType_t ticks);

  // Hold off on the protected versions
  // These are just for situation with multiple readers / writers
  // protectedRead
  // protectedWrite
  // Not supporting read / write from ISR

  // The following functions need to be public for C-wrapper compatibility

  // Looping task functions
  void txFunc();
  void rxFunc();

  // Callbacks
  void dmaRxCallback();
  void dmaTxCallback();
  void uartCallback();

private:
  // The tasks that manage reading and writing to the uart device
  StaticTask<UartTasks> txTask;
  StaticTask<UartTasks> rxTask;

  // Task names. Necessary to reserve space here for concatenating prefix.
  char txName[configMAX_TASK_NAME_LEN];
  char rxName[configMAX_TASK_NAME_LEN];

  // Size of tx and rx buffers.
  // Should be at least 2x size of largest packet.
  // Had issues with template strategy, so just hardcoding size here instead.
  static constexpr size_t TSize = 1024;

  // Buffer for read/write interface
  StaticMessageBuffer<TSize> txMsgBuf;
  StaticStreamBuffer<TSize> rxStreamBuf;

  // The "memory" buffers for DMA
  uint8_t txDmaBuf[TSize];
  uint8_t rxDmaBuf[TSize];

  // These are optional, to support multiple readers / writers.
  StaticMutex txMutex;
  StaticMutex rxMutex;

  // UART and DMA peripheral info
  const UartInfo ui;

  // Common utilities for each task
  TaskUtilities txUtil;
  TaskUtilities rxUtil;

  // Contains callbacks for changing tx/rx mode for half-duplex operation
  HalfDuplexCallbacks* halfDuplexCallbacks;
};
