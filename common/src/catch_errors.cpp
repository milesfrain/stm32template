/*
 * See header for notes.
 */

#include "board_defs.h"
#include <stdbool.h>
// Must include catch_errors.h here to apply extern C to these functions
// and disable C++ name mangling for compatibility with C callers.
#include "catch_errors.h"

// All errors / warnings / infos timeouts feed to here.
// Allows using a single breakpoint.
void catchAll()
{
  __asm volatile("nop");
}

// A more reasonable default breakpoint location.
// Excludes breaking on common benignTimeout().
void catchDefault()
{
  catchAll();
}

// Set a breakpoint here to cycle through all blocking tasks
void allTimeouts()
{
  __asm volatile("nop");
}

// Checks if value is true.
// Crashes if value is false.
void assert(bool value)
{
  if (!value) {
    critical();
  }
}

// For trapping critical errors
void critical()
{
  catchDefault();

  // Maybe this should blink instead to avoid confusion with other red LEDs on board.
  // Note that multiple tasks can encounter this critical loop, so blink should not
  // have 50-50 duty cycle, since that can conceal LED.
  LL_GPIO_SetOutputPin(RedLedPort, RedLedPin);

  while (1)
    ;
}

// For trapping nonCritical errors / warnings
void nonCritical()
{
  catchDefault();
}

// This is a timeout to indicate non-optimal behavior,
// such as waiting to write to a full buffer.
void timeout()
{
  catchDefault();
  allTimeouts();
}

// This is a timeout to indicate expected blocking,
// such as waiting to read from an empty buffer.
void benignTimeout()
{
  allTimeouts();
  catchAll();
}
