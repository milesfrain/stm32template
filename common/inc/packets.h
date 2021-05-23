#pragma once

#include "basic.h"
#include "modbus_common.h"
#include <stddef.h> // size_t
#include <stdint.h>

const uint32_t startWord = 0xFEEDABBE;

enum class PacketID : uint32_t
{
  LogMessage = 0,
  Heartbeat,
  ParsingErrorInvalidLength,
  ParsingErrorInvalidCRC,
  ParsingErrorInvalidID,
  ParsingErrorInvalidSequence,
  ParsingErrorDroppedBytes,
  WatchdogTimeout,
  VfdSetFrequency,
  VfdStatus,
  ModbusError,
  DummyPacket,
  NumIDs,
};

enum class PacketOrigin : uint32_t
{
  Internal = 0,
  HostToTarget,
  TargetToHost,
  HostToMonitor,
  MonitorToAscii,
  UnitTest,
  TargetTest,
  NumOrigins,
};

// -------------

const uint32_t maxLogMsgLength = 256;

struct LogMessage
{
  uint32_t length;
  char msg[maxLogMsgLength];
};

// ----------

union ParsingError
{
  // Thrown internally if length is too small or large
  uint32_t invalidLength;

  // Thrown internally if CRC does not match
  struct
  {
    uint32_t provided;
    uint32_t calculated;
  } invalidCRC;

  // Thrown internally if packet ID is out of range
  uint32_t invalidID;

  /*
   * Thrown whenever a sequence number is out of "sequence".
   * Thrown once whenever a group of packets is skipped.
   * 		1
   * 		2
   * 		4 <- error
   * 		5
   * 		6
   *
   * Thrown multiple times whenever there's an ordering issue, since
   * the new "unexpected" number becomes the "previous" number.
   * 		1
   * 		2
   * 		4 <- error
   * 		3 <- error
   * 		5 <- error
   * 		6
   *
   */
  struct
  {
    uint32_t provided;
    uint32_t expected;
  } invalidSequence;

  // Thrown internally whenever bytes must be discarded
  uint32_t droppedBytes;
};

struct WatchdogTimeout
{
  uint32_t unresponsiveTicks;
  // Name length should match configMAX_TASK_NAME_LEN,
  // but can't include FreeRTOSConfig.h,
  // so hardcoding here and asserting in watchdog_task.h
  char name[16];
};

// Sent from PC to set VFD frequency
struct VfdSetFrequency
{
  uint8_t node;
  uint16_t frequency;
};

// Sent from uC to PC
// Contents of status registers starting at modbus address 48449
struct __attribute__((__packed__)) VfdStatus
{
  struct
  {
    uint16_t error;
    uint16_t state; // could pretty-print or just dump masked hex/bin
    uint16_t freqCmd;
    uint16_t freqOut;
    uint16_t currentOut;
    uint16_t dcBusVoltage;       // Don't really care about this, should be constant
    uint16_t motorOutputVoltage; // Seems like this would be constant
    uint16_t rpm;                // Might be different than freqOut * 60
  } payload;

  uint8_t nodeAddress;
};

// Dummy packet for testing
struct DummyPacket
{
  uint32_t outId;
  uint8_t payload[64];
};

enum class ModbusErrorID : uint16_t
{
  BadEchoNotEnoughBytes,
  BadEchoMismatchedContents,
  BadResponseNotEnoughBytes,
  BadResponseMalformedPacket,
  ResponseException,
  ExtraBytes,
};

struct __attribute__((__packed__)) ModbusError
{
  ModbusErrorID id;
  uint8_t node;
  FunctionCode command;
  // These additional union fields are only used for some IDs.
  union
  {
    struct
    {
      uint32_t actual;
      uint32_t expected;
    } bytes;
    ExceptionCode exceptionCode;
  };
};

// Packet
struct Packet
{
  uint32_t length;      // The length of the entire packet, not including wrapper
  uint32_t sequenceNum; // Incrementing number to check for dropped packets
  // Note that sequence numbers are re-applied before transmitting,
  // so numbers noted in invalidSequence error are only accurate from the
  // perspective of the device generating that error packet.
  PacketOrigin origin;
  PacketID id;
  union
  {
    LogMessage logMessage;
    ParsingError parsingError;
    WatchdogTimeout watchdogTimeout;
    VfdSetFrequency vfdSetFrequency;
    VfdStatus vfdStatus;
    ModbusError modbusError;
    DummyPacket dummy;
  } body;
  // Would be nicer to omit 'body' so this could be an anonymous union
  // which makes it a bit more convenient to use, but then can't calculate
  // minimum packet sizes.
};

// Todo - reduce field sizes and convert to architecture-independent packed struct
struct WrappedPacket
{
  uint32_t magicStart; // Helps with faster re-syncing than re-calculating full crc at each new offset.
  uint32_t crc;
  Packet packet;
};

