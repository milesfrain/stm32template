/*
 * Wrapper for a USB instance.
 * Provides read() and write() interfaces.
 * Statically allocates two tasks and two stream buffers.
 */
#pragma once

#include "interfaces.h"
#include "task_utilities.h"
#include "usbd_cdc.h"
#include "usbd_def.h"

/*
 * Note that it's possible to improve efficiency by creating
 * a custom StreamBuffer that's more tightly coupled with
 * DMA and could share memory directly.
 * We'll cross that optimization bridge later if necessary.
 */
class UsbTask
  : public Writable
  , public Readable
{
public:
  UsbTask(TaskUtilitiesArg& utilArg, UBaseType_t priority = osPriorityNormal);

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

  // Callbacks
  int8_t initCb(void);
  int8_t deInitCb(void);
  int8_t controlCb(uint8_t cmd, uint8_t* pbuf, uint16_t length);
  int8_t receiveCb(uint8_t* buf, uint32_t* len);
  int8_t transmitCpltCb(uint8_t* buf, uint32_t* len, uint8_t epnum);

private:
  // The tasks that manage reading and writing to the uart device
  StaticTask<UsbTask> txTask;

  // A struct of callbacks that are reached via usb ISRs
  USBD_CDC_ItfTypeDef callbacks;

  size_t txLen = 0;

  // Size of tx and rx buffers.
  // Should be at least 2x size of largest packet.
  static constexpr size_t TSize = 2048;

  // Buffer for read/write interface.
  StaticMessageBuffer<TSize> txMsgBuf;
  StaticStreamBuffer<TSize> rxMsgBuf;

  // Buffers for the USB driver
  uint8_t txBuf[TSize];
  uint8_t rxBuf[TSize];

  USBD_HandleTypeDef usbDeviceHandle;

  // These are optional mutexes to support multiple readers / writers.
  // Note that these are currently not automatically being used for read and write.
  // A few options:
  // - (current) Require these to be locked explicitly in multi-reader/multi-writer situations.
  // - Add locking overhead for all reads and writes. Unnecessary for single reader/writer cases though.
  // - Add another default parameter to read/write for whether to lock.
  // - Create alternate locked functions, e.g. multiRead or protectedRead
  StaticMutex txMutex;
  StaticMutex rxMutex;

  // Common utilities
  TaskUtilities util;

  // Counters for ITM value logging
  uint32_t rxReceivedTotal = 0;
  size_t txPendingTotal = 0;
  uint32_t txTransmittedTotal = 0;
};
