/*
 * Wrapper for a UART instance.
 * Provides read() and write() interfaces.
 * Handles DMA coordination.
 * Statically allocates two tasks and two stream buffers.
 * Todo: create checklist for setting-up a compatible HW config.
 */
#pragma once

#include "interfaces.h"
#include "isr_callbacks.h"
#include "static_rtos.h"
#include "uart_info.h"

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
  UartTasks(const char*, // base name of both threads - not sure this can be passed through statically
            const UartInfo,
            UBaseType_t = osPriorityNormal // task priority
  );

  // Blocking reads and writes. Simple wrapper on xStreamBuffer API.
  // Reads are from uart rx.
  // Writes are to uart tx.
  size_t read(uint8_t*, size_t, TickType_t = portMAX_DELAY);
  size_t write(const uint8_t*, size_t, TickType_t = portMAX_DELAY);

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
  StaticStreamBuffer<TSize> txMsgBuf;
  StaticStreamBuffer<TSize> rxMsgBuf;

  // The "memory" buffers for DMA
  uint8_t txDmaBuf[TSize];
  uint8_t rxDmaBuf[TSize];

  // These are optional, to support multiple readers / writers.
  StaticMutex txMutex;
  StaticMutex rxMutex;

  // UART and DMA peripheral info
  const UartInfo ui;
};
