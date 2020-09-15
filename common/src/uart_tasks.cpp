/*
 * Wrapper for a UART instance.
 * Provides read() and write() interfaces.
 * Handles DMA coordination.
 * Statically allocates two tasks and two stream buffers.
 * Todo: create checklist for setting-up a compatible HW config.
 */

#include "uart_tasks.h"
#include "basic.h"
#include "dma_reg.h"
#include "logging.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_usart.h"
#include "string.h"

// ------- Forward declarations --------

void txFuncWrapper(UartTasks* p);
void rxFuncWrapper(UartTasks* p);

// ------- API ---------

// Constructor
UartTasks::UartTasks(const char* name, const UartInfo ui, UBaseType_t priority)
  : txTask{ concat(txName, name, "_tx", sizeof(rxName)), txFuncWrapper, this, priority }
  , rxTask{ concat(rxName, name, "_rx", sizeof(rxName)), rxFuncWrapper, this, priority }
  , ui{ ui }
{}

// Blocking read and write

size_t UartTasks::read(uint8_t* buf, size_t len, TickType_t ticks)
{
  return xStreamBufferReceive(rxMsgBuf.handle, buf, len, ticks);
}

size_t UartTasks::write(const uint8_t* buf, size_t len, TickType_t ticks)
{
  return xStreamBufferSend(txMsgBuf.handle, buf, len, ticks);
}

// ---------- Internal details -------------

constexpr uint32_t rxRolloverFlag = 1 << 31;

// C-style wrapper to enable launching C++ class member functions in rtos
// as task entry points.
void txFuncWrapper(UartTasks* p)
{
  p->txFunc();
}
void rxFuncWrapper(UartTasks* p)
{
  p->rxFunc();
}

// Callback wrappers for ISRs
// Note - these can be modified to remove void pointers and casting,
// but requires some additional templatizing.
void dmaTxCallbackWrapper(void* p)
{
  ((UartTasks*)p)->dmaTxCallback();
}
void dmaRxCallbackWrapper(void* p)
{
  ((UartTasks*)p)->dmaRxCallback();
}
void uartCallbackWrapper(void* p)
{
  ((UartTasks*)p)->uartCallback();
}

/*
 * Regarding notifications:
 * Note that there are two separate task functions here,
 * so there's no issue with ulTaskNotifyTake clearing the bits
 * upon read. It won't interfere with the other task.
 */

void UartTasks::dmaTxCallback()
{
  // Handle transfer-complete
  if (dmaFlagCheckAndClear(ui.dmaTxChReg, ui.dmaTxStream, DmaFlag::TC)) {
    // Generate signal that we're ready for next message.
    // The particular bits set in this case is insignificant.
    isrTaskNotifyBits(txTask.handle, 1);

    // We can rely solely on DMA transfer-complete (no need for UART transfer-complete).
    // Next DMA transfer automatically waits until UART is ready to receive more data.
    // Notes on DMA TC vs uart TC:
    // https://community.st.com/s/question/0D50X00009XkdlQSAR/long-wait-for-uartflagtc-after-dma-transfer
  } else {
    // unexpected interrupt triggered - possibly an error flag (FE, DME, TE) (although those interrupts probably not enabled)
    // todo - capture error or warning - needs an interrupt-safe way to do this
  }
}

void UartTasks::dmaRxCallback()
{
  // half transfer
  if (dmaFlagCheckAndClear(ui.dmaRxChReg, ui.dmaRxStream, DmaFlag::HT)) {
    isrTaskNotifyIncrement(rxTask.handle);
  }

  // transfer complete
  if (dmaFlagCheckAndClear(ui.dmaRxChReg, ui.dmaRxStream, DmaFlag::TC)) {
    isrTaskNotifyBits(rxTask.handle, rxRolloverFlag);
  }
}

void UartTasks::uartCallback()
{
  // Check for idle line - a gap in the data that
  // indicates likely end of packet
  if (LL_USART_IsActiveFlag_IDLE(ui.uartReg)) {
    LL_USART_ClearFlag_IDLE(ui.uartReg);
    // Notify rx task to wake-up and process data
    isrTaskNotifyIncrement(rxTask.handle);
  }
}

void UartTasks::txFunc()
{
  // Additional DMA setup

  // Register ISR callback
  registerDmaCallback(ui.dmaTxChNum, ui.dmaTxStream, dmaTxCallbackWrapper, this);

  // Memory address to copy from - this is always the same buffer for us
  LL_DMA_SetMemoryAddress(ui.dmaTxChReg, ui.dmaTxStream, (uint32_t)txDmaBuf);

  // Peripheral address to write to
  LL_DMA_SetPeriphAddress(ui.dmaTxChReg, ui.dmaTxStream, (uint32_t)&ui.uartReg->DR);

  // Enable transfer-complete interrupt
  LL_DMA_EnableIT_TC(ui.dmaTxChReg, ui.dmaTxStream);
  // Just letting DMA manage flow control, so keeping uart TCIE disabled

  // Enable DMA TX in UART
  LL_USART_EnableDMAReq_TX(ui.uartReg);

  while (1) {
    // Wait until new data to send is available on buffer
    size_t len = xStreamBufferReceive(txMsgBuf.handle, txDmaBuf, sizeof(txDmaBuf), portMAX_DELAY);

    // Todo, better error handling

    // Check if transfer is still in-progress
    if (LL_DMA_IsEnabledStream(ui.dmaTxChReg, ui.dmaTxStream)) {
      // error, although we shouldn't ever be in this situation
      error("DMA is not enabled");
    }

    // Configure size of this next transfer
    LL_DMA_SetDataLength(ui.dmaTxChReg, ui.dmaTxStream, len);

    // Start transfer
    LL_DMA_EnableStream(ui.dmaTxChReg, ui.dmaTxStream);

    // Wait until DMA transfer is complete.
    // Will be signaled from DMA ISR.
    // Clear notification value upon receipt and block forever.
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Alternative option is to wait until UART transfer is complete,
    // but that's an unnecessary extra delay (~170 uS @ 115200 baud),
    // and leads to other complexity / potential bugs.

    /*
    LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_8);
    LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_8);
    */
  }
}

