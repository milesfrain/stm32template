/*
 * See header for notes
 */

#include "uart_tasks.h"
#include "basic.h"
#include "board_defs.h"
#include "catch_errors.h"
#include "dma_reg.h"
#include "itm_logging.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_usart.h"
#include "string.h"

// ------- Forward declarations --------

void txFuncWrapper(UartTasks* p);
void rxFuncWrapper(UartTasks* p);
void dmaTxCallbackWrapper(void* p);
void dmaRxCallbackWrapper(void* p);
void uartCallbackWrapper(void* p);

// ------- API ---------

// Constructor
UartTasks::UartTasks( //
  const char* name,
  const UartInfo ui,
  TaskUtilitiesArg& utilArg,
  HalfDuplexCallbacks* halfDuplexCallbacks,
  UBaseType_t txPriority,
  UBaseType_t rxPriority)
  : txTask{ concat(txName, name, "_tx", sizeof(rxName)), txFuncWrapper, this, txPriority }
  , rxTask{ concat(rxName, name, "_rx", sizeof(rxName)), rxFuncWrapper, this, rxPriority }
  , ui{ ui }
  , txUtil{ utilArg }
  , rxUtil{ utilArg }
  , halfDuplexCallbacks{ halfDuplexCallbacks }
{
  // Register ISR callbacks
  registerDmaCallback(ui.dmaRxInstNum, ui.dmaRxStream, dmaRxCallbackWrapper, this);
  registerDmaCallback(ui.dmaTxInstNum, ui.dmaTxStream, dmaTxCallbackWrapper, this);
  registerUartCallback(ui.uartNum, uartCallbackWrapper, this);
}

// Blocking read and write

size_t UartTasks::read(void* buf, size_t len, TickType_t ticks)
{
  return rxStreamBuf.read(buf, len, ticks);
}

size_t UartTasks::write(const void* buf, size_t len, TickType_t ticks)
{
  return txMsgBuf.write(buf, len, ticks);
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
  // This interrupt should only be enabled in full duplex mode
  if (halfDuplexCallbacks) {
    critical();
  }

  // Handle transfer-complete
  if (dmaFlagCheckAndClear(ui.dmaTxReg, ui.dmaTxStream, DmaFlag::TC)) {

    // Generate signal that we're ready for next message.
    // The particular bits set in this case is insignificant.
    isrTaskNotifyBits(txTask.handle, 1);

    // We can rely solely on DMA transfer-complete (no need for UART transfer-complete).
    // Next DMA transfer automatically waits until UART is ready to receive more data.
    // Notes on DMA TC vs uart TC:
    // https://community.st.com/s/question/0D50X00009XkdlQSAR/long-wait-for-uartflagtc-after-dma-transfer

  } else {
    // unexpected interrupt triggered - possibly an error flag (FE, DME, TE) (although those interrupts probably not enabled)
    nonCritical();
  }
}

