/*
 * Routes incoming packets to various tasks.
 */

#pragma once

#include "cmsis_os.h"
#include "itm_logging.h"
#include "packet_flow_tasks.h"
#include "packet_utils.h"
#include "packets.h"
#include "static_rtos.h"
#include "task_utilities.h"
#include "vfd_task.h"

class DispatcherTask
{
public:
  DispatcherTask(const char* name, // task name
                 PacketIntake& packetIntake,
                 VfdTask& vfdTask,
                 PacketOutput& packetOutput,
                 TaskUtilitiesArg& utilArg,              // common utilities
                 UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(DispatcherTask* p) { p->func(); }
  PacketIntake& packetIntake;
  VfdTask& vfdTask;
  PacketOutput& packetOutput;
  TaskUtilities util;

  StaticTask<DispatcherTask> task;

  Packet packet;
};
