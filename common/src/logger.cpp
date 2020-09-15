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

#include "logger.h"
#include "stdio.h"

Logger::Logger(UBaseType_t priority)
  : task{ "Logger", funcWrapper, this, priority }
{}

/*
 Handles all log messages.
 Cannot let functions log directly, since they may be preempted,
 which interleaves the messages.
 */
void Logger::func()
{
  while (1) {
    // Wait one second to log new data.
    auto status = xQueueReceive(queue.handle, &msg, pdMS_TO_TICKS(1000));
    // If there's nothing to log, print a heartbeat message.
    if (pdPASS == status) {
      itmSendBuf(msg.buf, msg.len);
    } else {
      itmSendString("Nothing to log\r\n");
    }
  }
}

// Writes log messages to logging task buffer.
// Note that calling task needs to pass in a msg struct for temporary storage.
// We could alternatively allocate on the stack within this function, but that bumps stack size requirements.
// Todo, also log originating task.
// Todo, make a macro version of this.
// A further optimization is to just copy the args directly to the
// stack to defer the printf, but printf to fixed-size buffer is not that slow.
// https://stackoverflow.com/a/1565162
int Logger::log(struct LogMsg& msg, const char* fmt, ...)
{
  // Write printf string with expanded arguments into message buf
  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);

  // Don't block any calling threads for logging (xTicksToWait = 0).
  // We can relax this in the future, but it would be better to
  // initial detect and report these problems and increase log queue size if necessary.
  // But we WILL block to report an error.
  if (errQUEUE_FULL == xQueueSendToBack(queue.handle, &msg, 0)) {
    // Report that we encountered a full queue
    msg.len = snprintf((char*)&msg.buf, sizeof(msg), "Error: Logging queue full");

    // Wait one second to get this error message out.
    if (errQUEUE_FULL == xQueueSendToBack(queue.handle, &msg, pdMS_TO_TICKS(1000))) {
      // Bypass logging queue and write directly to ITM
      itmSendString("Logging queue STILL full");
    }

    return 0;
  }

  return msg.len;
}
