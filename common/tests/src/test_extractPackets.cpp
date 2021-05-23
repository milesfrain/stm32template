#include "CppUTest/TestHarness.h"

#include "packet_utils.h"
#include "software_crc.h"
#include <stdio.h>
#include <string.h>

#define println(format, ...) printf(format "\n", ##__VA_ARGS__)

// Todo - move this testing code somewhere where it can also be run on target

// Adds an incrementing marker to buffer to improve readability.
// Returns number of bytes added by marker
uint32_t markCount = 0;
uint32_t mark(uint8_t* buf)
{
  uint16_t markStart = 0xAAAA;
  uint16_t markEnd = 0xBBBB;
  uint16_t pos = 0;
  pos += mymemcpy(buf + pos, (&markStart), sizeof(markStart));
  pos += mymemcpy(buf + pos, (&markCount), sizeof(markCount));
  pos += mymemcpy(buf + pos, (&markEnd), sizeof(markEnd));
  markCount++;
  return pos;
}

void printHex(const void* buf, uint32_t len)
{
  for (int i = 0; i < len; i++) {
    printf(" %02X", ((uint8_t*)buf)[i]);
  }
  println();
}

// Object implementing CanProcessPacket interface.
// Describes what to do with parsed packets.
class TestProcesser : public CanProcessPacket
{
public:
  uint8_t bufOut[30000];
  uint32_t bufOutPos = 0;

  void processPacket(const Packet& packet)
  {
    // Add marker
    println("marker %d:", markCount);
    bufOutPos += mark(bufOut + bufOutPos);

    // Display contents
    printHex(&packet, packet.length);
    char buf[300];
    snprintPacket(buf, sizeof(buf), packet);
    println("%s", buf);

    // Copy packet
    bufOutPos += mymemcpy(bufOut + bufOutPos, &packet, packet.length);
  }
};

// To fix annoying eclipse syntax error squiggles
#ifdef TEST
TEST_GROUP(TestExtractPackets){ void setup(){} void teardown(){} };
TEST(TestExtractPackets, test_extractPackets)
#else
void foo()
#endif
{
  WrappedPacket goodPkt1;
  fillFreqPacket(goodPkt1, 1, 3, 25);

  WrappedPacket goodPkt2;
  fillFreqPacket(goodPkt2, 2, 3, 50);

  WrappedPacket badPktId;
  fillFreqPacket(badPktId, 5, 3, 50);
  badPktId.packet.id = PacketID::NumIDs;

  WrappedPacket badPktCrc;
  fillFreqPacket(badPktCrc, 6, 3, 50);
  badPktCrc.crc = 1234;

  WrappedPacket goodPkt3;
  fillFreqPacket(goodPkt3, 7, 3, 50);

  WrappedPacket goodPkt4;
  fillFreqPacket(goodPkt4, 8, 3, 50);

  WrappedPacket errorPkt;

  // -----------
  // Checking for raw packet encoding

  const uint32_t gp1_len = wrappedPacketSizeFromID(goodPkt1.packet.id);
  // Need to double-check endianness and padding on uC.
  // Both PC and STM32 should be little endian
  // Can enforce packed structs if necessary.
  const uint8_t gp1_mem[gp1_len]{
    0xBE, 0xAB, 0xED, 0xFE, // magic start, 0xfeedabbe
    0x23, 0x57, 0xC6, 0x55, // crc
    0x14, 0x00, 0x00, 0x00, // length 20
    0x01, 0x00, 0x00, 0x00, // sequence 1
    0x00, 0x00, 0x00, 0x00, // origin internal 0
    0x08, 0x00, 0x00, 0x00, // ID freq 8
    0x03, 0x00,             // addr 3
    0x19, 0x00              // freq 25
  };
  MEMCMP_EQUAL(gp1_mem, &goodPkt1, gp1_len);

  // ===========
  // Checking that one packet is extracted correctly

  uint8_t bufIn[30000];
  uint32_t bufInPos = 0;

  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt1);

  uint8_t bufExpect[30000];
  uint32_t bufExpectPos = 0;
  markCount = 0;

  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, goodPkt1);

  markCount = 0;

  TestProcesser testProcesser;
  PacketParser parser(testProcesser);
  uint32_t numRemainingBytes = parser.extractPackets(bufIn, bufInPos);

  LONGS_EQUAL(0, numRemainingBytes);
  LONGS_EQUAL(bufExpectPos, testProcesser.bufOutPos);
  MEMCMP_EQUAL(bufExpect, testProcesser.bufOut, bufExpectPos);

  // ===========
  // Check longer sequence with bad bytes

  // Fill bufIn with wrapped packets to parse
  // This simulates data on the wire

  bufInPos = 0;
  const uint8_t garbageBytes[] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, //
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, //
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
  };

  bufInPos += mymemcpy(bufIn + bufInPos, garbageBytes, sizeof(garbageBytes));
  bufInPos += mymemcpy(bufIn + bufInPos, &startWord, sizeof(startWord));
  bufInPos += mymemcpy(bufIn + bufInPos, garbageBytes, sizeof(garbageBytes));
  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt1);
  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt2);

  // Save pointer to packet that we're going to corrupt
  WrappedPacket* badPktSmallLength = (WrappedPacket*)(bufIn + bufInPos);
  // Write un-corrupted packet
  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt2);
  // Corrupt length field
  badPktSmallLength->packet.length = 1;

  // Save pointer to packet that we're going to corrupt
  WrappedPacket* badPktBigLength = (WrappedPacket*)(bufIn + bufInPos);
  // Write un-corrupted packet
  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt2);
  // Corrupt length field
  badPktBigLength->packet.length = sizeof(Packet) + 40;

  bufInPos += copyWrapped(bufIn + bufInPos, badPktId);
  bufInPos += copyWrapped(bufIn + bufInPos, badPktCrc);
  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt3);
  bufInPos += copyWrapped(bufIn + bufInPos, goodPkt4);
  bufInPos += mymemcpy(bufIn + bufInPos, garbageBytes, sizeof(garbageBytes));

  // Optionally write this data for testing with other tools:
#if 1
  FILE* fp = fopen("data.bin", "wb");

  // Write everything as one big block
  if (1 != fwrite(bufIn, bufInPos, 1, fp)) {
    println("Failed to write to file");
  }

  fclose(fp);
#endif
  // --------

  // Fill bufExpect with what we expect to see written to
  // bufOut by processPacket.
  // This contains successfully-parsed packets separated by "marks"
  // with additional packets indicating parsing errors.

  bufExpectPos = 0;
  markCount = 0;

  // mark 0
  // Report length error due to gibberish in length field after magic start word.
  // The first word of garbage bytes (0x04030201) is picked up as crc.
  fillLengthErrorPacket(errorPkt, 0x08070605);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 1
  // Report total dropped bytes before upcoming successful parse
  fillDropErrorPacket(errorPkt, 2 * sizeof(garbageBytes) + sizeof(startWord));
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 2
  // Well-formed packet, successful parse
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, goodPkt1);

  // mark 3
  // Well-formed packet, successful parse
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, goodPkt2);

  // mark 4
  // Packet with incorrect length field
  fillLengthErrorPacket(errorPkt, badPktSmallLength->packet.length);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 5
  // Packet with incorrect length field
  fillLengthErrorPacket(errorPkt, badPktBigLength->packet.length);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 6
  // Packet with incorrect ID
  initializePacket(errorPkt.packet, PacketID::ParsingErrorInvalidID);
  errorPkt.packet.body.parsingError.invalidID = static_cast<uint32_t>(badPktId.packet.id);
  setPacketWrapper(errorPkt);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 7
  // Packet with incorrect CRC
  initializePacket(errorPkt.packet, PacketID::ParsingErrorInvalidCRC);
  errorPkt.packet.body.parsingError.invalidCRC.provided = 1234;
  errorPkt.packet.body.parsingError.invalidCRC.calculated = //
    crc32(reinterpret_cast<const uint8_t*>(&badPktCrc.packet), badPktCrc.packet.length);
  setPacketWrapper(errorPkt);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 8
  // Report total dropped bytes before upcoming successful parse
  // There were 4 malformed packets of the same type (so same size).
  fillDropErrorPacket(errorPkt, 4 * wrappedPacketSizeFromID(badPktCrc.packet.id));
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 9
  // Sequence error
  initializePacket(errorPkt.packet, PacketID::ParsingErrorInvalidSequence);
  errorPkt.packet.body.parsingError.invalidSequence.provided = goodPkt3.packet.sequenceNum;
  errorPkt.packet.body.parsingError.invalidSequence.expected = goodPkt2.packet.sequenceNum + 1;
  setPacketWrapper(errorPkt);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // mark 10
  // Well-formed packet, successful parse
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, goodPkt3);

  // mark 11
  // Well-formed packet, successful parse
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, goodPkt4);

  // mark 12
  // Report remaining bytes dropped from buffer

  const uint32_t expectedNumRemainingBytes = minWrappedPacketLength - 1;
  const uint32_t droppedGarbageBytes = (uint32_t)sizeof(garbageBytes) - expectedNumRemainingBytes;

  fillDropErrorPacket(errorPkt, droppedGarbageBytes);
  bufExpectPos += mark(bufExpect + bufExpectPos);
  bufExpectPos += copyInner(bufExpect + bufExpectPos, errorPkt);

  // What should be leftover in the buffer
  const uint8_t* expectedLeftoverGarbageBytes = garbageBytes + droppedGarbageBytes;

  // --------
  // Run parsing

  testProcesser.bufOutPos = 0;
  markCount = 0;
  parser.lastSeqNum = 0;

  numRemainingBytes = parser.extractPackets(bufIn, bufInPos);

  // -------
  // Compare results

  // Check that we have expected unprocessed leftover bytes
  LONGS_EQUAL(23, numRemainingBytes);
  LONGS_EQUAL(expectedNumRemainingBytes, numRemainingBytes);
  MEMCMP_EQUAL(expectedLeftoverGarbageBytes, bufIn, numRemainingBytes);

  // Check that parsed output matches expectations
  LONGS_EQUAL(bufExpectPos, testProcesser.bufOutPos);
  MEMCMP_EQUAL(bufExpect, testProcesser.bufOut, bufExpectPos);
}
