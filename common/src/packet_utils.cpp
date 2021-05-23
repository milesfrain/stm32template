/*
 * Functions for working with packets
 */

#include "packet_utils.h"
#include "basic.h"
#include "software_crc.h"
#include <stdio.h>  // fwrite
#include <string.h> // memmove
#include <unistd.h> // write

#ifdef HOST_APP
#define println(format, ...) printf(format "\n", ##__VA_ARGS__)
#else
#define println(format, ...) (void)0
#endif

/*
 * Helper-function for pre-populating some packet fields.
 * Sets:
 * 	- origin
 * 	- ID
 * 	- Minimum length
 * 	  - Must increment after adding data to variable-length field.
 */
void initializePacket(Packet& packet, PacketID id)
{
  packet.origin = PacketOrigin::Internal;
  packet.id = id;
  packet.length = packetSizeFromID(id);
}

// A version of initializePacket() that just sets id and length fields
void setPacketIdAndLength(Packet& packet, PacketID id)
{
  packet.id = id;
  packet.length = packetSizeFromID(id);
}

/*
 * Sets wrapper fields
 */
WrappedPacket& setPacketWrapper(WrappedPacket& wrap)
{
  wrap.magicStart = startWord;
  wrap.crc = crc32(reinterpret_cast<const uint8_t*>(&wrap.packet), wrap.packet.length);
  return wrap;
}

/*
 * More helper functions for populating packets
 */

void fillFreqPacket(WrappedPacket& wrap, uint32_t seq, uint32_t node, uint32_t freq)
{
  initializePacket(wrap.packet, PacketID::VfdSetFrequency);
  wrap.packet.sequenceNum = seq;
  wrap.packet.body.vfdSetFrequency.node = node;
  wrap.packet.body.vfdSetFrequency.frequency = freq;
  setPacketWrapper(wrap);
}

void fillLengthErrorPacket(WrappedPacket& wrap, uint32_t len)
{
  initializePacket(wrap.packet, PacketID::ParsingErrorInvalidLength);
  wrap.packet.body.parsingError.invalidLength = len;
  setPacketWrapper(wrap);
}

void fillDropErrorPacket(WrappedPacket& wrap, uint32_t drop)
{
  initializePacket(wrap.packet, PacketID::ParsingErrorDroppedBytes);
  wrap.packet.body.parsingError.droppedBytes = drop;
  setPacketWrapper(wrap);
}

// Same as memcpy, but returns length of bytes written
uint32_t mymemcpy(void* dst, const void* src, uint32_t len)
{
  memcpy(dst, src, len);
  return len;
}

// Writes the entire wrapped packet to a file.
// Returns 1 if successfully written.
size_t fwriteWrapped(FILE* fp, WrappedPacket& wrap)
{
  return fwrite(&wrap, wrapperLength + wrap.packet.length, 1, fp);
}

// Writes the entire wrapped packet to a file.
// Returns -1 for error.
ssize_t writeWrapped(int fd, WrappedPacket& wrap)
{
  return write(fd, &wrap, wrapperLength + wrap.packet.length);
}

// Copies the entire wrapped packet to a buffer
uint32_t copyWrapped(void* buf, WrappedPacket& wrap)
{
  return mymemcpy(buf, &wrap, wrapperLength + wrap.packet.length);
}

// Copies just the inner unwrapped packet to a buffer
uint32_t copyInner(void* buf, WrappedPacket& wrap)
{
  return mymemcpy(buf, &wrap.packet, wrap.packet.length);
}

/*
 * Takes a buffer and its length.
 * Looks for any complete packets.
 * Sends these complete packets to processPacket callback.
 * Moves remaining unprocessed bytes to beginning of buf.
 * Returns index of next free spot in buf, for example:
 *   - Returns 0 if ALL bytes processed.
 *   - Returns len if NO bytes processed.
 *   - Returns n where n is the number of trailing bytes
 *   	(shifted to the start of the buffer) that have
 *   	not yet formed a completed Packet.
 * If parsing errors occur, will generate and send one or more
 *  of the following error packets to processPacket callback.
 *  These packets will have sequence number 0, which indicates they are
 *  generated internally.
 *
 *  Todos to think about:
 *  	Additional error packet allocated locally.
 *  	If we made a packetExtractor object, could statically allocate
 *  	the errorPacket.
 *  	Could have sequence number history too, to enable that error message.
 *  	There will likely need to be more than one of these extractors, so
 *  	can't just do non-object static.
 */
