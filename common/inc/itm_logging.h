/*
 * Provides ITM-based logging over SWV.
 * Note that these logging calls are protected by a common
 * mutex to prevent interleaving.
 * For higher-performance logging, use an instance of the ItmItmLogger class.
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

enum class ItmPort : uint32_t
{
  Print = 0, // reserved for printing
  // Viewing data sent to the following ports is not as easy as it could be:
  // https://community.st.com/s/question/0D53W00000Y4DuCSAV/log-numeric-data-in-swv-itm-data-console
  UsbBytesIn,
  UsbBytesOutPending,
  UsbBytesOutTransmitted,
  PacketsInCount,
  PacketsInSequence,
  PacketsOutCount,
  PacketsOutSequence,
  // Last bit used as workaround for this issue:
  // https://community.st.com/s/question/0D53W00000Hx6dxSAB/bug-itm-active-port-ter-defaults-to-port-0-enabled-when-tracing-is-disabled
  Enabled = 31,
};

struct LogMsg
{
  size_t len;
  char buf[252];
};

// Maximum number of hex bytes we could display in the message
// 3 chars per byte, plus newline
const size_t itmMaxHexBytes = (sizeof(LogMsg::buf) - 1) / 3;

uint32_t itmEnabled(ItmPort port);

// Blocking write to ITM log.
// This only works during a debugging session.
size_t itmSendBuf(const char* buf, size_t len);

size_t itmSendString(const char* str);

// convenience macro to add newline chars to itmPrintf
#define itmSendStringln(str) itmSendString(str "\n")

size_t itmSendMsg(LogMsg& msg);

// printf to ITM console
size_t itmPrintf(LogMsg& msg, const char* fmt, ...);

// convenience macro to add newline chars to itmPrintf
#define itmPrintln(msg, format, ...) itmPrintf(msg, format "\n", ##__VA_ARGS__)

// Expands msg to hex in-place
// Returns number of bytes lost due to truncation
size_t toHex(LogMsg& msg);

// Prints message as hex.
// Returns number of bytes lost due to truncation
size_t itmPrintHex(LogMsg& msg);

// Adds linebreak character.
// If there's not enough space, overwrites last character.
void addLinebreak(LogMsg& msg);

// Send a single binary value to an ITM port
void itmSendValue(ItmPort port, uint32_t value);

// For noting errors
void error(const char* fmt, ...);

// A less severe error()
void warn(const char* str);

// Helper function to be called from another variadic function.
// Writes a format string to a msg
size_t vmsgPrintf(LogMsg& msg, const char* fmt, va_list va);

// Version of above that can be called from regular functions.
size_t msgPrintf(LogMsg& msg, const char* fmt, ...);
