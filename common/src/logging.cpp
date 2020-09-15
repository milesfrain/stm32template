/*
 * Common logging functions.
 * Notes
 * - ITM logging could be split away into another file.
 * - These could be enhanced with macros that note filename, line number, and function.
 * - Could add control over log verbosity levels.
 */

#include "logging.h"
#include "basic.h"
#include "static_rtos.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

// For red LED when generating error
#include "board_defs.h"
#include "stm32f4xx_ll_gpio.h"

// Helper function to be called from another variadic function.
// Writes a format string to a msg
void vmsgPrintf(struct LogMsg& msg, const char* fmt, va_list va)
{
  msg.len = vsnprintf(msg.buf, sizeof(msg), fmt, va);
}

// Version of above that can be called from regular functions.
// todo - Use variadic templates to cut down on duplication:
// https://embeddedartistry.com/blog/2017/07/14/comparing-variadic-functions-c-vs-c/
void msgPrintf(struct LogMsg& msg, const char* fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);
}

// We define our own version of _write(), which is used by stdio functions (putchar, puts, printf, etc.).
// But we probably don't need to use any of these functions.
// Ignoring file descriptor.
// Note that buffering is enabled by default, so message's won't be printed until they reach a certain size,
// or if there's a newline character.
// Buffering can be disabled by calling this once during setup:
//   setvbuf(stdout, NULL, _IONBF, 0);
extern "C" int _write(int fd, char* buf, int len)
{
  itmSendBuf(buf, len);
  return len;
}

// Todo - macro-controlled logging verbosity levels

// -- todo, these can all be renamed to itmSend with overloading

// Blocking write to ITM log.
// This only works during a debugging session.
void itmSendBuf(const char* buf, size_t len)
{
  // This mutex is shared across all threads that call this logging function.
  static auto mutex = StaticMutex();

  // First check if ITM is even enabled, so we don't waste time spinning in a loop when not debugging.
  // Using port 2 as a dummy flag:
  // https://community.st.com/s/question/0D53W00000Hx6dxSAB/bug-itm-active-port-ter-defaults-to-port-0-enabled-when-tracing-is-disabled
  // Note that it takes about 10 uS per character to log over ITM (~10x faster than 115k uart).
  // Note that this section may still be active, even when not debugging.
  // May still be active across uC restarts (even with st-link disconnected)
  // Luckily always auto-disables upon power cycle.
  if ((ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & (1 << 0)) && (ITM->TER & (1 << 2))) {
    LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_9);
    // This section is mutex protected so we don't interleave partial prints.
    // Note that we can't call this from non-task code or ISRs,
    // but we could make other version of itm printing for that purpose.
    ScopedLock locked(mutex.handle);

    for (uint i = 0; i < len; i++) {
      LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_8);
      ITM_SendChar(buf[i]);
      LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_8);
    }
    LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_9);
  }
}

void itmSendString(const char* str)
{
  itmSendBuf(str, strlen(str));
}

void itmSendMsg(struct LogMsg& msg)
{
  itmSendBuf(msg.buf, msg.len);
}

// printf to ITM console
void itmPrintf(struct LogMsg& msg, const char* fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);
  itmSendMsg(msg);
}

// Expands msg to hex in-place
// Returns number of bytes lost due to truncation
size_t toHex(LogMsg& msg)
{
  // Number of bytes we're going to print (might be less than maximum)
  size_t printableBytes = min(MaxHexBytes, msg.len);
  // check for truncation
  size_t bytesLost = msg.len - printableBytes;
  // Setup newline chars
  memcpy(msg.buf + printableBytes * 3, "\r\n", 2);
  // temporary storage to capture null character
  char tmpHex[4];
  // Replace from back to front
  for (int i = printableBytes - 1; i >= 0; i--) {
    // Could substitute this with an optimized alternative.
    // Note that there's no way to tell printf to omit the null terminator
    snprintf(tmpHex, sizeof(tmpHex), " %02x", *(msg.buf + i));
    memcpy(msg.buf + i * 3, tmpHex, 3);
  }
  // Update length
  msg.len = printableBytes * 3 + 2;
  return bytesLost;
}

// Prints message as hex.
// Returns number of bytes lost due to truncation
size_t itmPrintHex(LogMsg& msg)
{
  size_t truncated = toHex(msg);
  if (truncated) {
    // Calling function can make another print with specific truncation number
    itmSendString("Truncated:\r\n");
  }
  itmSendMsg(msg);
  return truncated;
}

// For noting errors
void error(const char* str)
{
  // Maybe this should blink instead to avoid confusion with other red LEDs on board
  LL_GPIO_SetOutputPin(RedLedPort, RedLedPin);

  // Log directly to ITM, since logging queue cannot be trusted in error conditions.
  // Mutex to avoid interleaving messages
  static auto mutex = StaticMutex();
  {
    ScopedLock locked(mutex.handle);
    itmSendString("ERROR: ");
    itmSendString(str);
    itmSendString("\r\n");
  }
}

// A less severe error()
void warn(const char* str)
{
  // Mutex to avoid interleaving messages
  static auto mutex = StaticMutex();
  {
    ScopedLock locked(mutex.handle);
    itmSendString("Warning: ");
    itmSendString(str);
    itmSendString("\r\n");
  }
}
