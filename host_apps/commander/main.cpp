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

#include "basic.h"
#include "packet_utils.h"
#include "packets.h"

#define println(format, ...) printf(format "\n", ##__VA_ARGS__)

// Get microseconds elapsed since the time of the passed argument.
// Rollover after 300k years
uint64_t usSince(struct timespec& past)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec - past.tv_sec) * 1E6 + (now.tv_nsec - past.tv_nsec) / 1E3;
}

// Global file pointers.
// Necessary for for callbacks and for flushing on ctrl-c
FILE *logSerialIn, *logSerialOut, *logToMonitor;
int serialFileno;

FILE* fopenReport(const char* name, const char* modes)
{
  FILE* fp = fopen(name, modes);
  if (!fp) {
    perror("fopen() error");
    println("Failed to open file %s", name);
  }
  return fp;
}

// Prints additional status along with fwriteWrapped call
void fwriteWrappedReport(FILE* fp, WrappedPacket& wrap)
{
  // Counter currently shared among all file pointers
  static int packetCount = 0;
  packetCount++;
  if (!fwriteWrapped(fp, wrap)) {
    perror("fwrite error");
    println("Failed to fwrite packet %d", packetCount);
  } else {
    // println("fwrote packet %d", packetCount);
  }
}

// Prints additional status along with writeWrapped call
void writeWrappedReport(int fd, WrappedPacket& wrap)
{
  // Counter currently shared among all file descriptors
  static int packetCount = 0;
  packetCount++;
  ssize_t ret = writeWrapped(fd, wrap);
  // len for debugging
  ssize_t len = wrapperLength + wrap.packet.length;

  if (ret == -1) {
    perror("write error");
    println("Failed to write packet %d", packetCount);
  } else if (ret != len) {
    println("Only wrote %ld of %ld bytes of packet %d", ret, len, packetCount);
  } else {
    // println("wrote packet %d", packetCount);
  }
}

// Opens serial connection and returns file descriptor.
// 115200 baud rate.
// Returns -1 to indicate error.
int setupSerial(char* device)
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

// Writes packet to three locations:
// - serial output
// - serial logfile
// - monitor logfile
void writeToMonitorAndSerialOut(WrappedPacket& wrap)
{
  writeWrappedReport(serialFileno, wrap);
  fwriteWrappedReport(logSerialOut, wrap);
  fwriteWrappedReport(logToMonitor, wrap);
}

// Object implementing CanProcessPacket interface.
// Describes what to do with parsed packets.
class PacketProcesser : public CanProcessPacket
{
public:
  // What to do with incoming packets over serial
  void processPacket(const Packet& packet)
  {
    // Display contents
    // printHex(&packet, packet.length);
    char buf[300];
    snprintPacket(buf, sizeof(buf), packet);
    println("Got packet: %s", buf);

    // might be a small perf boost if wrap is static
    WrappedPacket wrap;
    // Copy packet contents into outgoing wrapper
    memcpy(&wrap.packet, &packet, packet.length);
    if (packet.origin == PacketOrigin::Internal) {
      wrap.packet.origin = PacketOrigin::HostToMonitor;
    }
    // Don't edit sequence number
    // static PacketSequencer ps;
    // fwriteWrapped(logToMonitor, ps.rewrap(wrap));
    setPacketWrapper(wrap);
    fwriteWrappedReport(logToMonitor, wrap);
  }
};

// ------
// argp command line options
// https://www.gnu.org/software/libc/manual/html_node/Argp.html

// Populate some globals recognized by argp
// const char *argp_program_version = "todo-version-0.1";

#define DEFAULT_SERIAL_PORT "/dev/ttyACM0"
// #define DEFAULT_SERIAL_PORT "/dev/ttyUSB0"

// Available options
static struct argp_option options[] = { //
  { "device", 'd', "DEVICE", 0, "Serial port to use. Default: " DEFAULT_SERIAL_PORT },
  { 0 }
};

