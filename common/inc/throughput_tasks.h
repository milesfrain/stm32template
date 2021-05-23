/*
 * Producer and Consumer classes for throughput testing
 * to objects that are Writable or Readable.
 */

#pragma once

#include "packet_utils.h"
#include "task_utilities.h"

class Producer
{
public:
  Producer(const char* name,                       // task name
           uint32_t source_id,                     // for labeling different producers
           Writable& target,                       // where to send produced data
           TaskUtilitiesArg& utilArg,              // common utilities
           UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(Producer* p) { p->func(); }
  Writable& target;
  TaskUtilities util;
  StaticTask<Producer> task;
  static uint8_t dummyData[100];
  static_assert(sizeof(DummyPacket::payload) <= sizeof(dummyData));
  WrappedPacket dummyWrap;
};

class Consumer : public CanProcessPacket
{
public:
  Consumer(const char* name,                       // task name
           Readable& target,                       // target
           TaskUtilitiesArg& utilArg,              // common utilities
           UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

  // Required by CanProcessPacket interface.
  // Describes what to do with incoming packets.
  void processPacket(const Packet& packet);

private:
  static void funcWrapper(Consumer* p) { p->func(); }
  Readable& target;
  PacketParser parser;
  TaskUtilities util;
  StaticTask<Consumer> task;

  uint8_t buf[maxWrappedPacketLength * 2]; // storage for parsing
  size_t len = 0;                          // number of bytes in buffer
  uint32_t pktCt = 0;                      // number of good packets received
};

class ProducerUsb
{
public:
  ProducerUsb(const char* name,                       // task name
              Writable& target,                       // target
              TaskUtilitiesArg& utilArg,              // common utilities
              UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(ProducerUsb* p) { p->func(); }
  Writable& target;
  TaskUtilities util;
  StaticTask<ProducerUsb> task;
};

class ConsumerUsb
{
public:
  ConsumerUsb(const char* name,                       // task name
              Readable& target,                       // target
              TaskUtilitiesArg& utilArg,              // common utilities
              UBaseType_t priority = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(ConsumerUsb* p) { p->func(); }
  Readable& target;
  TaskUtilities util;
  StaticTask<ConsumerUsb> task;
  uint32_t byteCt = 0; // how many bytes we got so far
  LogMsg hexMsg;       // for hex logging
};