void UartTasks::dmaRxCallback()
{
  // half transfer
  if (dmaFlagCheckAndClear(ui.dmaRxReg, ui.dmaRxStream, DmaFlag::HT)) {
    isrTaskNotifyIncrement(rxTask.handle);
  }

  // transfer complete
  if (dmaFlagCheckAndClear(ui.dmaRxReg, ui.dmaRxStream, DmaFlag::TC)) {
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

  // Only need to handle UART TC in half-duplex mode
  // Full-duplex mode also sets UART TC, but we simply ignore it
  // and rely on DMA TC instead.
  if (halfDuplexCallbacks) {
    // Check for uart transmit complete
    if (LL_USART_IsActiveFlag_TC(ui.uartReg)) {
      LL_USART_ClearFlag_TC(ui.uartReg);

      // We also need to clear the DMA TC flag too.
      // Just reusing this function to clear.
      // Ignoring the check result. No major efficiency gained
      // by rewriting a clear-only version.
      dmaFlagCheckAndClear(ui.dmaTxReg, ui.dmaTxStream, DmaFlag::TC);

      // Indicate to txTask that the transmit has completed,
      // and that we should go back to listening mode.
      // The particular bits set in this case is insignificant.
      isrTaskNotifyBits(txTask.handle, 1);
    }
  }
}

void UartTasks::txFunc()
{
  // Additional DMA setup

  // Memory address to copy from - this is always the same buffer for us
  LL_DMA_SetMemoryAddress(ui.dmaTxReg, ui.dmaTxStream, (uint32_t)txDmaBuf);

  // Peripheral address to write to
  LL_DMA_SetPeriphAddress(ui.dmaTxReg, ui.dmaTxStream, (uint32_t)&ui.uartReg->DR);

  // Half-duplex mode uses Uart TC interrupt instead of DMA TC interrupt
  if (halfDuplexCallbacks) {
    // Half duplex.

    // Clear transmit complete flag from default set value
    // so we don't immediately jump to the ISR once enabled
    // in the next line.
    LL_USART_ClearFlag_TC(ui.uartReg);

    // Enable UART transmit-complete interrupt.
    LL_USART_EnableIT_TC(ui.uartReg);

  } else {
    // Full duplex.
    // Enable DMA transfer-complete interrupt.
    LL_DMA_EnableIT_TC(ui.dmaTxReg, ui.dmaTxStream);
    // Just letting DMA manage flow control, so keeping uart TCIE disabled
  }

  // Enable DMA TX in UART
  LL_USART_EnableDMAReq_TX(ui.uartReg);

  txUtil.watchdogRegisterTask();

  while (1) {
    txUtil.watchdogKick();

    // Wait until new data to send is available on buffer
    size_t len = txUtil.readAll(txMsgBuf, txDmaBuf, sizeof(txDmaBuf));

    // Check if transfer is still in-progress
    if (LL_DMA_IsEnabledStream(ui.dmaTxReg, ui.dmaTxStream)) {
      // error, although we shouldn't ever be in this situation
      error("DMA transfer still in-progress");
    }

    // Configure size of this next transfer
    LL_DMA_SetDataLength(ui.dmaTxReg, ui.dmaTxStream, len);

    // Set output mode for half-duplex
    if (halfDuplexCallbacks) {
      halfDuplexCallbacks->txMode();
    }

    UartTxDbgPinHigh();

    // Start transfer
    LL_DMA_EnableStream(ui.dmaTxReg, ui.dmaTxStream);

    // Wait until UART transmit or DMA transfer is complete.

    // In half-duplex mode we wait for for a signal from the UART ISR
    // that the UART transmit is complete.

    // In full-duplex mode, we can avoid this unnecessary extra
    // delay (~170 uS @ 115200 baud), and wait for the earlier signal
    // from the DMA ISR that the transfer has completed. This means
    // that we can initiate the next dma transfer, even while UART is
    // still busy sending out bits, so there are no unnecessary gaps
    // in data output.

    // Clear notification value upon receipt and block forever.
    txUtil.taskNotifyTake(pdTRUE);

    // Set input mode for half-duplex
    if (halfDuplexCallbacks) {
      halfDuplexCallbacks->rxMode();
      // txUtil.logln("%s enabled rx", pcTaskGetName(txTask.handle));
    }

    UartTxDbgPinLow();
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

  // point to peripheral address to read from
  LL_DMA_SetPeriphAddress(ui.dmaRxReg, ui.dmaRxStream, (uint32_t)&ui.uartReg->DR);

  // point to memory address to write to
  LL_DMA_SetMemoryAddress(ui.dmaRxReg, ui.dmaRxStream, (uint32_t)rxDmaBuf);

  // setup size of memory destination
  LL_DMA_SetDataLength(ui.dmaRxReg, ui.dmaRxStream, sizeof(rxDmaBuf));

  // enable transfer-complete interrupt
  LL_DMA_EnableIT_TC(ui.dmaRxReg, ui.dmaRxStream);

  // enable half-transfer interrupt
  LL_DMA_EnableIT_HT(ui.dmaRxReg, ui.dmaRxStream);

  // Enable idle-line interrupt
  LL_USART_EnableIT_IDLE(ui.uartReg);

  // Enable DMA RX in UART
  LL_USART_EnableDMAReq_RX(ui.uartReg);

  // Enable DMA stream
  LL_DMA_EnableStream(ui.dmaRxReg, ui.dmaRxStream);

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

  rxUtil.watchdogRegisterTask();

  while (1) {

    rxUtil.watchdogKick();

    // Wait until it's time to read from DMA

    // Clear notification value upon receipt and block forever
    uint32_t notifyValue = rxUtil.taskNotifyTake(pdTRUE);

    UartRxDbgPinHigh();

    // Figure out where we are in the buffer
    uint32_t newIdx = sizeof(rxDmaBuf) - LL_DMA_GetDataLength(ui.dmaRxReg, ui.dmaRxStream);

    // rxUtil.logln("%s got rx data to index %lu", pcTaskGetName(rxTask.handle), newIdx);

    if (newIdx > oldIdx) {
      // New data

      // Wait forever to write
      rxUtil.write(rxStreamBuf, rxDmaBuf + oldIdx, newIdx - oldIdx);

    } else if (newIdx < oldIdx) {
      // New data with rollover - needs two separate writes

      // Write everything until the end of the buffer
      rxUtil.write(rxStreamBuf, rxDmaBuf + oldIdx, sizeof(rxDmaBuf) - oldIdx);

      // If we respond quickly to the TC interrupt, then there will be no
      // rollover data written, so this following packet will be of length 0.
      // FreeRTOS asserts when attempting to write a packet of length 0,
      // so we'll skip this unnecessary (and problematic) write.
      if (newIdx > 0) // check if there's any rollover data to write.
      {
        // Write the next chunk at the beginning of the buffer
        rxUtil.write(rxStreamBuf, rxDmaBuf, newIdx);
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

    UartRxDbgPinLow();

    oldIdx = newIdx;

    // Check if we got a special notification value which indicates that DMA got to the end of the write
    // buffer and rolled over.
    if (notifyValue >= rxRolloverFlag) {
      lostBuffers++;
    }
  }
}
