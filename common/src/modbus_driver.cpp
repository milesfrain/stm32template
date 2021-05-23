/*
 * See header for notes.
 */

#include "modbus_driver.h"
#include "basic.h"
#include "board_defs.h"
#include "catch_errors.h"
#include "packet_utils.h"
#include "string.h" // memcmp

ModbusDriver::ModbusDriver( //
  UartTasks& uart,
  uint32_t responseDelayMs,
  Writable& target,
  Packet& packet,
  TaskUtilities& util)
  : uart{ uart }
  , responseDelayMs{ responseDelayMs }
  , target{ target }
  , packet{ packet }
  , util{ util }
{}

// Attempts to send a modbus request.
// Assumes modbus "Request" packet to send is already written to outBuf.
// Returns "Response" packet length upon success, zero upon failure.
// Successful response lives in inBuf.
// Must call shiftOutConsumedBytes() once done with response packet in inBuf.
uint32_t ModbusDriver::sendRequest()
{
  // === Defining the following block of constants here to limit scope ===

  const uint32_t sysFreq = 96'000'000; // cycle counter rollover every 45 seconds
  // Todo - query uart for baudrate, or configure uart from constant
  const uint32_t baudrate = 38'400;
  const uint32_t cyclesPerSymbol = sysFreq / baudrate;             // 2500
  const uint32_t symbolsPerByte = 11;                              // 1 start bit + 8 bits per byte + 2 stop bits
  const uint32_t cyclesPerByte = cyclesPerSymbol * symbolsPerByte; // 27'500
  const uint32_t cyclesPerMs = sysFreq / 1000;                     // 96'000
  // Based on RTOS tick rate (currently at 1000 Hz)
  const uint32_t cyclesPerTick = sysFreq / configTICK_RATE_HZ; // 96'000
  // 3.5 character delay between end of last response and start of next request.
  // Expanding 3.5 to 7 / 2 for improved accuracy.
  const uint32_t cyclesInterMessageDelay = roundUpDiv<uint32_t>(cyclesPerByte * 7, 2);
  // Number of characters of inactivity required for UART idle line detection.
  const uint32_t idleLineChars = 1;
  // How long to wait for modbus server to prepare a response.
  static uint32_t cyclesResponseDelay = responseDelayMs * cyclesPerMs;

  // ===

  ModbusDbgPinHigh();

  // Crash if we made a false assumption about configured system frequency.
  if (SystemCoreClock != sysFreq) {
    error("Modbus system frequency mismatch. Actual %ul, expected %ul", SystemCoreClock, sysFreq);
  }

  // Only ModbusError packets are generated in this module
  setPacketIdAndLength(packet, PacketID::ModbusError);
  packet.body.modbusError.node = outPkt->nodeAddress;
  packet.body.modbusError.command = outPkt->command;

  // Estimate size of response.
  // This needs to happen before packet endianness is flipped during prepare call.
  uint32_t expectedResponseLen = modbusExpectedResponseLength(outPkt);

  size_t outLen = modbusPreparePacketForTransmit(outPkt, ModbusDirection::Request);

  if (!outLen) {
    error("Failed to build modbus packet correctly");
  }

  ModbusDbgPinLow();

  // clear accumulated bus data
  flushInput();

  // Check if we need to wait a bit longer to make the next request.
  uint32_t cyclesSinceLastResponse = DWT->CYCCNT - lastResponseCompletedCycle;
  if (cyclesSinceLastResponse < cyclesInterMessageDelay) {
    uint32_t currentTick = xTaskGetTickCount();
    uint32_t delayCycles = cyclesInterMessageDelay - cyclesSinceLastResponse;
    uint32_t delayTicks = roundUpDiv(delayCycles, cyclesPerTick);
    // Using delayUntil instead of delay, so we don't delay extra due to this print
    util.logln( //
      "Attempting to start next modbus request too early. Must wait %lu cycles, %lu ticks",
      delayCycles,
      delayTicks);
    // Todo - double check whether a delay of 1 tick just waits for the next tick
    // or ensures a delay of a full tick. The former case could result in no delay
    // if we're near the end of the current tick.
    vTaskDelayUntil(&currentTick, delayTicks);
  }

  ModbusDbgPinHigh();

  // Write "Request" packet
  util.write(uart, outBuf, outLen);

  // Hopefully uart tx task unblocks right away (versus another task),
  // so we don't waste time waiting for this packet to be sent.
  // Uart tx task has "AboveNormal" priority.

  // Timing for when to expect modbus response is relative to when "request" packet
  // transmission is initiated.
  uint32_t startTick = xTaskGetTickCount();

  ModbusDbgPinLow();

  uint32_t maxCyclesUntilResponse = cyclesPerByte * (outLen + expectedResponseLen + idleLineChars) + cyclesResponseDelay;

  // freeRTOS timing is pretty coarse (1ms resolution in our case), so we round up.
  uint32_t maxTicksUntilResponse = roundUpDiv(maxCyclesUntilResponse, cyclesPerTick);

#ifdef MODBUS_REQUEST_ECHOING_ENABLED
  ModbusDbgPinHigh();

  // Allowing a fairly long wait for echo. No rush to process echo immediately.
  readMinBytesWithTimeout(outLen, startTick, maxTicksUntilResponse);

  ModbusDbgPinLow();

  // Check if echo matches expectations.

  if (inLen < outLen) {
    // Receiving less than the expected number of echo bytes indicates a bus issue.

    packet.body.modbusError.id = ModbusErrorID::BadEchoNotEnoughBytes;
    packet.body.modbusError.bytes.actual = inLen;
    packet.body.modbusError.bytes.expected = outLen;

    // Report echo error
    util.write(target, &packet, packet.length);

    shiftOutConsumedBytes(inLen);
    return 0;

  } else {
    // Got enough bytes

    // Check if bytes are identical
    if (memcmp(inBuf, outBuf, outLen)) {
      // We could report more details on the particular differences.
      // Not sure if that adds much value.

      packet.body.modbusError.id = ModbusErrorID::BadEchoMismatchedContents;

      // Report echo error
      util.write(target, &packet, packet.length);

      shiftOutConsumedBytes(inLen);
      return 0;
    }

    // Attempt to gracefully handle situation where we got too many bytes
    // by allowing overflow bytes to be start of next "Response" packet.
    if (inLen > outLen) {
      // Non-critical FYI message, but likely points to modbus issue.
      util.logln("Received too many request echo bytes. %lu of %lu", inLen, outLen);
    }
  }

  // Discard echoed bytes
  shiftOutConsumedBytes(outLen);

#endif // MODBUS_REQUEST_ECHOING_ENABLED

  // If sending to broadcast address, don't expect a response
  if (outPkt->nodeAddress == 0) {
    lastResponseCompletedCycle = DWT->CYCCNT;
    // Special return value for broadcast.
    // Not possible to be returned for any other non-broadcast response.
    return 1;
  }

  ModbusDbgPinHigh();

  // Read "Response" packet
  uint32_t waitedTicks = readMinBytesWithTimeout(expectedResponseLen, startTick, maxTicksUntilResponse);

  ModbusDbgPinLow();

  // Use this cycle count to determine when it's safe to send the next request.
  // This value is checked the next time this sendModbusRequest() function is called.
  lastResponseCompletedCycle = DWT->CYCCNT;

#if 0
  // FYI log message about any potential time savings due to quick response.
  waitedTicks = xTaskGetTickCount() - startTick;
  if (waitedTicks != maxTicksUntilResponse) {
    util.logln("Waited %lu of %lu ticks for response", waitedTicks, maxTicksUntilResponse);
  }
#endif

  // == Check that we got a valid response ==

  // First check if we got an exception.
  // Not attempting to distinguish something that looks like an
  // exception (but has bad CRC) from arbitrary bad bytes.
  if (inLen >= ModbusExceptionPktSize &&                                                          //
      (uint8_t)inPkt->command == ((uint8_t)outPkt->command | (uint8_t)FunctionCode::Exception) && //
      modbusValidCrc(inPkt, ModbusExceptionPktSize)) {

    packet.body.modbusError.id = ModbusErrorID::ResponseException;
    packet.body.modbusError.exceptionCode = inPkt->exceptionCode;

    // Report result of modbus request
    util.write(target, &packet, packet.length);

    return ModbusExceptionPktSize;
    // Caller must call shiftOutConsumedBytes() once done with data.

    // Then check that we got enough data.
  } else if (inLen < expectedResponseLen) {

    packet.body.modbusError.id = ModbusErrorID::BadResponseNotEnoughBytes;
    packet.body.modbusError.bytes.actual = inLen;
    packet.body.modbusError.bytes.expected = expectedResponseLen;

    // Report result of modbus request
    util.write(target, &packet, packet.length);

    // Clear bytes
    inLen = 0;
    return 0;

    // Otherwise we got enough bytes, maybe more, which we can gracefully handle.
    // Possible overage will be noted during next flush.

    // Perform the following checks:
    // - Valid CRC - Must check crc before endianness is inverted.
    // - Matching address and command.
    // - Matching length.
  } else if (modbusValidCrc(inPkt, expectedResponseLen) && //
             inPkt->nodeAddress == outPkt->nodeAddress &&  //
             inPkt->command == outPkt->command &&          //
             modbusGetLengthAndSwapEndianness(inPkt, ModbusDirection::Response) == expectedResponseLen) {
    // Packet endianness inverted to uC-friendly format by this point.

    return expectedResponseLen;
    // Caller must call shiftOutConsumedBytes() once done with data.

  } else {

    packet.body.modbusError.id = ModbusErrorID::BadResponseMalformedPacket;

    // Report result of modbus request
    util.write(target, &packet, packet.length);

    shiftOutConsumedBytes(expectedResponseLen);
    return 0;
  }
}

