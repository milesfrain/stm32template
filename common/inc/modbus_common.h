/*
 * Common modbus types for inclusion by
 * modbus_defs.h and packets.h
 */

#pragma once

#include "basic.h"
#include <stdint.h>

// 21 functions listed in:
//   https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
// But we're only using 3 of them.
enum class FunctionCode : uint8_t
{
  ReadMultipleRegisters = 0x03,
  WriteSingleRegister = 0x06,
  WriteMultipleRegisters = 0x10,
  // Error bit
  Exception = 0x80,
};

// https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
enum class ExceptionCode : uint8_t
{
  IllegalFunction = 0x01,
  IllegalDataAddress = 0x02,
  IllegalDataValue = 0x03,
  SlaveDeviceFailure = 0x04,
  Acknowledge = 0x05,
  // We aren't planning on receiving any of the below exceptions.
  // SlaveDeviceBusy = 0x06,
  // MemoryParityError = 0x08,
  // GatewayPathUnavailable = 0x0A,
  // GatewayTargetFailedToRespond = 0x0B,
};

constexpr const char* exceptionCodeToString(ExceptionCode exceptionCode)
{
  switch (exceptionCode) {
    ENUM_STRING(ExceptionCode, IllegalFunction)
    ENUM_STRING(ExceptionCode, IllegalDataAddress)
    ENUM_STRING(ExceptionCode, IllegalDataValue)
    ENUM_STRING(ExceptionCode, SlaveDeviceFailure)
    ENUM_STRING(ExceptionCode, Acknowledge)
  }
  return "InvalidExceptionCode";
}