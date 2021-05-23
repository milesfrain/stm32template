#include <argp.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "packet_utils.h"
#include "packets.h"

#define println(format, ...) printf(format "\n", ##__VA_ARGS__)

// Get microseconds elapsed since the time of the passed argument
uint64_t usSince(struct timespec& past)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - past.tv_sec) * 1E6 + (now.tv_nsec - past.tv_nsec) / 1E3;
}

// Prints additional status along with writeWrapped call
// Returns 1 if packet was successfully written, 0 otherwise (even for partial write)
uint32_t writeWrappedReport(int fd, WrappedPacket& wrap)
{
  // len for debugging
  ssize_t len = wrappedPacketSize(wrap);

  ssize_t ret = writeWrapped(fd, wrap);

  if (ret == -1) {
    perror("write error");
    // Wait 100ms to make logs less spammy
    usleep(100000);
    return 0;
  } else if (ret != len) {
    println("Only wrote %ld of %ld bytes of packet", ret, len);
    return 0;
  } else {
    // println("wrote packet %d", packetCount);
    return 1;
  }
}

// Reports any errors encountered during serial read
ssize_t readReport(int fd, void* buf, ssize_t len)
{
  ssize_t ret = read(fd, buf, len);

  if (ret == -1) {
    perror("read error");
    // Wait 100ms to make logs less spammy
    usleep(100000);
    return 0;
  }

  return ret;
}

// Opens serial connection and returns file descriptor.
// 115200 baud rate.
// Returns -1 to indicate error.
int setupSerial(const char* device)
{
  // Open for read and write
  int fd = open(device, O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    perror("failed to open serial connection");
    return -1;
  }

  // For configuration details:
  // https://man7.org/linux/man-pages/man3/termios.3.html
  struct termios config = { 0 };

  // Set 8 bits per character, enable receiver, disable control lines
  config.c_cflag |= CS8 | CREAD | CLOCAL;

  // Set baud rate
  if (cfsetspeed(&config, B115200) != 0) {
    perror("failed to set serial port baud rate");
    return -1;
  }

  // Using non-blocking default, but could alternatively
  // block for minimum bytes with optional timeout.
  // See c_cc[VMIN, VTIME]

  // Flush before setting config
  if (tcflush(fd, TCIOFLUSH) != 0) {
    perror("failed to flush serial port");
    return -1;
  }

  // Set attributes that take effect immediately
  if (tcsetattr(fd, TCSANOW, &config) != 0) {
    perror("failed to set serial config");
    return -1;
  }

  return fd;
}

// Object implementing CanProcessPacket interface.
// Describes what to do with parsed packets.
class PacketProcesser : public CanProcessPacket
{
public:
  // What to do with incoming packets over serial
  void processPacket(const Packet& packet)
  {
    if (packet.id == PacketID::DummyPacket) {
      inPktCount++;
      lastInId = packet.body.dummy.outId;
      // println("valid dummy packet num %d, id %d", inPktCount, lastInId);
    } else {
      // Got a parsing error or another unexpected packet
      char buf[300];
      snprintPacket(buf, sizeof(buf), packet);
      println("Unexpected: %s", buf);
    }
  }

  uint32_t lastInId = 0;
  int inPktCount = 0;
};

// ------
// argp command line options
// https://www.gnu.org/software/libc/manual/html_node/Argp.html

// Populate some globals recognized by argp
// const char *argp_program_version = "todo-version-0.1";

#define DEFAULT_SERIAL_PORT "/dev/ttyACM0"
// #define DEFAULT_SERIAL_PORT "/dev/ttyUSB0"
// 115200 baud is 11520 bytes per second (10 bits per byte with start and stop bit)
#define DEFAULT_BYTE_RATE "11520"

// Available options
static struct argp_option options[] = { //
  { "device", 'd', "DEVICE", 0, "Serial port to use. Default: " DEFAULT_SERIAL_PORT },
  { "byterate", 'b', "BYTERATE", 0, "Bytes per second. Default: " DEFAULT_BYTE_RATE },
  { 0 }
};

// Additional program usage docs
static char doc[] = "See readme for more detailed usage information";

// Program's arguments and options
struct arguments
{
  char* device;
  char* byterate;
};

