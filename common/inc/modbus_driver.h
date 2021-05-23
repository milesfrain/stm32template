/*
 * Manages modbus communications over a provided UART interface.
 * Also generates and sends errors to provided target and logger.
 *
 * Data is passed to and from this object via mutable outPkt and inPkt structs.
 */

#pragma once

#include "interfaces.h"
#include "itm_logging.h"
#include "modbus_defs.h"
#include "packets.h"
#include "task_utilities.h"
#include "uart_tasks.h"

class ModbusDriver
{
public:
  ModbusDriver(UartTasks& uart,          // where to send and receive modbus data
               uint32_t responseDelayMs, // how long to wait for a response
               Writable& target,         // where to send resulting packets
               Packet& packet,           // for above - reuses parent's
               TaskUtilities& util       // common utilities
  );

  ModbusPacket* const outPkt = (ModbusPacket*)outBuf;
  ModbusPacket* const inPkt = (ModbusPacket*)inBuf;

  uint32_t sendRequest();
  void shiftOutConsumedBytes(size_t len);

private:
  void flushInput();
  uint32_t readMinBytesWithTimeout(size_t targetLen, uint32_t startTick, uint32_t maxTicks);

  UartTasks& uart;

  // How long to wait for modbus server to prepare a response.
  uint32_t responseDelayMs;

  Writable& target;
  Packet& packet;

  TaskUtilities& util;

  uint8_t outBuf[MaxModbusPktSize];
  uint8_t inBuf[MaxModbusPktSize];

  // Number of bytes stored in inBuf
  uint32_t inLen = 0;

  // Use this cycle count to determine when it's safe to send the next request
  uint32_t lastResponseCompletedCycle = 0;
};
