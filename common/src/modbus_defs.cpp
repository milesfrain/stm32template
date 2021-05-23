/*
 * See header for notes.
 */

#include "modbus_defs.h"
#include "software_crc.h"

// Prepares packet for transmit.
// Swaps endianness of packet and adds CRC.
// Assumes pkt points to a region a memory as large
// as largest possible packet.
// Returns length of packet upon success.
// Returns 0 if there's an error.
size_t modbusPreparePacketForTransmit(ModbusPacket* pkt, ModbusDirection dir)
{
  size_t len = modbusGetLengthAndSwapEndianness(pkt, dir);

  uint16_t* crcAddr = modbusCrcAddress(pkt, len);
  if (!crcAddr) {
    return 0;
  }
  *crcAddr = crc16(pkt, len - ModbusCrcSize);

  return len;
}

// Calculates modbus packet size based on contents.
// Also swaps endianness to prepare for transmit or receipt.
// Assumes pkt points to a region a memory as large
// as largest possible packet.
// Returns 0 if there's an error.
size_t modbusGetLengthAndSwapEndianness(ModbusPacket* pkt, ModbusDirection dir)
{
  // Unfortunately no 'using enum' syntax available (C++ 20 only)
  switch (dir) {

    case ModbusDirection::Request: {
      switch (pkt->command) {

        case FunctionCode::ReadMultipleRegisters:
          invert16(&pkt->readMultipleRegistersRequest.startingAddress);
          invert16(&pkt->readMultipleRegistersRequest.numRegisters);
          return ModbusHeaderAndCrcSize + sizeof(ModbusPacket::readMultipleRegistersRequest);

        case FunctionCode::WriteSingleRegister:
          invert16(&pkt->writeSingleRegisterRequest.registerAddress);
          invert16(&pkt->writeSingleRegisterRequest.data);
          return ModbusHeaderAndCrcSize + sizeof(ModbusPacket::writeSingleRegisterRequest);

        case FunctionCode::WriteMultipleRegisters: {
          uint16_t* numRegisters = &pkt->writeMultipleRegistersRequest.numRegisters;
          // check if num registers is out of bounds or mismatches with numBytes
          if (*numRegisters < minWriteRegisters || //
              *numRegisters > maxWriteRegisters || //
              *numRegisters * 2 != pkt->writeMultipleRegistersRequest.numBytes) {
            return 0;
          }

          for (int i = 0; i < *numRegisters; i++) {
            invert16(pkt->writeMultipleRegistersRequest.payload + i);
          }

          invert16(&pkt->writeMultipleRegistersRequest.startingAddress);
          // Wait to invert numRegisters until after we are done using it for looping
          invert16(&pkt->writeMultipleRegistersRequest.numRegisters);

          return                                                            //
            offsetof(ModbusPacket, writeMultipleRegistersRequest.payload) + //
            pkt->writeMultipleRegistersRequest.numBytes +                   //
            ModbusCrcSize;
        }

        default: return 0;
      }
    }

    case ModbusDirection::Response: {
      switch (pkt->command) {

        case FunctionCode::ReadMultipleRegisters: {
          uint8_t& numBytes = pkt->readMultipleRegistersResponse.numBytes;
          // check if num bytes is out of bounds or odd
          if (numBytes < minReadBytes || //
              numBytes > maxReadBytes || //
              numBytes % 2) {
            return 0;
          }

          for (int i = 0; i < numBytes / 2; i++) {
            invert16(pkt->readMultipleRegistersResponse.payload + i);
          }

          return                                                            //
            offsetof(ModbusPacket, readMultipleRegistersResponse.payload) + //
            pkt->readMultipleRegistersResponse.numBytes +                   //
            ModbusCrcSize;
        }

        case FunctionCode::WriteSingleRegister:
          invert16(&pkt->writeSingleRegisterResponse.registerAddress);
          invert16(&pkt->writeSingleRegisterResponse.data);
          return ModbusHeaderAndCrcSize + sizeof(ModbusPacket::writeSingleRegisterRequest);

        case FunctionCode::WriteMultipleRegisters:
          invert16(&pkt->writeMultipleRegistersResponse.startingAddress);
          invert16(&pkt->writeMultipleRegistersResponse.numRegisters);
          return ModbusHeaderAndCrcSize + sizeof(ModbusPacket::writeMultipleRegistersResponse);

        default:
          // Check if exception.
          // Will be checked more thoroughly later
          if ((uint8_t)pkt->command & (uint8_t)FunctionCode::Exception) {
            return ModbusExceptionPktSize;
          } else {
            return 0;
          }
      }
    }

    default: return 0;
  }
}

// Returns a pointer to the packet's CRC address
// Assumes that pkt pointer points a region of memory that can
// accomidate the largest packet (even that region is only partially
// filled with fresh data.
// On error, returns 0.
uint16_t* modbusCrcAddress(const ModbusPacket* pkt, size_t len)
{
  if (len < ModbusHeaderAndCrcSize) {
    return (uint16_t*)0;
  }

  return (uint16_t*)((size_t)pkt + len - ModbusCrcSize);
}

bool modbusValidCrc(const ModbusPacket* pkt, size_t len)
{
  return *modbusCrcAddress(pkt, len) == crc16(pkt, len - ModbusCrcSize);
}

// Determines the "Response" packet length for a given "Request" packet.
// Assumes pkt has already been validated.
// Returns length of packet upon success.
// Returns 0 if there's an error.
size_t modbusExpectedResponseLength(ModbusPacket* pkt)
{
  switch (pkt->command) {

    case FunctionCode::ReadMultipleRegisters:
      // One of those situations where limited use of magic numbers improves readability.
      return ModbusHeaderAndCrcSize + 1 + 2 * pkt->readMultipleRegistersRequest.numRegisters;

    case FunctionCode::WriteSingleRegister: //
      return ModbusHeaderAndCrcSize + sizeof(ModbusPacket::writeSingleRegisterResponse);

    case FunctionCode::WriteMultipleRegisters: //
      return ModbusHeaderAndCrcSize + sizeof(ModbusPacket::writeMultipleRegistersResponse);

    default: return 0;
  }
}
