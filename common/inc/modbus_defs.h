/*
 * Modbus packet definitions and helper functions
 */

#pragma once

#include "basic.h"
#include "modbus_common.h"
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

// Enable this define if nRE on the transceiver is active (tied to ground).
// Disable this define if nRE is tied to DE.
#define MODBUS_REQUEST_ECHOING_ENABLED

enum class ModbusDirection
{
  Request,
  Response,
};

const uint16_t minReadRegisters = 1;
const uint16_t maxReadRegisters = 125;
const uint8_t minReadBytes = minReadRegisters * 2;
const uint8_t maxReadBytes = maxReadRegisters * 2;

const uint16_t minWriteRegisters = 1;
const uint16_t maxWriteRegisters = 123; // why not 125?
const uint8_t minWriteBytes = minWriteRegisters * 2;
const uint8_t maxWriteBytes = maxWriteRegisters * 2;

struct ModbusPacket
{
  uint8_t nodeAddress;
  // MSB is set upon error
  FunctionCode command;
  union
  {
    // ============== Requests ==============

    struct
    {
      uint16_t startingAddress;
      // 1 to 125
      uint16_t numRegisters;
    } readMultipleRegistersRequest;

    struct
    {
      uint16_t registerAddress;
      uint16_t data;
    } writeSingleRegisterRequest;

    // packing to eliminate 1 byte of padding after numBytes
    struct __attribute__((__packed__))
    {
      uint16_t startingAddress;
      // 1 to 123
      uint16_t numRegisters;
      // Well, this seems unnecessary:
      // https://electronics.stackexchange.com/questions/265729/is-the-information-in-modbus-function-code-16-redundant
      uint8_t numBytes;
      uint16_t payload[maxReadRegisters];
    } writeMultipleRegistersRequest;

    // ============== Responses ==============

    // packing to eliminate 1 byte of padding after numBytes
    struct __attribute__((__packed__))
    {
      // Note that request specifies words (2 bytes each)
      // but response specifies bytes (2 per word).
      // Should be even.
      uint8_t numBytes;
      uint16_t payload[maxReadRegisters];
    } readMultipleRegistersResponse;

    struct
    {
      uint16_t registerAddress;
      uint16_t data;
    } writeSingleRegisterResponse;

    struct
    {
      uint16_t startingAddress;
      // 1 to 123
      uint16_t numRegisters;
    } writeMultipleRegistersResponse;

    ExceptionCode exceptionCode;
  };
};

// Includes CRC
const size_t MaxModbusPktSize = sizeof(ModbusPacket) + sizeof(uint16_t);

// nodeAddress and command fields
const size_t ModbusHeaderSize = offsetof(ModbusPacket, readMultipleRegistersRequest);
static_assert(ModbusHeaderSize == 2);

const size_t ModbusCrcSize = sizeof(uint16_t);
static_assert(ModbusCrcSize == 2);

const size_t ModbusHeaderAndCrcSize = ModbusHeaderSize + ModbusCrcSize;
static_assert(ModbusHeaderAndCrcSize == 4);

const size_t ModbusExceptionPktSize = offsetof(ModbusPacket, exceptionCode) + //
                                      sizeof(ModbusPacket::exceptionCode) + ModbusCrcSize;
static_assert(ModbusExceptionPktSize == 5);

static_assert(offsetof(ModbusPacket, readMultipleRegistersResponse.payload) == //
              ModbusHeaderSize + sizeof(ModbusPacket::readMultipleRegistersResponse.numBytes));

// Reverses endianness of 16-bit values.
// In-place mutation.
inline void invert16(uint16_t* x)
{
  *x = __builtin_bswap16(*x);
}

size_t modbusPreparePacketForTransmit(ModbusPacket* pkt, ModbusDirection dir);

size_t modbusGetLengthAndSwapEndianness(ModbusPacket* pkt, ModbusDirection dir);

uint16_t* modbusCrcAddress(const ModbusPacket* pkt, size_t len);

bool modbusValidCrc(const ModbusPacket* pkt, size_t len);

size_t modbusExpectedResponseLength(ModbusPacket* pkt);
