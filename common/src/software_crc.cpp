/*
 * CRC16 function adapted from:
 * https://cdn.automationdirect.com/static/manuals/gs3m/gs3m.pdf
 * Page 5-70
 *
 * See this page for origin of magic numbers:
 * https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations_of_cyclic_redundancy_checks
 *
 * Some other helpful tools and resources:
 * 	http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
 * 	https://crccalc.com/
 *
 * Note that STM32 only has special hardware support for 32-bit CRC.
 * 	 - Will need to be careful to only allow exclusive HW CRC access.
 *
 * Todo benchmark:
 * 	- 32 bit slow, vs byte lookup, vs word lookup (double-check storage cost), vs HW
 * 	- 16 bit slow, vs byte lookup, vs word lookup
 *
 * Can use crcany to generate fast lookup tables:
 * https://github.com/madler/crcany
 * Byte-lookup probably best middle-ground for easy architecture interop (no endianness issues).
 *
 */

#include <stdint.h>

/*
 * CRC-16/Modbus, uses these parameters:
 * Poly    	Init   	RefIn  	RefOut 	XorOut
 * 0x8005 	0xFFFF 	true 	true 	0x0000
 */
uint16_t crc16(const void* dataArg, uint32_t length)
{
  uint16_t reg = -1;
  uint8_t* data = (uint8_t*)dataArg;

  while (length--) {
    reg ^= *data++;

    for (int i = 0; i < 8; i++) {
      uint16_t lsb = reg & 0x01;
      reg >>= 1;
      // Using inverted 0x8005
      if (lsb)
        reg ^= 0xA001;
    }
  }
  return reg;
}

/*
 * CRC-32, uses these parameters:
 * Poly    	    Init   	    RefIn  	RefOut 	XorOut
 * 0x04C11DB7 	0xFFFFFFFF 	true 	true 	0xFFFFFFFF
 *
 * Could skip xorout step to get JAMCRC,
 * same level of integrity, but may mismatch hardware CRC.
 */
uint32_t crc32(const void* dataArg, uint32_t length)
{
  uint32_t reg = -1;
  uint8_t* data = (uint8_t*)dataArg;

  while (length--) {
    reg ^= *data++;

    for (int i = 0; i < 8; i++) {
      uint32_t lsb = reg & 0x01;
      reg >>= 1;
      // Using inverted 0x04C11DB7
      if (lsb)
        reg ^= 0xEDB88320;
    }
  }
  // return reg; // no xorout step
  return ~reg; // with full xorout/inversion step
}
