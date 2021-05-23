/*
 * See header for notes
 */

#include "itm_logger_task.h"
#include "stdio.h"

ItmLogger::ItmLogger( //
  Watchdog& watchdog,
  UBaseType_t priority)
  : task{ "ItmLogger", funcWrapper, this, priority }
  , watchdog{ watchdog }
{}

/*
 Handles all log messages.
 Cannot let functions log directly, since they may be preempted,
 which interleaves the messages.
 */
void ItmLogger::func()
{
  auto watchdogId = watchdog.registerTask();

  while (1) {
    watchdog.kick(watchdogId);

    // Wait briefly for new data to log.

#if defined(USE_QUEUE_FOR_LOGGER)

    auto status = xQueueReceive(queue.handle, &msg_, suggestedTimeoutTicks);

    // Check if we got any new data to log.
    if (pdPASS != status) {

#elif defined(USE_MSGBUF_FOR_LOGGER)

    msg_.len = msgbuf.read(msg_.buf, sizeof(msg_.buf), suggestedTimeoutTicks);

    // Check if we got any new data to log.
    if (msg_.len == 0) {

#endif

      // We timed out.
      // Print a heartbeat message.
      itmSendStringln("Nothing to log");
    } else {
      // We got a message. Log it.
      while (!itmSendBuf(msg_.buf, msg_.len)) {
        watchdog.kick(watchdogId);
        timeout();
      }
    }
  }
}

// Writes log messages to logging task queue/buffer.
// Returns msg.len if written, 0 if full.
// Note that calling task needs to pass in a msg struct for temporary storage.
// We could alternatively allocate on the stack within this function,
// but that bumps stack size requirements.
// Todo, also log originating task.
// Todo, make a macro version of this.
// A further optimization is to just copy the args directly to the
// stack to defer the printf, but printf to fixed-size buffer is not that slow.
// https://stackoverflow.com/a/1565162
size_t ItmLogger::log(LogMsg& msg, const char* fmt, ...)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return msg.len;
  }

  // Write printf string with expanded arguments into message buf
  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);

  return send(msg);
}

// A tad slower than a macro approach or
// manually including "\n" in the format string
size_t ItmLogger::logln(LogMsg& msg, const char* fmt, ...)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return msg.len;
  }

  // Write printf string with expanded arguments into message buf
  va_list va;
  va_start(va, fmt);
  vmsgPrintf(msg, fmt, va);
  va_end(va);

  // Add newline character
  addLinebreak(msg);

  return send(msg);
}

// Logs message as hex.
// Some bytes may not be printed if message length is too
// large to still fit in message after expansion to hex.
size_t ItmLogger::logHex(LogMsg& msg)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return msg.len;
  }

  // Bonus feature: Could check toHex return value for truncated bytes,
  // then right shift and prefix with a "Truncated: " note.
  toHex(msg);

  return send(msg);
}

// Writes message to logging queue / buffer.
// Returns:
// - msg.len if written
// - 0 if failed to write due to full queue / buffer.
//   - Drops message in this case.
size_t ItmLogger::send(LogMsg& msg)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    return msg.len;
  }

#if defined(USE_QUEUE_FOR_LOGGER)

  return xQueueSendToBack(queue.handle, &msg, suggestedTimeoutTicks);

#elif defined(USE_MSGBUF_FOR_LOGGER)

  // Message Buffers only allow a single writer by default,
  // but we can support multiple writers with a mutex.
  // This mutex is built-in to queues already.

  // Waiting half of recommended timeout because we're blocking twice:
  // - once for mutex, and once for write.
  ScopedLock locked(mutex, suggestedTimeoutTicks / 2);

  // Return 0 if we timed-out before acquiring lock
  if (!locked.gotLock()) {
    // Message dropped
    return 0;
  }

  return msgbuf.write(msg.buf, msg.len, suggestedTimeoutTicks / 2);

#endif
}
