/*
 * See header for summary
 */

#include "packet_logger.h"
#include "packet_utils.h"

// Formats and logs a packet via provided logger,
// but only if logging is enabled.
// Skips expensive formatting step when logging is disabled.
// Returns number of bytes written, 0 if failed.
int logPacketBase(const char* callerName, const char* note, const Packet& packet, ItmLogger* logger, LogMsg& msg)
{
  // Skip logging if disabled
  if (!itmEnabled(ItmPort::Print)) {
    // Number of bytes to write is unknown until formatting,
    // but returning a non-zero value here is good enough to
    // indicate success to caller.
    return 1;
  }

  if (!logger) {
    critical();
  }

  msg.len = snprintf((char*)&msg.buf, sizeof(msg), "%s%s", callerName, note);
  msg.len += snprintPacket(msg.buf + msg.len, sizeof(msg.buf) - msg.len, packet);
  addLinebreak(msg);
  return logger->send(msg);
}

int logPacketBase(const char* callerName, const char* note, const Packet& packet, ItmLogger& logger, LogMsg& msg)
{
  return logPacketBase(callerName, note, packet, &logger, msg);
}
