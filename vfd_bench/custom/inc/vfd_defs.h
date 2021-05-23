/*
 * Register definitions for GS3 VFD
 * https://cdn.automationdirect.com/static/manuals/gs3m/gs3m.pdf
 * Not exhaustive
 */

#pragma once

#include <stdint.h>

/*
 * Converts major and minor parameter indicies to a register address.
 *
 * For example:
 *   p1.05 => paramReg(1,5) => 0x0105
 *   p9.26 => paramReg(9,26) => 0x091A
 */
constexpr uint16_t paramReg(uint8_t major, uint8_t minor)
{
  return major * 16 + minor;
}

// For reading status
const uint16_t statusRegAddress = 0x2100;
const uint16_t statusRegNum = 8;

// For setting frequency
const uint16_t frequencyRegAddress = paramReg(9, 26);

// Datasheet suggests a 5ms delay, but it can often be longer.
// https://community.automationdirect.com/s/question/0D53u00002vQXGGCA4/maximum-response-delay-of-gs-drives
// Note that a longer delay for a lost packet eats into timeout margin.
const uint32_t responseDelayMs = 6;