/*
 * Finishes DMA RX setup, then listens for whenever there's new data to
 * copy from the DMA memory destination to the rtos stream buffer.
 * Note that DMA RX is setup for circular mode, so it is this function's
 * job to keep up with copying to avoid being lapped by DMA overwrites.
 * ISR-based notifications to this function are:
 * - DMA half-transfer and transfer-complete: HT TC
 * - UART idle line
 */
void UartTasks::rxFunc()
{
  // Additional DMA and UART setup

  // Register ISR callbacks
  registerDmaCallback(ui.dmaRxChNum, ui.dmaRxStream, dmaRxCallbackWrapper, this);
  registerUartCallback(ui.uartNum, uartCallbackWrapper, this);

  // point to peripheral address to read from
  LL_DMA_SetPeriphAddress(ui.dmaRxChReg, ui.dmaRxStream, (uint32_t)&ui.uartReg->DR);

  // point to memory address to write to
  LL_DMA_SetMemoryAddress(ui.dmaRxChReg, ui.dmaRxStream, (uint32_t)rxDmaBuf);

  // setup size of memory destination
  LL_DMA_SetDataLength(ui.dmaRxChReg, ui.dmaRxStream, sizeof(rxDmaBuf));

  // enable transfer-complete interrupt
  LL_DMA_EnableIT_TC(ui.dmaRxChReg, ui.dmaRxStream);

  // enable half-transfer interrupt
  LL_DMA_EnableIT_HT(ui.dmaRxChReg, ui.dmaRxStream);

  // Enable idle-line interrupt
  LL_USART_EnableIT_IDLE(ui.uartReg);

  // Enable DMA RX in UART
  LL_USART_EnableDMAReq_RX(ui.uartReg);

  // Enable DMA stream
  LL_DMA_EnableStream(ui.dmaRxChReg, ui.dmaRxStream);

  // Keeps track of our stopping point in the buffer from last time.
  uint32_t oldIdx = 0;

  // Tracks the difference in the number of times the DMA buffer rolls over.
  // This lets us check if we're not grabbing data from DMA fast enough.
  // We expect to see this value alternate between -1 and 0.
  // Positive values indicate data loss.
  // todo - this might be better to as object member for easier logging.
  // - Moving all locals to private members will reduce stack usage.
  // - Unfortunately, private members expand scope. There's no way to have
  //   per-instance static variables with function scope.
  int32_t lostBuffers = 0;

  while (1) {
    // Wait until it's time to read from DMA

    // Clear notification value upon receipt and block forever
    uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Figure out where we are in the buffer
    uint32_t newIdx = sizeof(rxDmaBuf) - LL_DMA_GetDataLength(ui.dmaRxChReg, ui.dmaRxStream);

    if (newIdx > oldIdx) {
      // New data

      // Wait forever to write
      xStreamBufferSend(rxMsgBuf.handle, rxDmaBuf + oldIdx, newIdx - oldIdx, portMAX_DELAY);
    } else if (newIdx < oldIdx) {
      // New data with rollover - needs two separate writes

      // Write everything until the end of the buffer
      xStreamBufferSend(rxMsgBuf.handle, rxDmaBuf + oldIdx, sizeof(rxDmaBuf) - oldIdx, portMAX_DELAY);

      // If we respond quickly to the TC interrupt, then there will be no
      // rollover data written, so this following packet will be of length 0.
      // FreeRTOS asserts when attempting to write a packet of length 0,
      // so we'll skip this unnecessary (and problematic) write.
      if (newIdx > 0) // check if there's any rollover data to write.
      {
        // Write the next chunk at the beginning of the buffer
        xStreamBufferSend(rxMsgBuf.handle, rxDmaBuf, newIdx, portMAX_DELAY);
      }

      // Note that we handled a rollover
      lostBuffers--;
    } else {
      // This means we got a notification, even though nothing was written.
      // This can happen if end of latest packet stops exactly at the end or
      // midpoint of the buffer, since will get a TC or HT notification, and then
      // and IDLE line notification.
      // So there's nothing we need to do here.
    }

    oldIdx = newIdx;

    // Check if we got a special notification value which indicates that DMA got to the end of the write
    // buffer and rolled over.
    if (notifyValue >= rxRolloverFlag) {
      lostBuffers++;
    }
  }
}
