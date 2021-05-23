/*
 * An ITM logger task.
 * Handles waiting for ITM bus to be free so other tasks
 * are unblocked to do other stuff.
 * Write logs to this by calling "log()".
 * Calls to log() may eventually be made faster by caching
 * printf args for formatting later. This improvement won't
 * require any changes from users.
 * Some additional ideas for improvements:
 * - Macro for capturing file, function name, and line number.
 * - Log levels and filtering by severity.
 */

#pragma once

#include "catch_errors.h"
#include "itm_logging.h"
#include "static_rtos.h"
#include "watchdog_task.h"

// forward declarations to work-around circular dependencies
class Watchdog;

// A Message Buffer seems to be strictly better,
// but allowing easy revert to Queue for performance experiments.
//#define USE_QUEUE_FOR_LOGGER
#define USE_MSGBUF_FOR_LOGGER

class ItmLogger
{
public:
  ItmLogger( //
    Watchdog& watchdog,
    UBaseType_t priority = osPriorityLow // task priority. Contains ITM busy loop
  );

  size_t log(LogMsg& msg, const char* fmt, ...);
  size_t logln(LogMsg& msg, const char* fmt, ...);
  // warnln is identical to logln, but allows easier breakpoint
  // detection of non-critical warnings
  template<typename... Args>
  size_t warnln(LogMsg& msg, const char* fmt, Args... args)
  {
    nonCritical();
    return logln(msg, fmt, args...);
  }
  size_t logHex(LogMsg& msg);
  size_t send(LogMsg& msg);
  // rtos looping function for task
  void func();

private:
  static void funcWrapper(ItmLogger* p) { p->func(); }
  StaticTask<ItmLogger> task;
  // For receiving message from buffer
  LogMsg msg_;
  Watchdog& watchdog;

#if defined(USE_QUEUE_FOR_LOGGER)

  // Can hold 64 messages in all cases.
  StaticQueue<LogMsg, 64> queue;

#elif defined(USE_MSGBUF_FOR_LOGGER)

  // Can hold 64 messages in worst case, and many more if messages don't require all space.
  StaticMessageBuffer<sizeof(LogMsg) * 64> msgbuf;
  // Mutex allows multiple writers to logger's Message Buffer
  StaticMutex mutex;

#endif
};
