/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_core_option_input.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <libretro_core_option_input.h>

static int failures = 0;

static void expect_true(const char *name, bool got)
{
   if (!got)
   {
      fprintf(stderr, "FAIL: %s expected true\n", name);
      failures++;
   }
}

static void expect_false(const char *name, bool got)
{
   if (got)
   {
      fprintf(stderr, "FAIL: %s expected false\n", name);
      failures++;
   }
}

static void test_ipv4(void)
{
   struct retro_core_option_input in;

   memset(&in, 0, sizeof(in));
   in.key  = "ip";
   in.type = RETRO_CORE_OPTION_INPUT_IPV4;

   expect_true("ipv4 127.0.0.1",
         retro_core_option_input_validate(&in, "127.0.0.1"));
   expect_true("ipv4 192.168.1.10",
         retro_core_option_input_validate(&in, "192.168.1.10"));
   expect_true("ipv4 0.0.0.0",
         retro_core_option_input_validate(&in, "0.0.0.0"));
   expect_true("ipv4 255.255.255.255",
         retro_core_option_input_validate(&in, "255.255.255.255"));

   expect_false("ipv4 leading zero",
         retro_core_option_input_validate(&in, "192.168.01.1"));
   expect_false("ipv4 256",
         retro_core_option_input_validate(&in, "256.0.0.1"));
   expect_false("ipv4 hex",
         retro_core_option_input_validate(&in, "0x7f.0.0.1"));
   expect_false("ipv4 short",
         retro_core_option_input_validate(&in, "1.2.3"));
   expect_false("ipv4 junk",
         retro_core_option_input_validate(&in, "1.2.3.4.5"));
   expect_false("ipv4 empty",
         retro_core_option_input_validate(&in, ""));
}

static void test_date(void)
{
   struct retro_core_option_input in;

   memset(&in, 0, sizeof(in));
   in.key  = "date";
   in.type = RETRO_CORE_OPTION_INPUT_DATE;

   expect_true("date 2024-02-29 leap",
         retro_core_option_input_validate(&in, "2024-02-29"));
   expect_true("date 2000-02-29 leap",
         retro_core_option_input_validate(&in, "2000-02-29"));
   expect_false("date 1900-02-29 not leap",
         retro_core_option_input_validate(&in, "1900-02-29"));
   expect_false("date 2023-02-29",
         retro_core_option_input_validate(&in, "2023-02-29"));
   expect_false("date 2024-13-01",
         retro_core_option_input_validate(&in, "2024-13-01"));
   expect_false("date 2024-00-01",
         retro_core_option_input_validate(&in, "2024-00-01"));
   expect_false("date slash",
         retro_core_option_input_validate(&in, "2024/02/01"));
   expect_true("date 2024-01-31",
         retro_core_option_input_validate(&in, "2024-01-31"));
}

static void test_float(void)
{
   struct retro_core_option_input in;

   memset(&in, 0, sizeof(in));
   in.key      = "gain";
   in.type     = RETRO_CORE_OPTION_INPUT_FLOAT;
   in.min      = -80.0;
   in.max      = 12.0;
   in.step     = 0.5;
   in.decimals = 1;

   expect_true("float 0.0",
         retro_core_option_input_validate(&in, "0.0"));
   expect_true("float -80.0",
         retro_core_option_input_validate(&in, "-80.0"));
   expect_true("float 12.0",
         retro_core_option_input_validate(&in, "12.0"));
   expect_true("float 0.5",
         retro_core_option_input_validate(&in, "0.5"));
   expect_false("float out of range",
         retro_core_option_input_validate(&in, "12.5"));
   expect_false("float bad step",
         retro_core_option_input_validate(&in, "0.25"));
   expect_false("float too many decimals",
         retro_core_option_input_validate(&in, "1.00"));
}

