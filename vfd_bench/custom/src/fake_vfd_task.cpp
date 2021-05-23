/*
 * See header for notes.
 */

#include "fake_vfd_task.h"
#include "modbus_defs.h"
#include "packets.h"
#include "vfd_defs.h"

FakeVfdTask::FakeVfdTask( //
  const char* name,
  UartTasks& uart,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : uart{ uart }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

// Fake VFDs at all addresses (including broadcast at address 0)

// Copies data from buffer (big endian) to modbus packet (little endian).
// Returns number of bytes consumed.
// Returns 0 if not enough bytes available.
// Returns -1 if parsing error (bad packet).
int32_t parseRequestToServer(uint8_t* buf, size_t bufLen, ModbusPacket* pkt)
{
  // Check that we at least have access to the command field
  if (bufLen <= ModbusHeaderAndCrcSize) {
    // Not enough bytes to process
    return 0;
  }

  switch (((ModbusPacket*)buf)->command) {

    case FunctionCode::ReadMultipleRegisters: {
      size_t requiredLen = ModbusHeaderAndCrcSize + sizeof(ModbusPacket::readMultipleRegistersRequest);
      if (bufLen < requiredLen) {
        return 0;
      }

      memcpy(pkt, buf, requiredLen);

      invert16(&pkt->readMultipleRegistersRequest.startingAddress);
      invert16(&pkt->readMultipleRegistersRequest.numRegisters);

      return requiredLen;
    }

    case FunctionCode::WriteSingleRegister: {
      size_t requiredLen = ModbusHeaderAndCrcSize + sizeof(ModbusPacket::writeSingleRegisterRequest);
      if (bufLen < requiredLen) {
        return 0;
      }

      memcpy(pkt, buf, requiredLen);

      invert16(&pkt->writeSingleRegisterRequest.registerAddress);
      invert16(&pkt->writeSingleRegisterRequest.data);

      return requiredLen;
    }

    case FunctionCode::WriteMultipleRegisters: {
      size_t requiredLen = ModbusHeaderAndCrcSize + offsetof(ModbusPacket, writeMultipleRegistersRequest.payload);

      if (bufLen < requiredLen) {
        return 0;
      }

      uint8_t numBytes = ((ModbusPacket*)buf)->writeMultipleRegistersRequest.numBytes;

      if (numBytes < minWriteBytes || numBytes > maxWriteBytes) {
        return -1;
      }

      requiredLen += numBytes;

      if (bufLen < requiredLen) {
        return 0;
      }

      memcpy(pkt, buf, requiredLen);

      // Modbus data sent over the wire as big-endian,
      // but uC architecture is little-endian, so invert to correct
      // endianness for looping as modbus server.
      invert16(&pkt->writeMultipleRegistersRequest.numRegisters);
      uint16_t numRegisters = pkt->writeMultipleRegistersRequest.numRegisters;

      // Check if num registers mismatches with numBytes
      if (numBytes != numRegisters * 2) {
        return -1;
      }

      for (uint16_t i = 0; i < numRegisters; i++) {
        invert16(pkt->writeMultipleRegistersRequest.payload + i);
      }

      invert16(&pkt->writeMultipleRegistersRequest.startingAddress);

      return requiredLen;
    }

    default: return -1;
  }
}

void FakeVfdTask::func()
{
  // Number of skipped-over bytes due to parsing errors in current buffer
  size_t index = 0;

  util.watchdogRegisterTask();

  // As modbus server, keep trying to read and parse
  while (1) {

    util.watchdogKick();

    // Block until there's new data to read
    size_t readLen = util.read(uart, inBuf + inLen, sizeof(inBuf) - inLen);

#ifdef MODBUS_REQUEST_ECHOING_ENABLED
    // Echo incoming bytes (simulates transceiver echo)
    util.write(uart, inBuf + inLen, readLen);
    // Note that this simulated echo is sent after some delay,
    // rather than being simultaneous with the request, as is the
    // case with the real hardware echo.
    // So if echo duration is longer than responseDelayMs, then
    // echo and response will be squished together as one contiguous
    // sequence of bytes, which will fail modbus protocol decoding on
    // logic analyzer, but will be successfully decoded by our more
    // permissive modbus client.
#endif // MODBUS_REQUEST_ECHOING_ENABLED
    const uint32_t fakeResponseDelayMs = responseDelayMs - 2;

    inLen += readLen;

    // Keep attempting to match packet while there are enough bytes remaining
    while (inLen >= ModbusHeaderAndCrcSize + index) {
      // Attempt to match packet
      int32_t parsedLen = parseRequestToServer(inBuf + index, inLen - index, &inPkt);

      if (parsedLen == -1) {
        // Discard first byte
        index++;
        util.warnln("Parsing error");
        continue;
      }

      if (parsedLen == 0) {
        // Should encounter for HT or TC
        // util.warnln("Not enough bytes");
        break;
      }

      // Check CRC
      // Must run CRC check on original (big) endianness from wire
      if (modbusValidCrc((ModbusPacket*)(inBuf + index), parsedLen)) {
        // Handle command
        switch (inPkt.command) {
          case FunctionCode::ReadMultipleRegisters:
            // Only expecting status read
            if (inPkt.readMultipleRegistersRequest.startingAddress == statusRegAddress && //
                inPkt.readMultipleRegistersRequest.numRegisters == statusRegNum) {
              // Send back simulated data
              VfdStatus* status = (VfdStatus*)&(outPkt.readMultipleRegistersResponse.payload);

              // Set header fields
              outPkt.nodeAddress = inPkt.nodeAddress;
              outPkt.command = inPkt.command;
              outPkt.readMultipleRegistersResponse.numBytes = sizeof(status->payload);

              static_assert(sizeof(status->payload) == statusRegNum * 2);

              // Default to all 0x55 payload
              memset(&status->payload, 0x55, sizeof(status->payload));
              // Only set a few simulated fields
              status->payload.freqCmd = frequencies[inPkt.nodeAddress];
              status->payload.freqOut = frequencies[inPkt.nodeAddress];

              // Prepare response to send on wire
              size_t outLen = modbusPreparePacketForTransmit(&outPkt, ModbusDirection::Response);
              if (outLen == 0) {
                util.warnln("Error preparing status response");
              } else {
                osDelay(fakeResponseDelayMs);
                // Send response
                util.write(uart, &outPkt, outLen);
              }
            } else {
              util.warnln("Unexpected readMultipleRegistersRequest contents");
            }
            break;

          case FunctionCode::WriteSingleRegister:
            // Only expecting frequency write
            if (inPkt.writeSingleRegisterRequest.registerAddress == frequencyRegAddress) {
              if (inPkt.nodeAddress == 0) {
                // Set all frequencies if broadcast address
                for (size_t i = 0; i < sizeof(frequencies) / sizeof(*frequencies); i++) {
                  frequencies[i] = inPkt.writeSingleRegisterRequest.data;
                }
                // Don't send a response for broadcast
              } else {
                // Only set frequency at address for non-broadcast
                frequencies[inPkt.nodeAddress] = inPkt.writeSingleRegisterRequest.data;

                // Send a response
                // outPkt is same as inPkt for single register write
                memcpy(&outPkt, &inPkt, parsedLen);

                // Prepare response to send on wire
                size_t outLen = modbusPreparePacketForTransmit(&outPkt, ModbusDirection::Response);
                if (outLen != (size_t)parsedLen) {
                  util.warnln("parsedLen %d mismatches outLen %d", parsedLen, outLen);
                } else {
                  osDelay(fakeResponseDelayMs);
                  // Send response
                  util.write(uart, &outPkt, outLen);
                }
              }
            } else {
              util.warnln("Unexpected writeSingleRegisterRequest contents");
            }
            break;

          default: util.warnln("Unexpected function code %d", inPkt.command); break;
        }
      } else {
        util.warnln("Invalid crc");
      }

      // Discard consumed bytes
      size_t totalConsumed = index + parsedLen;
      if (inLen > totalConsumed) {
        // Shift remaining bytes to beginning of buffer for next round of parsing
        memmove(inBuf, inBuf + totalConsumed, totalConsumed);
      }

      inLen -= totalConsumed;
      index = 0;
    }
  }
}
