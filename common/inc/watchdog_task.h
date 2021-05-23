/*
 * A watchdog management task that monitors other tasks for
 * lockups or long stalls that exceed the timeout threshold.
 *
 * When a timeout is detected, it is logged, but this behavior
 * could be modified to allow a hardware watchdog timer to expire
 * and reset the device.
 *
 * To enable task monitoring, each task must call registerTask()
 * at the beginning of their RTOS function, then call kick()
 * periodically within the timeout threshold.
 *
 * There's a circular dependency setup challenge where
 * watchdog needs access to the following tasks, but those
 * tasks also need access to the watchdog:
 *  - logger (ITM)
 *  - packetOutput
 * This means that we can't statically create these object
 * relationships at compile time. So instead, watchdog allows
 * setting pointers to these objects during runtime, after they
 * are created. If those pointers remain unset / null, then watchdog
 * will skip attempting to report timeouts through those channels.
 *
 * Todo:
 * Another design option to reduce code coupling is to pass callback
 * functions describing:
 *  - How to report a task timeout (takes a TrackedTask arg)
 *  - How to kick the hardware watchdog
 */

#pragma once

#include "itm_logger_task.h"
#include "packets.h"
#include "watchdog_common.h"

// forward declarations to work-around circular dependencies
class PacketOutput;
class ItmLogger;

static_assert(configMAX_TASK_NAME_LEN == sizeof(WatchdogTimeout::name));

/*
 * Would be nice if we could assert check maxTasks against
 * number of available event group bits.
 * Could potentially get more info by inspecting eventEVENT_BITS_CONTROL_BYTES,
 * but that only has visibility within event_groups.c
 */
const uint32_t maxTasks = 24;

struct TrackedTask
{
  char* name;
  uint32_t lastKickTick;
};

class Watchdog
{
public:
  // Set high task priority to prevent busy loop in other tasks from
  // suppressing watchdog timeout reporting.
  Watchdog(UBaseType_t priority = osPriorityHigh);

  // Optionally set these pointers once these objects are created
  ItmLogger* logger = nullptr;
  PacketOutput* packetOutput = nullptr;

  // rtos looping function for task
  void func();

  // Call at the start of each monitored tasks's RTOS function.
  uint32_t registerTask();

  // Call periodically within each monitored tasks's RTOS function
  // with the id returned by registerTask().
  void kick(uint32_t id);

private:
  static void funcWrapper(Watchdog* p) { p->func(); }
  StaticTask<Watchdog> task;

  StaticEventGroup eventGroup;
  TrackedTask tasks[maxTasks];
  uint32_t numTasks = 0;

  LogMsg msg;    // for logging via ITM
  Packet packet; // for logging via packetOutput
};
