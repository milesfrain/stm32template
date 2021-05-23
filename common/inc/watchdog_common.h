#pragma once

#include <stdint.h>

// How many ticks are allowed to elapse between kicks
// before a timeout is reported.
// Note that this should be shorter than hardware timeout threshold
// if hardware watchdog is enabled.
const uint32_t watchdogTimeoutTicks = 2000;
const uint32_t suggestedTimeoutTicks = watchdogTimeoutTicks / 2;