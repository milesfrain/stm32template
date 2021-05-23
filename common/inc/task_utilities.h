/*

Convenience wrapper for common objects of most tasks.
This makes it less tedious to create members for and
pass these arguments to tasks:
  - logger
  - watchdog

Debatable whether it's worthwhile to also add packetOutput
to this wrapper.

*/

#pragma once

#include "interfaces.h"
#include "packet_logger.h"
#include "watchdog_task.h"

struct TaskUtilitiesArg
{
  TaskUtilitiesArg(ItmLogger& logger, Watchdog& watchdog)
    : logger{ logger }
    , watchdog{ watchdog } {};

  ItmLogger& logger;
  Watchdog& watchdog;
};

/*
 */
struct TaskUtilities
{

  TaskUtilities(TaskUtilitiesArg& arg)
    : arg{ arg } {};

  // ------- Watchdog Wrappers -------

  void watchdogRegisterTask() //
  {
    watchdogId = arg.watchdog.registerTask();
  }

  void watchdogKick() //
  {
    arg.watchdog.kick(watchdogId);
  }

  // ------- ItmLogger Wrappers -------
  // Reports if any of these time-out, and prevents delays
  // in these functions triggering watchdog timeout.

  size_t logPacket(const char* callerName, const char* note, const Packet& packet) //
  {
    return reportLogTimeout(logPacketBase(callerName, note, packet, arg.logger, msg));
  }

  template<typename... Args>
  size_t log(const char* fmt, Args... args)
  {
    return reportLogTimeout(arg.logger.log(msg, fmt, args...));
  }

  template<typename... Args>
  size_t logln(const char* fmt, Args... args)
  {
    return reportLogTimeout(arg.logger.logln(msg, fmt, args...));
  }

  template<typename... Args>
  size_t warnln(const char* fmt, Args... args)
  {
    return reportLogTimeout(arg.logger.warnln(msg, fmt, args...));
  }

  size_t logHex(LogMsg& msg) //
  {
    return reportLogTimeout(arg.logger.logHex(msg));
  }

  size_t send(LogMsg& msg) //
  {
    return reportLogTimeout(arg.logger.send(msg));
  }

  // ------- Read / Write Wrappers -------

  // Watchdog-friendly blocking write wrapper.
  // Writes len bytes from buf to target.
  size_t write(Writable& target, const void* buf, size_t len)
  {
    // Blocks until successful receive with periodic watchdog kicks
    size_t retval;
    while (!(retval = target.write(buf, len, suggestedTimeoutTicks))) {
      watchdogKick();
      // Failure to write means that the target buffer is full,
      // and so there is and issue in the data pipeline that should
      // be noted.
      timeout();
    }
    // Kick again before returning to avoid situations where
    // delays can accumulate to exceed timeout threshold.
    watchdogKick();
    return retval;
  }

  // Watchdog-friendly blocking read wrapper.
  // Reads len bytes from target to buf.
  size_t read(Readable& target, void* buf, size_t len)
  {
    // Blocks until successful receive with periodic watchdog kicks
    size_t retval;
    while (!(retval = target.read(buf, len, suggestedTimeoutTicks))) {
      watchdogKick();
      // Failure to read is perfectly normal.
      // There could simply be no new data to read.
      benignTimeout();
    }
    // Kick again before returning to avoid situations where
    // delays can accumulate to exceed timeout threshold.
    watchdogKick();
    return retval;
  }

  // A special version of read() for MessageBuffers which makes multiple reads in an
  // attempt to fill the provided receipt buffer with one or more messages.
  // Note that read() on MessageBuffer would otherwise just return a single message,
  // and read() on a StreamBuffer always attempts to fill the provided receipt buffer
  // with as many bytes as possible.
  template<size_t TSize>
  size_t readAll(StaticMessageBuffer<TSize>& msgbuf, void* buf, size_t bufLen)
  {

    // Wait until new data is available.
    // This just grabs the first message.
    size_t consumedLen = read(msgbuf, buf, bufLen);

    // Keep attempting to pack more messages into buffer
    while (1) {
      // Check for additional messages to read
      size_t nextAvailableLen = msgbuf.nextLengthBytes();
      // If there's another message to read and we have space for it
      if (nextAvailableLen && consumedLen + nextAvailableLen <= bufLen) {
        // Grab next message with no timeout
        size_t nextReceivedLen = msgbuf.read((uint8_t*)buf + consumedLen, bufLen - consumedLen, 0);
        // Sanity check that we read the expected number of bytes
        if (nextReceivedLen != nextAvailableLen) {
          critical();
        }
        // Note consumed bytes
        consumedLen += nextReceivedLen;
      } else {
        // Either no more messages, or no more space
        break;
      }
    }

    return consumedLen;
  }

  // ------- ulTaskNotifyTake -------

  // Blocks forever while waiting for notification.
  // Calls timeoutKick after each ticks delay.
  uint32_t taskNotifyTake(BaseType_t clearCountOnExit)
  {
    uint32_t retval;
    while (!(retval = ulTaskNotifyTake(clearCountOnExit, suggestedTimeoutTicks))) {
      watchdogKick();
      timeout();
    }
    // Kick again before returning to avoid situations where
    // delays can accumulate to exceed timeout threshold.
    watchdogKick();
    return retval;
  }

  // -------

  LogMsg msg;

private:
  TaskUtilitiesArg& arg;

  // Set to invalid initial value to catch missing registerTask call.
  uint32_t watchdogId = maxTasks;

  // Common code in all util log wrappers.
  // Reports a timeout if logging failed.
  size_t reportLogTimeout(size_t retval)
  {
    if (!retval) {
      timeout();
    }
    // Kick again before returning to avoid situations where
    // delays can accumulate to exceed timeout threshold.
    watchdogKick();
    return retval;
  }
};
