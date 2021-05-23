/*
 * Emulates a GS3 VFD (at address 1) for select commands.
 * https://cdn.automationdirect.com/static/manuals/gs3m/gs3m.pdf
 * Also emulates transceiver command echoing.
 */

#pragma once

#include "cmsis_os.h"
#include "interfaces.h"
#include "itm_logging.h"
#include "modbus_defs.h"
#include "static_rtos.h"
#include "task_utilities.h"
#include "uart_tasks.h"

class FakeVfdTask
{
public:
  FakeVfdTask(const char* name,                       // task name
              UartTasks& uart,                        // where to receive and send modbus data
              TaskUtilitiesArg& utilArg,              // common utilities
              UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(FakeVfdTask* p) { p->func(); }

  UartTasks& uart;

  TaskUtilities util;

  StaticTask<FakeVfdTask> task;

  uint8_t inBuf[MaxModbusPktSize];

  // Number of bytes stored in inBuf
  uint32_t inLen = 0;

  ModbusPacket inPkt;
  ModbusPacket outPkt;

  // Tracking frequency for each possible VFD address
  uint16_t frequencies[256] = { 0 };
};