uint32_t PacketParser::extractPackets(void* bufArg, uint32_t len)
{
  uint32_t offset = 0;
  uint32_t skippedBytes = 0;
  uint8_t* buf = (uint8_t*)bufArg;

  // Keep checking while there are still enough bytes to read
  while (len >= offset + minWrappedPacketLength) {
    // Check if this is a valid packet

    auto wrap = (WrappedPacket*)(buf + offset);
    Packet& packet = wrap->packet;

    // Check for start word
    if (wrap->magicStart != startWord) {
      // Mismatch, try next byte
      offset++;
      skippedBytes++;
      continue;
    }

    // Check if length is reasonable
    if (packet.length < minPacketLength || //
        packet.length > sizeof(Packet)) {
      // Report length mismatch
      initializePacket(errorPacket, PacketID::ParsingErrorInvalidLength);
      errorPacket.body.parsingError.invalidLength = packet.length;
      processer.processPacket(errorPacket);

      // Mismatch, try next byte
      offset++;
      skippedBytes++;
      continue;
    }

    // Check if id is reasonable
    if (packet.id >= PacketID::NumIDs) {
      // Report id mismatch
      initializePacket(errorPacket, PacketID::ParsingErrorInvalidID);
      errorPacket.body.parsingError.invalidID = static_cast<uint32_t>(packet.id);
      processer.processPacket(errorPacket);

      // Mismatch, try next byte
      offset++;
      skippedBytes++;
      continue;
    }

    // Check if we have enough bytes of data for this packet
    if (len < offset + wrapperLength + packet.length) {
      // This is just an informative print, rather than an error.
      // println("Waiting for remaining bytes of packet. Have %d, need %d.",
      //   len, offset + wrapperLength + packet.length);

      // We likely have an incomplete packet.
      // Need more data to be sure, so done with parsing for now.
      // Should hopefully receive the rest of the packet on next parsing call.
      break;
    }

    // Check crc
    uint32_t calculatedCRC = crc32(reinterpret_cast<const uint8_t*>(&packet), packet.length);
    if (calculatedCRC != wrap->crc) {
      // Report crc mismatch
      initializePacket(errorPacket, PacketID::ParsingErrorInvalidCRC);
      errorPacket.body.parsingError.invalidCRC.provided = wrap->crc;
      errorPacket.body.parsingError.invalidCRC.calculated = calculatedCRC;
      processer.processPacket(errorPacket);

      // Mismatch, try next byte
      offset++;
      skippedBytes++;
      continue;
    }

    // We have a valid packet

    // First, report how many bytes we had to skip (if any)
    if (skippedBytes) {
      // Report skipped bytes
      initializePacket(errorPacket, PacketID::ParsingErrorDroppedBytes);
      errorPacket.body.parsingError.droppedBytes = skippedBytes;
      processer.processPacket(errorPacket);

      // Reset skipped bytes
      skippedBytes = 0;
    }

    // Check if sequence number is out of order
    if (packet.sequenceNum != lastSeqNum + 1) {
      // Report unexpected sequence number
      initializePacket(errorPacket, PacketID::ParsingErrorInvalidSequence);
      errorPacket.body.parsingError.invalidSequence.provided = packet.sequenceNum;
      errorPacket.body.parsingError.invalidSequence.expected = lastSeqNum + 1;
      processer.processPacket(errorPacket);
    }

    lastSeqNum = packet.sequenceNum;

    // Send valid packet to callback

    processer.processPacket(packet);

    // Adjust offset
    offset += wrapperLength + packet.length;
  }

  // Do a final reporting of skipped bytes
  if (skippedBytes) {
    // Report skipped bytes
    initializePacket(errorPacket, PacketID::ParsingErrorDroppedBytes);
    errorPacket.body.parsingError.droppedBytes = skippedBytes;
    processer.processPacket(errorPacket);
  }

  // Shift out any consumed bytes
  if (offset) {
    len -= offset;
    // Move leftover bytes to beginning of buffer
    memmove(buf, buf + offset, len);
  }

  // Return number of leftover bytes
  return len;
}

/*
 * Returns rewrapped packet with updated sequence number.
 * Updates internally-tracked sequence number.
 */
WrappedPacket& PacketSequencer::rewrap(WrappedPacket& wrap)
{
  wrap.packet.sequenceNum = num++;
  return setPacketWrapper(wrap);
}

/*
 * Generates human-readable string for packets
 */
