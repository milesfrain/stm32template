#pragma once

#include "packets.h"
#include <stdio.h>

void initializePacket(Packet& packet, PacketID id);

void setPacketIdAndLength(Packet& packet, PacketID id);

WrappedPacket& setPacketWrapper(WrappedPacket& wrap);

void fillFreqPacket(WrappedPacket& wrap, uint32_t seq, uint32_t node, uint32_t freq);

void fillLengthErrorPacket(WrappedPacket& wrap, uint32_t len);

void fillDropErrorPacket(WrappedPacket& wrap, uint32_t drop);

// Same as memcpy, but returns length of bytes written
uint32_t mymemcpy(void* dst, const void* src, uint32_t len);

// Writes the entire wrapped packet to a file.
// Returns 1 if successfully written.
size_t fwriteWrapped(FILE* fp, WrappedPacket& wrap);

// Writes the entire wrapped packet to a file.
// Returns -1 for error.
ssize_t writeWrapped(int fd, WrappedPacket& wrap);

// Writes the entire wrapped packet to a buffer
uint32_t copyWrapped(void* buf, WrappedPacket& wrap);

// Writes just the inner unwrapped packet to a buffer
uint32_t copyInner(void* buf, WrappedPacket& wrap);

// Interface for objects that have a packet parsing callback
class CanProcessPacket
{
public:
  virtual void processPacket(const Packet&) = 0;
};

/*
 * Manages packet parsing.
 *
 * Create an instance by providing another object which implements
 * the CanProcessPacket interface (requires a processPacket() callback).
 *
 * Feed data into this parser via extractPackets().
 *
 * When complete packets (or parsing errors) are encountered,
 * they are sent to the processPacket() callback.
 */
class PacketParser
{
public:
  // Constructor for parser object that takes a callback function of
  // where to send extracted packets.
  PacketParser(CanProcessPacket& processer)
    : processer{ processer }
  {}

  // Feed new data for parsing into this function.
  // See .cpp comments for more details.
  uint32_t extractPackets(void* bufArg, uint32_t len);

  // 0 used for internal messages,
  // so external seq num should start at 1.
  // todo - convert to using `internal` origin
  uint32_t lastSeqNum = 0;

private:
  // Allocated space for any new packets we need to generate
  // for reporting parsing errors.
  Packet errorPacket;
  // Contains callback for what to do with parsed packet
  CanProcessPacket& processer;
};

/*
 * Tracks latest sequence number to apply to outgoing packets.
 * Call `rewrap` to apply next number to packet.
 */
class PacketSequencer
{
public:
  WrappedPacket& rewrap(WrappedPacket& wrap);
  // Todo initialize to 0
  uint32_t num = 1; // Tracks sequence number
};

/*
 * Writes a human-readable string for `packet` into `buf`.
 * Will not write more than `len` bytes.
 * Returns total bytes written.
 */
uint32_t snprintPacket(char* buf, uint32_t len, const Packet& packet);