// Checks if there is any unexpected input data on modbus.
// Clears and reports.
void ModbusDriver::flushInput()
{
  // Clear accumulated bus data
  size_t inTotal = inLen;
  // Keep attempting to read while bytes are available. No timeout.
  do {
    // We are discarding these bytes, so just keep rewriting over same buffer
    inLen = uart.read(inBuf, sizeof(inBuf), 0);
    inTotal += inLen;
  } while (inLen);

  if (inTotal) {

    packet.body.modbusError.id = ModbusErrorID::ExtraBytes;
    packet.body.modbusError.bytes.actual = inTotal;
    packet.body.modbusError.bytes.expected = 0;

    // Report result of modbus request
    util.write(target, &packet, packet.length);
  }

  inLen = 0;
}

// Keeps attempting to read until either:
// - We got our target number of bytes.
// - We exceeded our maximum timeout (in reference to a starting tick count).
// Current number of bytes accumulated before this call contributes to total.
// Returns number of ticks waited.
uint32_t ModbusDriver::readMinBytesWithTimeout(size_t targetLen, uint32_t startTick, uint32_t maxTicks)
{
  uint32_t waitedTicks, remainingTimeoutTicks;
  do {
    // Set timeout based on how many ticks we have left to wait for data.
    waitedTicks = xTaskGetTickCount() - startTick;
    remainingTimeoutTicks = 0;
    if (waitedTicks < maxTicks) {
      // We have not yet met nor exceeded timeout
      remainingTimeoutTicks = maxTicks - waitedTicks;
    }

    // block until new data, or timeout
    inLen += uart.read(inBuf + inLen, sizeof(inBuf) - inLen, remainingTimeoutTicks);

  } while (inLen < targetLen && remainingTimeoutTicks);

  return waitedTicks;
}

// Remove len bytes from the front of inBuf and adjust inLen accordingly.
void ModbusDriver::shiftOutConsumedBytes(size_t len)
{
  memmove(inBuf, inBuf + len, inLen - len);
  inLen -= len;
}
