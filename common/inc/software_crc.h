#pragma once

#include <stdint.h>

uint16_t crc16(const void* data, uint32_t length);

uint32_t crc32(const void* data, uint32_t length);
