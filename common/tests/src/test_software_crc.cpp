#include "CppUTest/TestHarness.h"

#include "software_crc.h"

TEST_GROUP(TestCrc){ void setup(){} void teardown(){} };

TEST(TestCrc, test_crc16)
{
  // Pg 5-68, left column
  uint8_t in1[] = { 0x01, 0x10, 0x20, 0x00, 0x00, 0x02, 0x04, 0x00, 0x02, 0x02, 0x58 };
  LONGS_EQUAL(0x34cb, crc16(in1, sizeof(in1)));

  // Pg 5-68, right column
  uint8_t in2[] = { 0x01, 0x10, 0x20, 0x00, 0x00, 0x02 };
  LONGS_EQUAL(0x084a, crc16(in2, sizeof(in2)));
}

TEST(TestCrc, test_crc32)
{
  uint8_t in1[] = { 0x01, 0x10, 0x20, 0x00, 0x00, 0x02, 0x04, 0x00, 0x02, 0x02, 0x58 };
  LONGS_EQUAL(0xE31C0586, crc32(in1, sizeof(in1)));

  uint8_t in2[] = { 0x01, 0x10, 0x20, 0x00, 0x00, 0x02 };
  LONGS_EQUAL(0x54422B96, crc32(in2, sizeof(in2)));
}
