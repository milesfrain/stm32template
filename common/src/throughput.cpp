/*
 * Producer and Consumer classes for throughput testing
 * to objects that are Writable or Readable.
 *
 * Note that there's an auto-formatter bug where "private:" is indented
 * https://stackoverflow.com/questions/25341017/eclipse-cdt-code-formatting-indentation-wrong-in-classes-and-potential-problem
 */

#include "throughput.h"
#include "board_defs.h"
#include "logging.h"
#include "stm32f4xx_ll_gpio.h"
#include "string.h"

// ------ Producer ---------

Producer::Producer(const char* name, uint32_t source_id, Writable& target, UBaseType_t priority)
  : target{ target }
  , task{ name, funcWrapper, this, priority }
  , source_id{ source_id }
{}

void Producer::func()
{
  // this will re-run once for each task, but nbd
  for (uint32_t i = 0; i < sizeof(dummyData); i++) {
    Producer::dummyData[i] = i;
  }

  pkt.startWord = startWord;
  pkt.source = source_id;
  pkt.crc = crc;
  uint32_t id = 0;

  while (1) {
    pkt.id = id;

    // copy in a chunk of dummy data
    memcpy(pkt.payload, dummyData + (id % (1 + sizeof(dummyData) - sizeof(pkt.payload))), sizeof(pkt.payload));

    LL_GPIO_SetOutputPin(GreenLedPort, GreenLedPin);
    target.write((uint8_t*)&pkt, sizeof(pkt));
    LL_GPIO_ResetOutputPin(GreenLedPort, GreenLedPin);

    osDelay(10);

    id++;
  }
}

// with C++17, we could remove this line and just write as "inline static" in the declaration.
uint8_t Producer::dummyData[dummyDataSize];

// ------------ Consumer ----------
// todo - rename consumer as parser, and make it Readable.

Consumer::Consumer(const char* name, Readable& target, UBaseType_t priority)
  : target{ target }
  , task{ name, funcWrapper, this, priority }
{}

void Consumer::func()
{
  len = 0;
  pktCt = 0;

  while (1) {
    // attempt to fill buffer
    len += target.read(buf + len, sizeof(buf) - len);

    size_t offset = 0;
    // keep checking for packets as long as we have enough space for one
    while (len - offset >= sizeof(TestPacket)) {
      // Check if this is a packet
      auto pkt = (TestPacket*)(buf + offset);
      if (pkt->startWord == startWord && pkt->crc == crc) {
        // found packet
        offset += sizeof(TestPacket);

        // reporting interval
        const uint32_t interval = 100;
        if (pktCt % interval == 0) {
          itmPrintf(msg, "%d packets (last id %d) from source %d. Dropped %d\r\n", pktCt, pkt->id, pkt->source, pkt->id - pktCt);
        }
        pktCt++;
      } else {
        // did not find packet
        // try next spot
        offset++;
      }
    }

    // shift out any consumed bytes
    if (offset) {
      len -= offset;
      memmove(buf, buf + offset, len);
    }
  }
}

// ------ ProducerUsb ---------

ProducerUsb::ProducerUsb(const char* name, Writable& target, UBaseType_t priority)
  : target{ target }
  , task{ name, funcWrapper, this, priority }
{}

void ProducerUsb::func()
{
  static LogMsg msg;
  int i = 0;

  while (1) {

    LL_GPIO_SetOutputPin(GreenLedPort, GreenLedPin);
    msgPrintf(msg, "123 abc hello %d\r\n", i);
    // itmSendMsg(msg);
    target.write((uint8_t*)msg.buf, msg.len);
    LL_GPIO_ResetOutputPin(GreenLedPort, GreenLedPin);

    osDelay(100);

    i++;
  }
}

// -------- Consumer Usb ---------

ConsumerUsb::ConsumerUsb(const char* name, Readable& target, UBaseType_t priority)
  : target{ target }
  , task{ name, funcWrapper, this, priority }
{}

void ConsumerUsb::func()
{
  byteCt = 0;

  while (1) {
    // read as much as we can print without truncation
    hexMsg.len = target.read((uint8_t*)hexMsg.buf, MaxHexBytes);
    byteCt += hexMsg.len;
    itmPrintHex(hexMsg);

    itmPrintf(msg, "%d total bytes over USB\r\n", byteCt);
  }
}

// -------- Pipe ---------

Pipe::Pipe(const char* name, Readable& src, Writable& dst, UBaseType_t priority)
  : src{ src }
  , dst{ dst }
  , task{ name, funcWrapper, this, priority }
{}

void Pipe::func()
{
  while (1) {
    // blocking read of next chunk of data
    len = src.read(buf, sizeof(buf));

    // blocking writes until full chunk of data is sent
    written = dst.write(buf, len);
    while (written != len) {
      written += dst.write(buf + written, len - written);
    }
  }
}
