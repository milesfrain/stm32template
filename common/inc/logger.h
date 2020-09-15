/*
 * A combination "logger" task and queue.
 * Ensures that logs arrive in order without a blocking mutex.
 * Write logs to this by calling "log()".
 * Calls to log() may eventually be made faster by caching
 * printf args for formatting later. This improvement won't
 * require any changes from users.
 * Some additional ideas for improvements:
 * - Macro for capturing file, function name, and line number.
 * - Log levels and filtering by severity.
 */

#pragma once

#include "logging.h"
#include "static_rtos.h"

// Could make this a template parameter instead, but that seems unnecessary
const int LogQueueLength = 64;

class Logger
{
public:
  Logger(UBaseType_t = osPriorityNormal // task priority
  );

  int log(struct LogMsg& msg, const char* fmt, ...);
  // rtos looping function for task
  void func();

private:
  static void funcWrapper(Logger* p) { p->func(); }
  StaticTask<Logger> task;
  struct LogMsg msg; // A temporary place to store messages read from the queue
  // It would be better if rtos allowed peek-pop to avoid this redundant copy
  StaticQueue<LogMsg, LogQueueLength> queue;
};
