/*
 * Producer and Consumer classes for throughput testing
 * to objects that are Writable or Readable.
 */

#pragma once

#include "cmsis_os.h"
#include "interfaces.h"
#include "logging.h"
#include "packets.h"
#include "static_rtos.h"

class Producer
{
public:
  Producer(const char*,                   // task name
           uint32_t,                      // source id
           Writable&,                     // target
           UBaseType_t = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(Producer* p) { p->func(); }
  Writable& target;
  StaticTask<Producer> task;
  TestPacket pkt;
  static uint8_t dummyData[dummyDataSize];
  uint32_t source_id;
};

const size_t consumerBufSize = 200;

class Consumer
{
public:
  Consumer(const char*,                   // task name
           Readable&,                     // target
           UBaseType_t = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(Consumer* p) { p->func(); }
  Readable& target;                                        // readable interface
  StaticTask<Consumer, configMINIMAL_STACK_SIZE * 2> task; // local static task
  uint8_t buf[consumerBufSize];                            // storage for parsing
  size_t len;                                              // number of bytes in buffer
  struct LogMsg msg;                                       // for logging
  uint32_t pktCt;
};

class ProducerUsb
{
public:
  ProducerUsb(const char*,                   // task name
              Writable&,                     // target
              UBaseType_t = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(ProducerUsb* p) { p->func(); }
  Writable& target;
  StaticTask<ProducerUsb> task;
};

class ConsumerUsb
{
public:
  ConsumerUsb(const char*,                   // task name
              Readable&,                     // target
              UBaseType_t = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(ConsumerUsb* p) { p->func(); }
  Readable& target;
  StaticTask<ConsumerUsb> task;
  uint32_t byteCt;      // how many bytes we got so far
  struct LogMsg msg;    // for logging
  struct LogMsg hexMsg; // for hex logging
};

class Pipe
{
public:
  Pipe(const char*,                   // task prefix
       Readable&,                     // src target
       Writable&,                     // dst target
       UBaseType_t = osPriorityNormal // task priority
  );

  // rtos looping function
  void func();

private:
  static void funcWrapper(Pipe* p) { p->func(); }
  Readable& src;
  Writable& dst;
  StaticTask<Pipe> task;
  uint8_t buf[32]; // Internal intermediate buffer for passing data between reader and writer
  size_t len;      // bytes in buffer
  size_t written;  // bytes written from buffer (in case of partial writes)
};
