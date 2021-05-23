#include "CppUTest/TestHarness.h"

#include "bar.h"

// Dummy placeholder test.

TEST_GROUP(TestBar){ void setup(){} void teardown(){} };

TEST(TestBar, operation)
{
  LONGS_EQUAL(12, my_prod(3, 4));
}