static void test_uint_port(void)
{
   struct retro_core_option_input in;

   memset(&in, 0, sizeof(in));
   in.key  = "port";
   in.type = RETRO_CORE_OPTION_INPUT_UINT;
   in.min  = 1.0;
   in.max  = 65535.0;
   in.step = 1.0;

   expect_true("port 26760",
         retro_core_option_input_validate(&in, "26760"));
   expect_true("port 1",
         retro_core_option_input_validate(&in, "1"));
   expect_false("port 0",
         retro_core_option_input_validate(&in, "0"));
   expect_false("port 65536",
         retro_core_option_input_validate(&in, "65536"));
   expect_false("port leading zero",
         retro_core_option_input_validate(&in, "026760"));
   expect_false("port sign",
         retro_core_option_input_validate(&in, "-1"));
}

static void test_custom_pattern(void)
{
   struct retro_core_option_input in;
   char overlong[RETRO_CORE_OPTION_INPUT_PATTERN_MAX + 8];
   unsigned i;

   memset(&in, 0, sizeof(in));
   in.key        = "serial";
   in.type       = RETRO_CORE_OPTION_INPUT_CUSTOM;
   in.min_length = 1;
   in.max_length = 8;
   in.pattern    = "[0-9A-Fa-f]{1,8}";

   expect_true("hex AB",
         retro_core_option_input_validate(&in, "AB"));
   expect_true("hex deadbeef",
         retro_core_option_input_validate(&in, "deadbeef"));
   expect_false("hex too long",
         retro_core_option_input_validate(&in, "deadbeef0"));
   expect_false("hex empty",
         retro_core_option_input_validate(&in, ""));
   expect_false("hex bad char",
         retro_core_option_input_validate(&in, "GG"));

   in.pattern = "abc|def";
   expect_false("reject alternation",
         retro_core_option_input_validate(&in, "abc"));

   in.pattern = ".*";
   expect_false("reject wildcard dot",
         retro_core_option_input_validate(&in, "a"));

   in.pattern = "(abc)";
   expect_false("reject groups",
         retro_core_option_input_validate(&in, "abc"));

   for (i = 0; i < RETRO_CORE_OPTION_INPUT_PATTERN_MAX + 4; i++)
      overlong[i] = 'a';
   overlong[RETRO_CORE_OPTION_INPUT_PATTERN_MAX + 4] = '\0';
   in.pattern = overlong;
   expect_false("reject overlong pattern",
         retro_core_option_input_validate(&in, "a"));

   in.pattern = "aaaaaaaaaaaaaaaaa"; /* 17 literal atoms */
   expect_false("reject too many atoms",
         retro_core_option_input_validate(&in, "aaaaaaaaaaaaaaaaa"));

   in.pattern = "ID-[0-9]{3}";
   expect_true("lit+class ID-042",
         retro_core_option_input_validate(&in, "ID-042"));
   expect_false("lit+class ID-42",
         retro_core_option_input_validate(&in, "ID-42"));
}

static void test_string(void)
{
   struct retro_core_option_input in;

   memset(&in, 0, sizeof(in));
   in.key        = "name";
   in.type       = RETRO_CORE_OPTION_INPUT_STRING;
   in.min_length = 1;
   in.max_length = 16;

   expect_true("string hello",
         retro_core_option_input_validate(&in, "hello"));
   expect_false("string empty",
         retro_core_option_input_validate(&in, ""));
   expect_false("string control",
         retro_core_option_input_validate(&in, "a\nb"));

   in.allowed_chars = "abc";
   expect_true("string charset",
         retro_core_option_input_validate(&in, "cab"));
   expect_false("string charset miss",
         retro_core_option_input_validate(&in, "cad"));
}

int main(void)
{
   test_ipv4();
   test_date();
   test_float();
   test_uint_port();
   test_custom_pattern();
   test_string();

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return EXIT_FAILURE;
   }

   printf("test_core_option_input: ok\n");
   return EXIT_SUCCESS;
}
