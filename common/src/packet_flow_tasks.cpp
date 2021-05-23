/*
 * See header for comments
 */

#include "packet_flow_tasks.h"
#include "catch_errors.h"

// Set to true to enable more verbose ITM printing of incoming and outgoing packets.
const bool verboseIO = true;

// ------------ PacketIntake ----------

PacketIntake::PacketIntake( //
  const char* name,
  Readable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : target{ target }
  , parser{ *this }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void PacketIntake::func()
{
  util.watchdogRegisterTask();

  while (1) {
    util.watchdogKick();

    // Attempt to fill buffer
    len += util.read(target, buf + len, sizeof(buf) - len);

    // Attempt to parse packets
    len = parser.extractPackets(buf, len);
  }
}

// Required by CanProcessPacket interface.
// Describes what to do with incoming packets.
void PacketIntake::processPacket(const Packet& packet)
{
  // If not a parsing error
  if (packet.origin != PacketOrigin::Internal) {

    // Log incoming packet counters via ITM
    packetsInCount++;
    itmSendValue(ItmPort::PacketsInCount, packetsInCount);
    itmSendValue(ItmPort::PacketsInSequence, packet.sequenceNum);

    if (verboseIO) {
      // Verbose logging of incoming packet contents.
      util.logPacket(pcTaskGetName(task.handle), " got packet: ", packet);
    }

  } else {
    util.logPacket(pcTaskGetName(task.handle), " receive error: ", packet);
  }

  // Stash parsed packet (or packet parsing error) until another task reads from intake.
  util.write(msgbuf, &packet, packet.length);
}

size_t PacketIntake::read(void* buf, size_t len, TickType_t ticks)
{
  return msgbuf.read(buf, len, ticks);
}

// ------ PacketOutput ---------

PacketOutput::PacketOutput( //
  const char* name,
  Writable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : target{ target }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void PacketOutput::func()
{
  util.watchdogRegisterTask();

  while (1) {
    util.watchdogKick();

    // Copy next available packet from buffer into wrapper
    uint32_t len = util.read(msgbuf, &wrap.packet, sizeof(wrap.packet));

    // We could alternatively just assume buffer length is correct
    // and update the packet field here, but that might let more sigificant
    // issues slip by.
    if (len != wrap.packet.length) {
      util.logln( //
        "%s dropping packet with invalid length field. Expected %u, got %u",
        pcTaskGetName(task.handle),
        len,
        wrap.packet.length);
      continue;
    }

    if (wrap.packet.length != packetSizeFromID(wrap.packet.id)) {
      util.logln( //
        "%s dropping packet where length field %u does not match expected length %u from ID",
        pcTaskGetName(task.handle),
        wrap.packet.length,
        packetSizeFromID(wrap.packet.id));
      continue;
    }

    // Edit origin of parsing errors
    if (wrap.packet.origin == PacketOrigin::Internal) {
      wrap.packet.origin = PacketOrigin::TargetToHost;
    }

    // Update sequence number (updates crc too)
    sequencer.rewrap(wrap);

    // Log outgoing packet counters via ITM
    packetsOutCount++;
    itmSendValue(ItmPort::PacketsOutCount, packetsOutCount);
    //__asm volatile ("nop");
    itmSendValue(ItmPort::PacketsOutSequence, wrap.packet.sequenceNum);

    if (verboseIO) {
      // Verbose logging of outgoing packet contents.
      util.logPacket(pcTaskGetName(task.handle), " sending wrapped packet: ", wrap.packet);
    }

    // Write wrapped packet.
    util.write(target, &wrap, wrappedPacketSize(wrap));
  }
}

size_t PacketOutput::write(const void* buf, size_t len, TickType_t ticks)
{
  // Message Buffers only allow a single writer by default,
  // but we can support multiple writers with a mutex.
  // This mutex is built-in to queues already.
  ScopedLock locked(writeMutex, suggestedTimeoutTicks);

  // Return 0 if we timed-out before acquiring lock
  if (!locked.gotLock()) {
    return 0;
  }

  return msgbuf.write(buf, len, ticks);
}

// -------- Coupling ---------

Coupling::Coupling( //
  const char* name,
  Readable& src,
  Writable& dst,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : src{ src }
  , dst{ dst }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void Coupling::func()
{
  util.watchdogRegisterTask();

  while (1) {

    util.watchdogKick();

    // Read next chunk of data
    len = util.read(src, buf, sizeof(buf));

    // Blocking writes until full chunk of data is sent.
    written = util.write(dst, buf, len);
    while (written != len) {
      written += util.write(dst, buf + written, len - written);
    }
  }
}
