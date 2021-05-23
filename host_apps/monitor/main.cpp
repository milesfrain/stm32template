#include <stdio.h>
#include <unistd.h>

#include "packet_utils.h"
#include "packets.h"

// Todo - move these to common location

#define println(format, ...) printf(format "\n", ##__VA_ARGS__)

void printHex(uint8_t* buf, uint32_t len)
{
  for (uint32_t i = 0; i < len; i++) {
    printf(" %02X", buf[i]);
  }
  println();
}

// Object implementing CanProcessPacket interface.
// Describes what to do with parsed packets.
class PacketProcesser : public CanProcessPacket
{
public:
  void processPacket(const Packet& packet)
  {
    // Display contents
    // printHex(&packet, packet.length);
    char buf[300];
    snprintPacket(buf, sizeof(buf), packet);
    println("%s", buf);
  }
};

int main()
{
  char buf[maxWrappedPacketLength * 2];
  // How many bytes are leftover in buffer after parsing attempt
  uint32_t bufLen = 0;
  uint32_t totalRead = 0;

  PacketProcesser processer;
  PacketParser parser(processer);

  // Loop until file/stdin is closed
  while (1) {
    // Attempt to read enough bytes to fill buffer
    ssize_t numRead = read(STDIN_FILENO, buf + bufLen, sizeof(buf) - bufLen);
    if (numRead == -1) {
      perror("Got an error reading from stdin");
      break;
    }
    if (numRead == 0) {
      println("Got EOF");
      break;
    }
    // println("Read %ld of %ld bytes", numRead, sizeof(buf) - bufLen);
    bufLen += numRead;
    totalRead += numRead;
    bufLen = parser.extractPackets(buf, bufLen);
  }
  println("read a total of %d bytes", totalRead);
  println("remaining bytes %d", bufLen);
  println("done");
  return 0;
}
