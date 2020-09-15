#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "packets.h"

using namespace std;

const int reportingHz = 10;

// maximum uart datarate @ 115200 baud with start and stop bits
const int bytesPerSecond = 11520; // 11.52 KBps

// When only doing USB loopback (no uart) able to reach 300 KBps (2.4 Mbps),
// even though baud rate is set to much lower limit. Not a bad bug to have.
// const int bytesPerSecond = 300000; // seems totally robust.
// const int bytesPerSecond = 400000; // able to produce stall (non ideal host lockup), maxes at 320kB

// delay times are rounded down, so data rate may be slightly higher
const uint64_t usBetweenPackets = 1E6 * sizeof(TestPacket) / bytesPerSecond;
const uint64_t usBetweenReports = 1E6 / reportingHz;

// Get microseconds elapsed since the time of the passed argument
uint64_t usSince(struct timespec& past)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - past.tv_sec) * 1E6 + (now.tv_nsec - past.tv_nsec) / 1E3;
}

int main()
{
  const char device[] = "/dev/ttyACM0";
  // Open for read and write
  int fd = open(device, O_RDWR);
  if (fd == -1) {
    cout << "failed to open " << device << endl;
    exit(1);
  }

  // For configuration details:
  // https://man7.org/linux/man-pages/man3/termios.3.html
  struct termios config
  {
    0
  };

  // Set 8 bits per character, enable receiver, disable control lines
  config.c_cflag |= CS8 | CREAD | CLOCAL;

  // Set baud rate
  cfsetspeed(&config, B115200);

  // Using non-blocking default, but could alternatively
  // block for minimum bytes with optional timeout.
  // See c_cc[VMIN, VTIME]

  // Flush before setting config
  tcflush(fd, TCIOFLUSH);

  // Set attributes that take effect immediately
  if (tcsetattr(fd, TCSANOW, &config) != 0) {
    cout << "failed to set config" << endl;
    exit(1);
  }

  uint8_t dummyData[dummyDataSize];
  for (uint32_t i = 0; i < sizeof(dummyData); i++) {
    dummyData[i] = i;
  }

  TestPacket pktOut;
  pktOut.startWord = startWord;
  pktOut.source = 42;
  pktOut.crc = crc;

  uint32_t outId = 0;

  char buf[10000]; // for content we get back from the device
  size_t len = 0;
  int inPktCount = 0;
  uint32_t lastInId = 0;

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  uint64_t t = usSince(start);
  uint64_t nextPacketEvent = t;
  uint64_t nextReportEvent = t + usBetweenReports;

  // To check for complete packet writes
  size_t packetBytesWritten = sizeof(pktOut);

  while (1) {
    t = usSince(start);

    // Check if it's time to send the next packet
    if (t >= nextPacketEvent) {
      // Check if we successfully wrote everything last time
      if (packetBytesWritten == sizeof(pktOut)) {
        pktOut.id = outId;

        // copy in a chunk of dummy data
        memcpy(pktOut.payload, dummyData + (outId % (1 + sizeof(dummyData) - sizeof(pktOut.payload))), sizeof(pktOut.payload));

        packetBytesWritten = write(fd, &pktOut, sizeof(pktOut));

        outId++;
        nextPacketEvent += usBetweenPackets;
      } else {
        // Still pending write from last time
        packetBytesWritten += write(fd, &pktOut + packetBytesWritten, sizeof(pktOut) - packetBytesWritten);

        // Question - how persistent should these rewrite attempts be?
        // How verbose should we be with reporting?
        // Maybe a slight additional delay, plus only notify during reporting interval.
        usleep(1000); // fine as long as packets are bigger than 13 bytes.
      }
    }

    // always attempt to read and parse an incomming packet

    // attempt to fill buffer
    len += read(fd, buf + len, sizeof(buf) - len);

    size_t offset = 0;
    // keep checking for packets as long as we have enough space for one
    while (len - offset >= sizeof(TestPacket)) {
      // Check if this is a packet
      auto pktIn = (TestPacket*)(buf + offset);
      if (pktIn->startWord == startWord && pktIn->crc == crc) {
        // found packet
        offset += sizeof(TestPacket);
        inPktCount++;
        lastInId = pktIn->id;
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

    // reporting interval
    if (t >= nextReportEvent) {
      printf("Sent %d packets\n", outId);

      if (packetBytesWritten != sizeof(pktOut)) {
        printf("Waiting to write some bytes in packet\n");
      }
      static uint32_t lastInPktCount = 0;
      uint32_t inBpsTotal = inPktCount * sizeof(TestPacket) * 1E6 / t;
      uint32_t inBpsInterval = (inPktCount - lastInPktCount) * sizeof(TestPacket) * 1E6 / usBetweenReports;
      lastInPktCount = inPktCount;
      printf("Got %d packets (last id %d). Dropped %d. Pending %d. Bps total %d, in interval %d\n", inPktCount, lastInId, lastInId + 1 - inPktCount, outId - lastInId, inBpsTotal, inBpsInterval);

      nextReportEvent += usBetweenReports;
    }

    // calculate sleep time
    int nextEvent = min(nextPacketEvent, nextReportEvent);
    t = usSince(start);
    if (t < nextEvent) {
      usleep(nextEvent - t);
    }
  }

  return 0;
}