// Additional program usage docs
static char doc[] = "See readme for more detailed usage information";

// Program's arguments and options
struct arguments
{
  char* device;
};

// How to parse a single option or argument
static error_t parse_arg(int key, char* arg, struct argp_state* state)
{
  struct arguments* arguments = (struct arguments*)state->input;

  switch (key) {
    case 'd': //
      arguments->device = arg;
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

  // Parse program arguments
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  println("Launching on %s", arguments.device);

  // Open (b)inary files for (w)riting:

  // Raw bytes incoming from serial port
  logSerialIn = fopen("in.bin", "wb");
  // Raw bytes being sent to serial port
  logSerialOut = fopen("out.bin", "wb");
  // Packets describing incoming and outgoing data.
  // Contains no bad blocks of data, replaced by packets reporting those errors.
  logToMonitor = fopen("all.bin", "wb");

  if (!(logSerialIn && logSerialOut && logToMonitor)) {
    println("Failed to open one or more files");
    return 1;
  }

  serialFileno = setupSerial(arguments.device);
  if (serialFileno == -1) {
    println("Failed to setup serial connection on %s", arguments.device);
    return 1;
  }

  PacketSequencer sequencer;
  // shorthand to call sequencer.rewrap
  // Seems like the compiler should be able to deduce the return type,
  // but this alternative fails to compile:
  // auto seq = [&sequencer](WrappedPacket & wrap) { return sequencer.rewrap(wrap); };
  // https://blog.feabhas.com/2014/03/demystifying-c-lambdas/
  auto seq = [&sequencer](WrappedPacket& wrap) -> WrappedPacket& { return sequencer.rewrap(wrap); };

  WrappedPacket heartbeat;
  initializePacket(heartbeat.packet, PacketID::Heartbeat);
  heartbeat.packet.origin = PacketOrigin::HostToTarget;

  // We include a node 0 (broadcast) vfd
  const uint32_t numVfds = 6;
  WrappedPacket freqPkts[numVfds];
  for (uint32_t i = 0; i < numVfds; i++) {
    auto& freqPkt = freqPkts[i];
    // Setup frequency packet for each vfd node address.
    // Start with frequency 0;
    fillFreqPacket(freqPkt, 1, i, 0);
    freqPkt.packet.origin = PacketOrigin::HostToTarget;
  }

  uint32_t selectedVfd = 1;

  // Switch to canonical mode so stdin keystrokes are read immediately,
  // (otherwise user needs to press Enter key).
  // Also disable character echoing.
  // https://man7.org/linux/man-pages/man3/termios.3.html
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &term);

  // https://www.man7.org/linux/man-pages/man2/poll.2.html
  struct pollfd fds[] = { //
                          { fd : STDIN_FILENO, events : POLLIN },
                          { fd : serialFileno, events : POLLIN }
  };

  // Slightly safer than using magic numbers for indices later
  pollfd& stdinPoll = fds[0];
  pollfd& serialPoll = fds[1];

  char serialInBuf[10000];
  // char serialInBuf[maxWrappedPacketLength * 2];
  size_t serialInLen = 0;

  PacketProcesser processer;
  PacketParser parser(processer);

  const uint64_t usBetweenHeartbeats = 1E6;

  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  uint64_t nextHeartbeat = usSince(start);

  bool quit = false;
  while (!quit) {
    // Wait for new data on one of the watched interfaces.
    // Timeout after 1000ms
    int ret = poll(fds, sizeof(fds) / sizeof(fds[0]), 1000);

    // Check for errors
    if (ret == -1) {
      // report error and abort
      perror("Poll() error");
    } else if (ret) { // Data detected
      // Check for incoming serial data
      if (serialPoll.revents & POLLIN) {
        // Reset flag for next polling loop
        serialPoll.revents = 0;

        // Attempt to fill buffer
        ssize_t bytesRead = read(serialPoll.fd, serialInBuf + serialInLen, sizeof(serialInBuf) - serialInLen);
        if (bytesRead == -1) {
          perror("Error reading from serial");
        }

        if (bytesRead <= 0) {
          println("Unexpected: No poll timeout, but only read %ld bytes over serial", bytesRead);
        } else {
          // Write backup copy of data
          if (1 != fwrite(serialInBuf + serialInLen, bytesRead, 1, logSerialIn)) {
            perror("Could not write all serial bytes to in.bin logfile");
          }

          // Attempt to parse packets
          serialInLen += bytesRead;
          serialInLen = parser.extractPackets(serialInBuf, serialInLen);
        }
      }

      // Check for incoming stdin data
      if (stdinPoll.revents & POLLIN) {
        // Reset flag for next polling loop
        stdinPoll.revents = 0;

        // Note that if pasting multiple chars (unlikely in practice)
        // then all trailing chars are picked up on next input pass.
        int c = getchar();
        // println("Got char 0x%X %c", c, c);
        switch (c) {
          case 'q':
            println("Quitting");
            quit = true;
            break;
          case 'h': println("Todo - show help"); break;
          case 'u': {
            auto& freqPkt = freqPkts[selectedVfd];
            auto& data = freqPkt.packet.body.vfdSetFrequency;
            data.frequency++;
            println("Increasing frequency at node %u to %u.%u Hz", //
                    data.node,
                    data.frequency / 10,
                    data.frequency % 10);
            writeToMonitorAndSerialOut(seq(freqPkt));
            break;
          }
          case 'd': {
            auto& freqPkt = freqPkts[selectedVfd];
            auto& data = freqPkt.packet.body.vfdSetFrequency;
            data.frequency = max<uint32_t>(1, data.frequency) - 1;
            println("Decreasing frequency at node %u to %u.%u Hz", //
                    data.node,
                    data.frequency / 10,
                    data.frequency % 10);
            writeToMonitorAndSerialOut(seq(freqPkt));
            break;
          }
          case 'z': {
            auto& freqPkt = freqPkts[selectedVfd];
            auto& data = freqPkt.packet.body.vfdSetFrequency;
            data.frequency = 0;
            println("Zeroing frequency at node %u to %u.%u Hz", //
                    data.node,
                    data.frequency / 10,
                    data.frequency % 10);
            writeToMonitorAndSerialOut(seq(freqPkt));
            break;
          }
          case ' ': {
            for (uint32_t i = 0; i < numVfds; i++) {
              freqPkts[i].packet.body.vfdSetFrequency.frequency = 0;
            }
            auto& freqPkt = freqPkts[0];
            auto& data = freqPkt.packet.body.vfdSetFrequency;
            println("Zeroing all frequencies and broadcasting from node %u frequency %u.%u Hz", //
                    data.node,
                    data.frequency / 10,
                    data.frequency % 10);
            writeToMonitorAndSerialOut(seq(freqPkt));
            break;
          }
          case 'n': {
            selectedVfd = min<uint32_t>(selectedVfd, numVfds - 2) + 1;
            println("Selected next VFD: index %u node %u", //
                    selectedVfd,
                    freqPkts[selectedVfd].packet.body.vfdSetFrequency.node);
            break;
          }
          case 'p': {
            selectedVfd = max<int32_t>(1, selectedVfd) - 1;
            println("Selected previous VFD: index %d node %u", //
                    selectedVfd,
                    freqPkts[selectedVfd].packet.body.vfdSetFrequency.node);
            break;
          }
          default: break;
        }
      }
    } else {
      println("Timeout");
    }

    // Send heartbeat periodically
    uint64_t t = usSince(start);
    if (t > nextHeartbeat) {
      writeToMonitorAndSerialOut(seq(heartbeat));
      nextHeartbeat += usBetweenHeartbeats;
    }

    fflush(logToMonitor);
    fflush(logSerialIn);
    fflush(logSerialOut);
  }

  // Close files
  fclose(logSerialIn);
  fclose(logSerialOut);
  fclose(logToMonitor);

  println("Done");

  return 0;
}
