/*
 * Common logging functions.
 * Notes
 * - ITM logging could be split away into another file.
 * - These could be enhanced with macros that note filename, line number, and function.
 *   - https://stackoverflow.com/questions/23230003/something-between-func-and-pretty-function?noredirect=1&lq=1#comment35541516_23230003
 * - Could add control over log verbosity levels.
 */

#include "itm_logging.h"
#include "basic.h"
#include "board_defs.h"
#include "catch_errors.h"
#include "static_rtos.h"
#include "stm32f4xx.h"
#include "watchdog_common.h"
#include <stdio.h>
#include <string.h>

// Helper function to be called from another variadic function.
// Writes a format string to a msg
size_t vmsgPrintf(LogMsg& msg, const char* fmt, va_list va)
{
  msg.len = vsnprintf(msg.buf, sizeof(msg), fmt, va);
  return msg.len;
}

// Version of above that can be called from regular functions.
// todo - Use variadic templates to cut down on duplication:
// https://embeddedartistry.com/blog/2017/07/14/comparing-variadic-functions-c-vs-c/
size_t msgPrintf(LogMsg& msg, const char* fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);
  return msg.len;
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
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return len;
  }

  itmSendBuf(buf, len);
  return len;
}

uint32_t itmEnabled(ItmPort port)
{
  return                                              //
    ((ITM->TCR & ITM_TCR_ITMENA_Msk) &&               // Globally enabled
     (ITM->TER & (1 << (uint32_t)port)) &&            // Port we're writing to
     (ITM->TER & (1 << (uint32_t)ItmPort::Enabled))); // Workaround for constant enabling bug
}

// Todo - macro-controlled logging verbosity levels

// -- todo, these can all be renamed to itmSend with overloading

// Blocking write to ITM log.
// This only works during a debugging session.
size_t itmSendBuf(const char* buf, size_t len)
{
  // First check if ITM is even enabled, so we don't waste time spinning in a loop when not debugging.
  // Using port 31 as a dummy flag:
  // https://community.st.com/s/question/0D53W00000Hx6dxSAB/bug-itm-active-port-ter-defaults-to-port-0-enabled-when-tracing-is-disabled
  // Note that it takes about 10 uS per character to log over ITM (~10x faster than 115k uart).
  // Note that this section may still be active, even when not debugging.
  // May still be active across uC restarts (even with st-link disconnected)
  // Luckily always auto-disables upon power cycle.
  if (!itmEnabled(ItmPort::Print)) {
    return len;
  }

  // This mutex is shared across all threads that call this logging function.
  static auto mutex = StaticMutex();

  LoggingDbgPinHigh();

  // This section is mutex protected so we don't interleave partial prints.
  // Note that we can't call this from non-task code or ISRs,
  // but we could make other version of itm printing for that purpose.
  ScopedLock locked(mutex, suggestedTimeoutTicks);

  // Return 0 if we timed-out before acquiring lock
  if (!locked.gotLock()) {
    // Message dropped
    return 0;
  }

  for (uint i = 0; i < len; i++) {
    // Blocks with a busy loop.
    // Don't know if there's a better way to handle this besides setting.
    // logger task to low priority.
    ITM_SendChar(buf[i]);
  }

  LoggingDbgPinLow();

  return len;
}

size_t itmSendString(const char* str)
{
  return itmSendBuf(str, strlen(str));
}

size_t itmSendMsg(LogMsg& msg)
{
  return itmSendBuf(msg.buf, msg.len);
}

// printf to ITM console
size_t itmPrintf(LogMsg& msg, const char* fmt, ...)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    // Number of bytes to write is unknown until formatting,
    // but returning a non-zero value here is good enough to
    // indicate success to caller.
    return 1;
  }

  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);
  return itmSendMsg(msg);
}

// Expands msg to hex in-place
// Returns number of bytes lost due to truncation
size_t toHex(LogMsg& msg)
{
  // Number of bytes we're going to print (might be less than maximum)
  size_t printableBytes = min(itmMaxHexBytes, msg.len);
  // check for truncation
  size_t bytesLost = msg.len - printableBytes;
  // Setup newline chars
  memcpy(msg.buf + printableBytes * 3, "\n", 2);
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
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return msg.len;
  }

  size_t truncated = toHex(msg);
  if (truncated) {
    // Calling function can make another print with specific truncation number
    itmSendStringln("Truncated:");
  }
  itmSendMsg(msg);
  return truncated;
}

// Adds linebreak character.
// If there's not enough space, overwrites last character.
void addLinebreak(LogMsg& msg)
{
  // Must be able to at least fit linebreak into buffer.
  // Very odd if buffer is sized smaller than that.
  static_assert(sizeof(msg.buf) >= 1);

  // Find where to put linebreak
  size_t index = min(msg.len, sizeof(msg.buf) - 1);

  // Add linebreak
  msg.buf[index] = '\n';
  msg.len = index + 1;
}

// Send a single binary value to an ITM port
void itmSendValue(ItmPort port, uint32_t value)
{
  // Skip logging if disabled
  if (!itmEnabled(port)) {
    return;
  }

  // Busy loop while we wait for port to be free.
  // Maximum of 10uS wait.
  // Hopefully unique ports will not block each other.
  uint32_t loops = 0;
  while (!ITM->PORT[(uint32_t)port].u32) {
    // Only ever reached the breakpoint set here with stress tests.
    // so unique ports assumption seems mostly correct.
    __NOP();
    loops++;
  }
  if (loops) {
    // nop necessary for catching breakpoint
    __NOP();
  }
  // Write value to port
  ITM->PORT[(uint32_t)port].u32 = value;
}

// For noting errors
void error(const char* fmt, ...)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    critical();
    return;
  }

  // Log directly to ITM, since logging queue cannot be trusted in error conditions.
  // Mutex to allow sharing static msg struct and avoid interleaving messages.
  static auto mutex = StaticMutex();
  static LogMsg msg;
  {
    ScopedLock locked(mutex, suggestedTimeoutTicks);

    // If we timed-out before acquiring lock, don't bother attempting to print.
    if (!locked.gotLock()) {
      critical();
    }

    itmSendString("ERROR: ");

    va_list va;
    va_start(va, fmt);
    vmsgPrintf(msg, fmt, va);
    va_end(va);

    addLinebreak(msg);
    itmSendMsg(msg);
  }

  critical();
}

// A less severe error()
void warn(const char* str)
{
  nonCritical();

  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return;
  }

  // Mutex to avoid interleaving messages
  static auto mutex = StaticMutex();
  {
    ScopedLock locked(mutex, suggestedTimeoutTicks);

    // If we timed-out before acquiring lock, don't bother attempting to print.
    if (!locked.gotLock()) {
      timeout();
    }

    itmSendString("Warning: ");
    itmSendString(str);
    itmSendStringln("");
  }
}
