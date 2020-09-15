/*
 * Provides ITM-based logging over SWV.
 * Note that these logging calls are protected by a common
 * mutex to prevent interleaving.
 * For higher-performance logging, use an instance of the Logger class.
 */

#pragma once

#include "stdarg.h"
#include "stddef.h"

const size_t LogMsgLength = 128;
const size_t MaxLogMsgChars = LogMsgLength - sizeof(size_t);

// Maximum number of hex bytes we could display in the message
// 3 chars per byte, plus newline
const size_t MaxHexBytes = (MaxLogMsgChars - 2) / 3;

struct LogMsg
{
  size_t len;
  char buf[MaxLogMsgChars];
};

// check if things are packed as expected
static_assert(sizeof(LogMsg) == LogMsgLength, "Unexpected size calculation");

// Blocking write to ITM log.
// This only works during a debugging session.
void itmSendBuf(const char* buf, size_t len);

void itmSendString(const char* str);

void itmSendMsg(struct LogMsg& msg);

// printf to ITM console
void itmPrintf(struct LogMsg& msg, const char* fmt, ...);

// Prints message as hex.
// Returns number of bytes lost due to truncation
size_t itmPrintHex(LogMsg& msg);

// For trapping critical errors
inline void critical()
{
  while (1)
    ;
}

// For noting errors
void error(const char* str);

// A less severe error()
void warn(const char* str);

// Helper function to be called from another variadic function.
// Writes a format string to a msg
void vmsgPrintf(struct LogMsg& msg, const char* fmt, va_list va);

// Version of above that can be called from regular functions.
void msgPrintf(struct LogMsg& msg, const char* fmt, ...);
