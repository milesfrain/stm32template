#pragma once

const uint32_t startWord = 0xFEEDABBE;
const uint32_t crc = 0xCAFEBEEF;

const size_t payloadSize = 64;
const size_t dummyDataSize = 100;
static_assert(payloadSize <= dummyDataSize);

struct TestPacket
{
  uint32_t startWord;
  uint32_t source;
  uint32_t id;
  uint8_t payload[payloadSize];
  uint32_t crc;
};