// How to parse a single option or argument
static error_t parse_arg(int key, char* arg, struct argp_state* state)
{
  struct arguments* arguments = (struct arguments*)state->input;

  switch (key) {
    case 'd': //
      arguments->device = arg;
      break;

    case 'b': //
      arguments->byterate = arg;
      break;

    case ARGP_KEY_ARG:
      // Unexpected additional arguments
      argp_usage(state);
      break;

    default: //
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

int main(int argc, char** argv)
{
  // argp parser
  struct argp argp = { options, parse_arg, 0, doc };

  struct arguments arguments;

  // Default argument values
  arguments.device = (char*)DEFAULT_SERIAL_PORT;
  arguments.byterate = (char*)DEFAULT_BYTE_RATE;

  // Parse program arguments
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  int bytesPerSecond = atoi(arguments.byterate);
  println("Launching on %s with byterate %d", arguments.device, bytesPerSecond);

  uint8_t dummyData[100];
  static_assert(sizeof(DummyPacket::payload) <= sizeof(dummyData));

  for (uint32_t i = 0; i < sizeof(dummyData); i++) {
    dummyData[i] = i;
  }

  WrappedPacket dummyWrap;
  initializePacket(dummyWrap.packet, PacketID::DummyPacket);
  dummyWrap.packet.origin = PacketOrigin::HostToTarget;
  DummyPacket& dummyPkt = dummyWrap.packet.body.dummy;
  dummyPkt.outId = 0;

  PacketProcesser processer;
  PacketParser parser(processer);

  char buf[10000]; // for content we get back from the device
  size_t len = 0;

  const int reportingHz = 10;
  const uint64_t usBetweenReports = 1E6 / reportingHz;

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  uint64_t t = usSince(start);
  uint64_t nextPacketEvent = t;
  uint64_t nextReportEvent = t + usBetweenReports;

  int serialFileno = setupSerial("/dev/ttyACM0");
  if (serialFileno == -1) {
    return 1;
  }

  // https://www.man7.org/linux/man-pages/man2/poll.2.html
  struct pollfd fds[] = { { fd : serialFileno, events : POLLIN } };

  // Calculate delays based on byterate
  const uint32_t pktOutSize = wrappedPacketSizeFromID(PacketID::DummyPacket);
  // delay times are rounded down, so data rate may be slightly higher
  uint64_t usBetweenPackets = 1E6 * pktOutSize / bytesPerSecond;

  while (1) {
    // Wait for new data on one of the watched interfaces.
    // Timeout after n miliseconds
    int ret = poll(fds, sizeof(fds) / sizeof(fds[0]), usBetweenPackets / 1000);

    // Check for errors
    if (ret == -1) {
      // report error and abort
      perror("Poll() error");
    } else if (ret) { // Data detected
      // Check for incoming serial data
      if (fds[0].revents & POLLIN) {
        // Reset flag for next polling loop
        fds[0].revents &= ~POLLIN;

        // Attempt to fill buffer
        ssize_t bytesRead = read(serialFileno, buf + len, sizeof(buf) - len);

        if (bytesRead == -1) {
          perror("Error reading from serial");
          bytesRead = 0;
        }

        // println("read %ld bytes", bytesRead);

        // Attempt to parse packets
        len += bytesRead;
        len = parser.extractPackets(buf, len);
      }
    } // end of poll check

    t = usSince(start);

    static int outPktCount = 0;
    // Check if it's time to send the next packet
    if (t >= nextPacketEvent) {
      // Not retrying for partial writes

      // copy in a chunk of dummy data
      memcpy(dummyPkt.payload, dummyData + (dummyPkt.outId % (1 + sizeof(dummyData) - sizeof(dummyPkt.payload))), sizeof(dummyPkt.payload));

      if (writeWrappedReport(serialFileno, setPacketWrapper(dummyWrap))) {
        outPktCount++;
        dummyPkt.outId++;
        dummyWrap.packet.sequenceNum++;
        nextPacketEvent += usBetweenPackets;
      } else {
        println("Failed to write packet with id %d", dummyPkt.outId);

        // By not incrementing nextPacketEvent, we allow datarate to eventually
        // catch up to desired rate.

        // This just drops the packet. Data rate will never catch-up to desired rate.
        // nextPacketEvent += usBetweenPackets;

        // This also drops the packet, and waits a bit longer to retry
        // so logs are less spammy when encountering errors.
        // nextPacketEvent += usBetweenReports;
      }
    }

    // reporting interval
    if (t >= nextReportEvent) {
      // If zero packets sent, then last id is -1
      println("Sent %d packets (last id: %d)", outPktCount, dummyPkt.outId - 1);

      static uint32_t lastInPktCount = 0;
      uint32_t inBpsTotal = processer.inPktCount * pktOutSize * 1E6 / t;
      uint32_t inBpsInterval = (processer.inPktCount - lastInPktCount) * pktOutSize * 1E6 / usBetweenReports;
      lastInPktCount = processer.inPktCount;
      println("Got %d packets (last id %d). Dropped %d. Pending %d. Bps total %d, in interval %d", //
              processer.inPktCount,
              processer.lastInId,
              processer.lastInId + 1 - processer.inPktCount, // many ways to calculate drops
              outPktCount - 1 - processer.lastInId,
              inBpsTotal,
              inBpsInterval);

      nextReportEvent += usBetweenReports;
    }
  }

  return 0;
}
