/*
 * Catches if new or delete are ever unexpectedly called.
 * We don't want any dynamic allocation.
 * You can test if these are triggered by adding this line anywhere in the code:
 *   char * foo = new char[9];
 */

#pragma once

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr);
void operator delete[](void* ptr);