const uint32_t maxWrappedPacketLength = sizeof(WrappedPacket);
const uint32_t minWrappedPacketLength = sizeof(WrappedPacket) - sizeof(WrappedPacket::packet.body);
const uint32_t minPacketLength = sizeof(Packet) - sizeof(Packet::body);
const uint32_t wrapperLength = sizeof(WrappedPacket) - sizeof(WrappedPacket::packet);

// --------

// Size of just the unique packet body
constexpr uint32_t packetBodySizeFromID(PacketID id)
{
  switch (id) {
    case PacketID::LogMessage: {
      return sizeof(Packet::body.logMessage); // maximum size
    }
    case PacketID::Heartbeat: {
      return 0;
    }
    case PacketID::ParsingErrorInvalidLength: {
      return sizeof(Packet::body.parsingError.invalidLength);
    }
    case PacketID::ParsingErrorInvalidCRC: {
      return sizeof(Packet::body.parsingError.invalidCRC);
    }
    case PacketID::ParsingErrorInvalidID: {
      return sizeof(Packet::body.parsingError.invalidID);
    }
    case PacketID::ParsingErrorInvalidSequence: {
      return sizeof(Packet::body.parsingError.invalidSequence);
    }
    case PacketID::ParsingErrorDroppedBytes: {
      return sizeof(Packet::body.parsingError.droppedBytes);
    }
    case PacketID::WatchdogTimeout: {
      return sizeof(Packet::body.watchdogTimeout);
    }
    case PacketID::VfdSetFrequency: {
      return sizeof(Packet::body.vfdSetFrequency);
    }
    case PacketID::VfdStatus: {
      return sizeof(Packet::body.vfdStatus);
    }
    case PacketID::ModbusError: {
      // Not attempting to optimize packet size for IDs that don't
      // require additional union fields (e.g. BadResponseMalformedPacket)
      return sizeof(Packet::body.modbusError);
    }
    case PacketID::DummyPacket: {
      return sizeof(Packet::body.dummy);
    }
    case PacketID::NumIDs: // This should not be called
    {
      return -1;
    }
  }
  // This is never reached
  // but compiler is not smart enough to know that, and demands a return
  return -1;
}

// Size of packet body plus common packet field, but excluding wrapper fields
constexpr uint32_t packetSizeFromID(PacketID id)
{
  return minPacketLength + packetBodySizeFromID(id);
}

// Size of the entire wrapped packet
constexpr uint32_t wrappedPacketSizeFromID(PacketID id)
{
  return minWrappedPacketLength + packetBodySizeFromID(id);
}

// Size of the wrapped packet (assuming length field is set correctly)
inline uint32_t wrappedPacketSize(WrappedPacket& wrap)
{
  return wrapperLength + wrap.packet.length;
}

constexpr const char* packetIdToString(PacketID id)
{
  switch (id) {
    ENUM_STRING(PacketID, LogMessage)
    ENUM_STRING(PacketID, Heartbeat)
    ENUM_STRING(PacketID, ParsingErrorInvalidLength)
    ENUM_STRING(PacketID, ParsingErrorInvalidCRC)
    ENUM_STRING(PacketID, ParsingErrorInvalidID)
    ENUM_STRING(PacketID, ParsingErrorInvalidSequence)
    ENUM_STRING(PacketID, ParsingErrorDroppedBytes)
    ENUM_STRING(PacketID, WatchdogTimeout)
    ENUM_STRING(PacketID, VfdSetFrequency)
    ENUM_STRING(PacketID, VfdStatus)
    ENUM_STRING(PacketID, ModbusError)
    ENUM_STRING(PacketID, DummyPacket)
    ENUM_STRING(PacketID, NumIDs)
  }
  return "InvalidID";
}

constexpr const char* packetOriginToString(PacketOrigin origin)
{
  switch (origin) {
    ENUM_STRING(PacketOrigin, Internal)
    ENUM_STRING(PacketOrigin, HostToTarget)
    ENUM_STRING(PacketOrigin, TargetToHost)
    ENUM_STRING(PacketOrigin, HostToMonitor)
    ENUM_STRING(PacketOrigin, MonitorToAscii)
    ENUM_STRING(PacketOrigin, UnitTest)
    ENUM_STRING(PacketOrigin, TargetTest)
    ENUM_STRING(PacketOrigin, NumOrigins)
  }
  return "InvalidOrigin";
}

constexpr const char* modbusErrorIdToString(ModbusErrorID id)
{
  switch (id) {
    ENUM_STRING(ModbusErrorID, BadEchoNotEnoughBytes)
    ENUM_STRING(ModbusErrorID, BadEchoMismatchedContents)
    ENUM_STRING(ModbusErrorID, BadResponseNotEnoughBytes)
    ENUM_STRING(ModbusErrorID, BadResponseMalformedPacket)
    ENUM_STRING(ModbusErrorID, ResponseException)
    ENUM_STRING(ModbusErrorID, ExtraBytes)
  }
  return "InvalidID";
};
