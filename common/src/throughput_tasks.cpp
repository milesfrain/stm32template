/*
 * Producer and Consumer classes for throughput testing
 * to objects that are Writable or Readable.
 *
 * Note that there's an auto-formatter bug where "private:" is indented
 * https://stackoverflow.com/questions/25341017/eclipse-cdt-code-formatting-indentation-wrong-in-classes-and-potential-problem
 */

#include "throughput_tasks.h"
#include "board_defs.h"
#include "string.h"

// ------ Producer ---------

Producer::Producer( //
  const char* name,
  uint32_t source_id,
  Writable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : target{ target }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{
  initializePacket(dummyWrap.packet, PacketID::DummyPacket);
  dummyWrap.packet.origin = PacketOrigin::TargetTest;
  dummyWrap.packet.body.dummy.outId = source_id;
}

void Producer::func()
{
  // this will re-run once for each task, but nbd
  for (uint32_t i = 0; i < sizeof(dummyData); i++) {
    dummyData[i] = i;
  }

  // Convenience reference
  DummyPacket& dummyPkt = dummyWrap.packet.body.dummy;

  util.watchdogRegisterTask();

  while (1) {

    util.watchdogKick();

    // copy in a chunk of dummy data
    size_t dummyDataOffset = dummyWrap.packet.sequenceNum % (1 + sizeof(dummyData) - sizeof(dummyPkt.payload));
    memcpy(dummyPkt.payload, dummyData + dummyDataOffset, sizeof(dummyPkt.payload));

    // Write packet
    LL_GPIO_SetOutputPin(GreenLedPort, GreenLedPin);
    util.write(target, &setPacketWrapper(dummyWrap), wrappedPacketSize(dummyWrap));
    LL_GPIO_ResetOutputPin(GreenLedPort, GreenLedPin);

    util.logln("%s sent packet with sequence number %d", pcTaskGetName(task.handle), dummyWrap.packet.sequenceNum);

    dummyWrap.packet.sequenceNum++;

    osDelay(10);
  }
}

// with C++17, we could remove this line and just write as "inline static" in the declaration.
uint8_t Producer::dummyData[(sizeof(Producer::dummyData))];

// ------------ Consumer ----------

Consumer::Consumer( //
  const char* name,
  Readable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : target{ target }
  , parser{ *this }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void Consumer::func()
{
  util.watchdogRegisterTask();

  while (1) {

    util.watchdogKick();

    // Attempt to fill buffer
    size_t bytesRead = util.read(target, buf + len, sizeof(buf) - len);
    len += bytesRead;

    // optional logging of additional info
    // util.logln("%s read %d bytes", pcTaskGetName(task.handle), bytesRead);

    // Attempt to parse packets
    len = parser.extractPackets(buf, len);
  }
}

// Required by CanProcessPacket interface.
// Describes what to do with incoming packets.
void Consumer::processPacket(const Packet& packet)
{
  if (packet.id == PacketID::DummyPacket) {
    pktCt++;

    util.logln( //
      "%s received %d packets, last seq num %d",
      pcTaskGetName(task.handle),
      pktCt,
      packet.sequenceNum);

  } else {
    util.logPacket(pcTaskGetName(task.handle), " receive error: ", packet);
  }
}

// ------ ProducerUsb ---------

ProducerUsb::ProducerUsb( //
  const char* name,
  Writable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : target{ target }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void ProducerUsb::func()
{
  util.watchdogRegisterTask();

  for (int i = 0; true; i++) {

    util.watchdogKick();

    LL_GPIO_SetOutputPin(GreenLedPort, GreenLedPin);

    util.log("123 abc hello %d\r\n", i);

    // Send above log message over USB
    util.write(target, util.msg.buf, util.msg.len);

    LL_GPIO_ResetOutputPin(GreenLedPort, GreenLedPin);

    osDelay(100);
  }
}

// -------- Consumer Usb ---------

ConsumerUsb::ConsumerUsb( //
  const char* name,
  Readable& target,
  TaskUtilitiesArg& utilArg,
  UBaseType_t priority)
  : target{ target }
  , util{ utilArg }
  , task{ name, funcWrapper, this, priority }
{}

void ConsumerUsb::func()
{
  util.watchdogRegisterTask();

  while (1) {

    util.watchdogKick();

    // read as much as we can print without truncation
    hexMsg.len = util.read(target, hexMsg.buf, itmMaxHexBytes);
    byteCt += hexMsg.len;
    util.logHex(hexMsg);

    util.logln("%d total bytes over USB", byteCt);
  }
}
