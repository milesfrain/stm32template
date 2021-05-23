/*
 * Provides building-blocks for flowing packets through the system.
 */

#pragma once

#include "interfaces.h"
#include "packet_utils.h"
#include "task_utilities.h"

class PacketIntake
  : public CanProcessPacket
  , public Readable
{
public:
  PacketIntake(const char* name,                       // task name
               Readable& target,                       // supplies unparsed data stream
               TaskUtilitiesArg& utilArg,              // common utilities
               UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

  // Required by CanProcessPacket interface.
  // Describes what to do with parsed packets.
  void processPacket(const Packet& packet);

  // Required by Readable interface.
  // Describes how to get packets from this object.
  size_t read(void* buf, size_t len, TickType_t ticks);

private:
  static void funcWrapper(PacketIntake* p) { p->func(); }
  Readable& target;
  PacketParser parser;
  TaskUtilities util;
  StaticTask<PacketIntake> task;

  uint8_t buf[maxWrappedPacketLength * 2]; // storage for parsing
  size_t len = 0;                          // number of bytes in buffer
  uint32_t packetsInCount = 0;             // number of good packets received

  // Where to stash parsed packets until they are ready to be read.
  StaticMessageBuffer<sizeof(Packet) * 12> msgbuf;
};

class PacketOutput : public Writable
{
public:
  PacketOutput(const char* name,                       // task name
               Writable& target,                       // where to send wrapped packets
               TaskUtilitiesArg& utilArg,              // common utilities
               UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

  // Required by Writable interface.
  // Describes how to give packets to this object.
  size_t write(const void* buf, size_t len, TickType_t ticks);

private:
  static void funcWrapper(PacketOutput* p) { p->func(); }
  Writable& target;
  TaskUtilities util;
  StaticTask<PacketOutput> task;
  WrappedPacket wrap;

  uint32_t packetsOutCount = 0; // number of packets sent

  // For rewrapping packets with correct outgoing sequence number
  PacketSequencer sequencer;

  // Where to stash incoming packets until we are ready to
  // wrap and transmit.
  StaticMessageBuffer<sizeof(Packet) * 12> msgbuf;

  // Mutex allows multiple writers to this object's Message Buffer
  StaticMutex writeMutex;
};

class Coupling
{
public:
  Coupling(const char*,                   // task prefix
           Readable&,                     // src target
           Writable&,                     // dst target
           TaskUtilitiesArg& utilArg,     // common utilities
           UBaseType_t = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(Coupling* p) { p->func(); }
  Readable& src;
  Writable& dst;
  TaskUtilities util;
  StaticTask<Coupling> task;
  uint8_t buf[sizeof(Packet)]; // Internal intermediate buffer for passing data between reader and writer
  size_t len;                  // bytes in buffer
  size_t written;              // bytes written from buffer (in case of partial writes)
};
