/*
 * Catches if new or delete are ever unexpectedly called.
 * We don't want any dynamic allocation.
 * You can test if these are triggered by adding this line anywhere in the code:
 *   char * foo = new char[9];
 */

#include "logging.h"

void* operator new(size_t size)
{
  critical();
  return (void*)0;
}
void* operator new[](size_t size)
{
  critical();
  return (void*)0;
}
void operator delete(void* ptr)
{
  critical();
}

void operator delete[](void* ptr)
{
  critical();
}