uint32_t snprintPacket(char* buf, uint32_t len, const Packet& packet)
{
  uint32_t n = 0;
  n += snprintf(buf + n,
                len - n, //
                "Sequence %u, Origin %u %s, ID %u %s: ",
                packet.sequenceNum,
                static_cast<uint32_t>(packet.origin),
                packetOriginToString(packet.origin),
                static_cast<uint32_t>(packet.id),
                packetIdToString(packet.id));

  switch (packet.id) {
    case PacketID::LogMessage: {
      return snprintf(buf + n,
                      len - n, //
                      "%s",
                      packet.body.logMessage.msg);
    }
    case PacketID::Heartbeat: return n;
    case PacketID::ParsingErrorInvalidLength: {
      return n + snprintf(buf + n,
                          len - n, //
                          "%u",
                          packet.body.parsingError.invalidLength);
    }
    case PacketID::ParsingErrorInvalidCRC: {
      return n + snprintf(buf + n,
                          len - n, //
                          "provided 0x%08X, calculated 0x%08X",
                          packet.body.parsingError.invalidCRC.provided,
                          packet.body.parsingError.invalidCRC.calculated);
    }
    case PacketID::ParsingErrorInvalidID: {
      return n + snprintf(buf + n,
                          len - n, //
                          "%u",
                          packet.body.parsingError.invalidID);
    }
    case PacketID::ParsingErrorInvalidSequence: {
      return n + snprintf(buf + n,
                          len - n, //
                          "provided %u, expected %u",
                          packet.body.parsingError.invalidSequence.provided,
                          packet.body.parsingError.invalidSequence.expected);
    }
    case PacketID::ParsingErrorDroppedBytes: {
      return n + snprintf(buf + n,
                          len - n, //
                          "%u",
                          packet.body.parsingError.droppedBytes);
    }
    case PacketID::WatchdogTimeout: {
      n += snprintf(buf + n, //
                    min<size_t>(len - n, sizeof(WatchdogTimeout::name) + 1),
                    "%s",
                    packet.body.watchdogTimeout.name);
      return n + snprintf(buf + n, //
                          len - n,
                          " unresponsive for %d ticks",
                          packet.body.watchdogTimeout.unresponsiveTicks);
    }
    case PacketID::VfdSetFrequency: {
      return n + snprintf(buf + n,
                          len - n, //
                          "node %u, frequency %u.%u Hz",
                          packet.body.vfdSetFrequency.node,
                          packet.body.vfdSetFrequency.frequency / 10,
                          packet.body.vfdSetFrequency.frequency % 10);
    }
    case PacketID::VfdStatus: {
      // todo conversions and improve readability
      return n + snprintf(buf + n,
                          len - n, //
                          "node %u"
                          " error 0x%04X,"
                          " state 0x%04X,"
                          " freqCmd %u.%u Hz,"
                          " freqOut %u.%u Hz,"
                          " currentOut %u A,"
                          " dcBusVoltage %u.%u V,"
                          " motorOutputVoltage %u.%u V,"
                          " rpm %u",
                          packet.body.vfdStatus.nodeAddress,
                          packet.body.vfdStatus.payload.error,
                          packet.body.vfdStatus.payload.state,
                          packet.body.vfdStatus.payload.freqCmd / 10,
                          packet.body.vfdStatus.payload.freqCmd % 10,
                          packet.body.vfdStatus.payload.freqOut / 10,
                          packet.body.vfdStatus.payload.freqOut % 10,
                          packet.body.vfdStatus.payload.currentOut,
                          packet.body.vfdStatus.payload.dcBusVoltage / 10,
                          packet.body.vfdStatus.payload.dcBusVoltage % 10,
                          packet.body.vfdStatus.payload.motorOutputVoltage / 10,
                          packet.body.vfdStatus.payload.motorOutputVoltage % 10,
                          packet.body.vfdStatus.payload.rpm);
    }
    case PacketID::ModbusError: {
      n += snprintf(buf + n, //
                    len - n,
                    "node %u, cmd 0x%x, %s: ",
                    packet.body.modbusError.node,
                    (uint8_t)packet.body.modbusError.command,
                    modbusErrorIdToString(packet.body.modbusError.id));

      switch (packet.body.modbusError.id) {
        case ModbusErrorID::BadEchoNotEnoughBytes:
        case ModbusErrorID::BadResponseNotEnoughBytes:
        case ModbusErrorID::ExtraBytes:
          return n + snprintf(buf + n, //
                              len - n,
                              "actual %u, expected %u",
                              packet.body.modbusError.bytes.actual,
                              packet.body.modbusError.bytes.expected);
        case ModbusErrorID::ResponseException:
          return n + snprintf(buf + n, //
                              len - n,
                              "0x%x %s",
                              (uint8_t)packet.body.modbusError.exceptionCode,
                              exceptionCodeToString(packet.body.modbusError.exceptionCode));
        case ModbusErrorID::BadEchoMismatchedContents:
        case ModbusErrorID::BadResponseMalformedPacket: return n;
      }
    }
    case PacketID::DummyPacket: {
      return n + snprintf(buf + n,
                          len - n, //
                          "%u",
                          packet.body.dummy.outId);
    }
    // This should not be called
    case PacketID::NumIDs: return n;
  }
  // This is never reached
  return n;
}
