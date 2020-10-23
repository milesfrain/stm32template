/*
 * Generic functions for setting and clearing DMA interrupt flags
 * without hardcoding the stream.
 * The LL library cannot be used generically, and instead uses
 * 40x as many hardcoded functions for every combination of
 * 5 flags and 8 streams.
 * Note that these generic functions are a hair slower than the
 * hardcoded versions.
 */

#pragma once

enum class DmaFlag : int
{
  FE = 0,
  DME = 2,
  TE = 3,
  HT = 4,
  TC = 5
};

// Flag offset per stream. Repeats for streams 4-7. Note the non-linear pattern.
static constexpr uint32_t flagOffset[] = { 0, 6, 16, 22 };

// Returns a non-zero value if flag is set.
// Also clears the flag - a common pattern.
// There may be a special assembly instruction for this. Not sure if compiler will find it.
uint32_t dmaFlagCheckAndClear(DMA_TypeDef* dma, uint32_t stream, DmaFlag flag)
{
  // Find the bit location of a particular flag for a given stream
  uint32_t mask = 1 << (static_cast<int>(flag) + flagOffset[stream % 4]);
  // Streams 0-3 use LI* (low) interrupt registers and 4-7 use HI* (high) interrupt registers
  if (stream < 4) {
    // Check flag
    uint32_t result = dma->LISR & mask;
    // Clear flag by writing a 1 to each bit to clear
    dma->LIFCR = mask;
    // Return whether flag was set
    return result;
  } else {
    // Check flag
    uint32_t result = dma->HISR & mask;
    // Clear flag by writing a 1 to each bit to clear
    dma->HIFCR_OOPS = mask;
    // Return whether flag was set
    return result;
  }
}
