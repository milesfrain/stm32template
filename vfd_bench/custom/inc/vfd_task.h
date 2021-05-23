/*
 * Manages communications with GS3 VFD.
 * https://cdn.automationdirect.com/static/manuals/gs3m/gs3m.pdf
 * Receives packet commands over Writable interface.
 * Sends results to provided target.
 */

#pragma once

#include "modbus_driver.h"

class VfdTask : public Writable
{
public:
  VfdTask(const char* name,                       // task name
          UartTasks& uart,                        // where to send and receive modbus data
          Writable& target,                       // where to send resulting packets
          TaskUtilitiesArg& utilArg,              // common utilities
          UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

  // Required by Writable interface.
  // Describes how to give command packets to this object.
  size_t write(const void* buf, size_t len, TickType_t ticks);

private:
  static void funcWrapper(VfdTask* p) { p->func(); }

  // Could follow the interfaces approach for uart too, but more involved,
  // or requires splitting into two separate args for read/write.
  // https://stackoverflow.com/questions/33427561/composing-interfaces-in-c
  UartTasks& uart;

  Writable& target;
  Packet packet; // for receiving and reporting

  TaskUtilities util;

  StaticTask<VfdTask> task;

  // Where to stash incoming command packets.
  StaticMessageBuffer<sizeof(Packet) * 12> msgbuf;

  ModbusDriver bus;
};
