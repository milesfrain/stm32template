#include "CppUTest/TestHarness.h"

#include "basic.h"

TEST_GROUP(TestMin){ void setup(){} void teardown(){} };

TEST(TestMin, min_int)
{
  LONGS_EQUAL(3, min(3, 4));
  LONGS_EQUAL(4, min(5, 4));
}

TEST_GROUP(TestConcat){ void setup(){} void teardown(){} };

TEST(TestConcat, concat_12)
{
  char dst[10];

  concat(dst, "one", "two", sizeof(dst));
  STRNCMP_EQUAL("onetwo", dst, sizeof(dst));

  concat(dst, "one", "second is too big", sizeof(dst));
  STRNCMP_EQUAL("onesecond ", dst, sizeof(dst));

  concat(dst, "first is too big", "two", sizeof(dst));
  STRNCMP_EQUAL("first is t", dst, sizeof(dst));

  concat(dst, "just one", "", sizeof(dst));
  STRNCMP_EQUAL("just one", dst, sizeof(dst));

  concat(dst, "", "just two", sizeof(dst));
  STRNCMP_EQUAL("just two", dst, sizeof(dst));
}
