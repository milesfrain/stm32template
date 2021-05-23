/*
 * See header for comments
 */

#include "dispatcher_task.h"

DispatcherTask::DispatcherTask( //
  const char* name,
  PacketIntake& packetIntake,
  VfdTask& vfdTask,
  PacketOutput& packetOutput,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : packetIntake{ packetIntake }
  , vfdTask{ vfdTask }
  , packetOutput{ packetOutput }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void DispatcherTask::func()
{
  util.watchdogRegisterTask();

  while (1) {

    util.watchdogKick();

    // Get next available packet
    util.read(packetIntake, &packet, sizeof(packet));

    // Todo - more error handling. Double-check lengths, etc.

    // Route vfd commands to vfdTask.
    // All other packets go straight to output.
    switch (packet.id) {
      case PacketID::VfdSetFrequency: //
        util.write(vfdTask, &packet, packet.length);
        break;
      default: //
        util.write(packetOutput, &packet, packet.length);
        break;
    }
  }
}
