/*
 * See header for notes.
 */

#include "vfd_task.h"
#include "board_defs.h"
#include "catch_errors.h"
#include "packet_utils.h"
#include "string.h" // memcpy
#include "vfd_defs.h"

VfdTask::VfdTask( //
  const char* name,
  UartTasks& uart,
  Writable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : uart{ uart }
  , target{ target }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
  , bus{ uart, responseDelayMs, target, packet, util }
{}

void VfdTask::func()
{
  // Assuming sequential address, including broadcast address (0)
  // Todo - more flexible address configuration

  const uint8_t numNodes = 3; // node 1, 2, and broadcast
  // const uint8_t numNodes = 6; // nodes 1-5, and broadcast

  uint16_t lastFrequency[numNodes];
  for (int i = 0; i < numNodes; i++) {
    lastFrequency[i] = -1; // invalid, max of 4000
  }
  uint16_t setFrequency[numNodes] = { 0 };

  // Which address we're focusing on updating.
  // Address 0 is broadcast, which does not get any responses.
  uint8_t focus = 0;

  util.watchdogRegisterTask();

  while (1) {

    util.watchdogKick();

    // Collect all incoming host commands before deciding what modbus commands to send
    while (msgbuf.read(&packet, sizeof(packet), 0)) {
      switch (packet.id) {
        case PacketID::VfdSetFrequency: {
          uint8_t node = packet.body.vfdSetFrequency.node;
          uint16_t freq = packet.body.vfdSetFrequency.frequency;
          util.logln( //
            "%s got command to set vfd %u frequency to %u.%u Hz",
            pcTaskGetName(task.handle),
            node,
            freq / 10,
            freq % 10);

          if (node < numNodes) {
            setFrequency[node] = freq;
          } else {
            util.logln( //
              "%s got invalid address %u, exceeds %u",
              pcTaskGetName(task.handle),
              node,
              numNodes - 1);
          }
          break;
        }
        default:
          util.logln( //
            "%s doesn't know what to do with packet id: %s",
            pcTaskGetName(task.handle),
            packetIdToString(packet.id));
          critical();
          break;
      }
    }

    // For each address in round-robbin fashion,
    // create a modbus "Request" packet to send.
    // If there's a new frequency setpoint, send that,
    // otherwise request status.

    focus += 1;
    focus %= numNodes;

    if (setFrequency[focus] != lastFrequency[focus]) {

      // Write frequency value
      bus.outPkt->nodeAddress = focus;
      bus.outPkt->command = FunctionCode::WriteSingleRegister;
      bus.outPkt->writeSingleRegisterRequest.registerAddress = frequencyRegAddress;
      bus.outPkt->writeSingleRegisterRequest.data = setFrequency[focus];

    } else {

      // Don't broadcast status request. Won't get a response.
      if (focus == 0) {
        continue;
      }

      // Read status registers
      bus.outPkt->nodeAddress = focus;
      bus.outPkt->command = FunctionCode::ReadMultipleRegisters;
      bus.outPkt->readMultipleRegistersRequest.startingAddress = statusRegAddress;
      bus.outPkt->readMultipleRegistersRequest.numRegisters = statusRegNum;
    }

    // Set origin for all outgoing reporting packets.
    // Note that this packet is reused by modbus driver.
    packet.origin = PacketOrigin::TargetToHost;

    uint32_t respLen = bus.sendRequest();

    // Special handling for broadcast messages
    if (respLen == 1 && bus.outPkt->nodeAddress == 0) {
      // If frequency setpoint update
      if (bus.outPkt->command == FunctionCode::WriteSingleRegister && //
          __builtin_bswap16(bus.outPkt->writeSingleRegisterRequest.registerAddress) == frequencyRegAddress) {
        // Only update last frequency setpoint if write succeeded.
        // Otherwise, will attempt retransmission next lap.
        // For broadcast, failure could be due to a bad echo.
        lastFrequency[focus] = setFrequency[focus];
      } else {
        error("Unexpected modbus broadcast");
      }

      // Successful non-broadcast requests
    } else if (respLen) {

      switch (bus.inPkt->command) {

        case FunctionCode::ReadMultipleRegisters: {
          // The requested register is not returned in the response,
          // and the outgoing request packet inverted endianness, so
          // need to reverse that conversion.
          uint16_t regAddr = __builtin_bswap16(bus.outPkt->readMultipleRegistersRequest.startingAddress);
          switch (regAddr) {
            case statusRegAddress:
              // Response size is already verified by modbus driver.

              // Form packet for reporting
              setPacketIdAndLength(packet, PacketID::VfdStatus);
              packet.body.vfdStatus.nodeAddress = bus.inPkt->nodeAddress;
              memcpy(&packet.body.vfdStatus.payload, bus.inPkt->readMultipleRegistersResponse.payload, sizeof(VfdStatus::payload));

              // Report result of modbus request
              util.write(target, &packet, packet.length);

              break;

            default: //
              util.logln("Unexpected multi-reg modbus read response at address 0x%x", regAddr);
              break;
          }
          break;
        }

        case FunctionCode::WriteSingleRegister: {
          uint16_t regAddr = bus.inPkt->writeSingleRegisterResponse.registerAddress;
          switch (regAddr) {
            case frequencyRegAddress:
              util.logln( //
                "node %u: wrote frequency %u, %u.%u Hz",
                bus.inPkt->nodeAddress,
                bus.inPkt->writeSingleRegisterResponse.data,
                bus.inPkt->writeSingleRegisterResponse.data / 10,
                bus.inPkt->writeSingleRegisterResponse.data % 10);

              // Only update last frequency setpoint if write succeeded.
              // Otherwise, will attempt retransmission next lap.
              lastFrequency[focus] = setFrequency[focus];

              break;

            default: //
              util.logln("Unexpected single-reg modbus write response at address 0x%x", regAddr);
              break;
          }
        }

        case FunctionCode::WriteMultipleRegisters: // not expecting anything for this yet
        case FunctionCode::Exception:              // error bit convenience

        default: //

          util.logln( //
            "node %u unexpected modbus response command 0x%x - possible exception",
            bus.outPkt->nodeAddress,
            bus.inPkt->command);

          VfdErrorDbgPinHigh();
          VfdErrorDbgPinLow();
          VfdErrorDbgPinHigh();
          // Hold to allow capture by low sample rate scope
          osDelay(1);
          VfdErrorDbgPinLow();
          break;
      }

      bus.shiftOutConsumedBytes(respLen);

    } else {
      util.logln("node %u: Unsuccessful modbus request", bus.outPkt->nodeAddress);
      VfdErrorDbgPinHigh();
      // Hold to allow capture by low sample rate scope
      osDelay(1);
      VfdErrorDbgPinLow();
    }
  }
}

size_t VfdTask::write(const void* buf, size_t len, TickType_t ticks)
{
  return msgbuf.write(buf, len, ticks);
}
