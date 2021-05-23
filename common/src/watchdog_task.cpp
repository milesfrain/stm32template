/*
 * See header for notes
 *
 */

#include "watchdog_task.h"
#include "packet_flow_tasks.h"
#include "packet_logger.h"

Watchdog::Watchdog( //
  UBaseType_t priority)
  : task{ "watchdog", funcWrapper, this, priority }
{}

void Watchdog::func()
{
  while (1) {
    if (!numTasks) {
      // No tasks are registered with watchdog.
      // This can happen if watchdog task is started before other
      // tasks get a chance to register.

      osDelay(10);
      continue;

      // Without this check, this high-priority watchdog task will hog CPU.
    }

    EventBits_t expectedRunningTasks = (1 << numTasks) - 1;

    EventBits_t runningTasks = xEventGroupWaitBits( //
      eventGroup.handle,
      expectedRunningTasks,
      pdTRUE,
      pdTRUE,
      watchdogTimeoutTicks);

    // Check for stalled tasks
    EventBits_t stalled = expectedRunningTasks & ~runningTasks;

    if (stalled) {
      // Just reporting timeouts for now, rather than letting system reset
      // via HW watchdog.

      uint32_t now = xTaskGetTickCount();

      nonCritical();

      // Report all stalled tasks

      uint32_t id = 0;
      while (stalled) {
        // Check lsb
        if (stalled & 1) {
          // Form reporting packet.
          setPacketIdAndLength(packet, PacketID::WatchdogTimeout);
          packet.body.watchdogTimeout.unresponsiveTicks = now - tasks[id].lastKickTick;

          // Copying all bytes likely faster than checking for terminating char
          memcpy(&packet.body.watchdogTimeout.name, tasks[id].name, configMAX_TASK_NAME_LEN);

          // Report timeout

          if (packetOutput) {
            while (!packetOutput->write(&packet, packet.length, suggestedTimeoutTicks)) {
              timeout();
            }
          }

          if (logger) {
            while (!logPacketBase("", "", packet, logger, msg)) {
              timeout();
            }
          }
        }

        id++;
        stalled >>= 1;
      }
    } else {
      // No stalled tasks.
      // We are not using hardware watchdog yet, but if we were,
      // we'd kick it here to prevent system reset.
    }
  }
}

uint32_t Watchdog::registerTask()
{
  assert(numTasks < maxTasks);
  uint32_t id = numTasks;
  numTasks++;

  tasks[id].name = pcTaskGetName(xTaskGetCurrentTaskHandle());
  tasks[id].lastKickTick = xTaskGetTickCount();

  return id;
}

void Watchdog::kick(uint32_t id)
{
  assert(id < maxTasks);
  xEventGroupSetBits(eventGroup.handle, 1 << id);
  tasks[id].lastKickTick = xTaskGetTickCount();
}
