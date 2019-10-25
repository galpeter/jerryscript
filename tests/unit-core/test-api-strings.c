/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jerryscript.h"

#include "test-common.h"

static bool
strict_equals (jerry_value_t a, /**< the first string to compare */
               jerry_value_t b) /**< the second string to compare */
{
  const jerry_char_t is_equal_src[] = "var isEqual = function(a, b) { return (a === b); }; isEqual";
  jerry_value_t is_equal_fn_val = jerry_eval (is_equal_src, sizeof (is_equal_src) - 1, JERRY_PARSE_NO_OPTS);
  TEST_ASSERT (!jerry_value_is_error (is_equal_fn_val));
  jerry_value_t args[2] = {a, b};
  jerry_value_t res = jerry_call_function (is_equal_fn_val, jerry_create_undefined (), args, 2);
  TEST_ASSERT (!jerry_value_is_error (res));
  TEST_ASSERT (jerry_value_is_boolean (res));
  bool is_strict_equal = jerry_get_boolean_value (res);
  jerry_release_value (res);
  jerry_release_value (is_equal_fn_val);
  return is_strict_equal;
} /* strict_equals */

static jerry_value_t
call_function (const char* args_str_p, const char* src_p, jerry_value_t args[], jerry_size_t argc)
{
    jerry_value_t function_value = jerry_parse_function (NULL, 0,
                                                         (const jerry_char_t *) args_str_p, strlen (args_str_p),
                                                         (const jerry_char_t *) src_p, strlen (src_p),
                                                         JERRY_PARSE_NO_OPTS);
    TEST_ASSERT (!jerry_value_is_error (function_value));

    jerry_value_t result = jerry_call_function (function_value, jerry_create_undefined (), args, argc);
    TEST_ASSERT (!jerry_value_is_error (result));
    jerry_release_value (function_value);

    return result;
}

static void
string_compare (jerry_value_t string_value, const char *expected_string)
{
  TEST_ASSERT (jerry_value_is_string (string_value));
  jerry_size_t str_size = jerry_get_string_size (string_value);
  JERRY_VLA (char, str_p, str_size + 20);
  memset (str_p, 0xFF, str_size + 20);

  jerry_size_t copied = jerry_string_to_char_buffer (string_value, (jerry_char_t *) str_p, str_size);
  TEST_ASSERT (copied == str_size);

  TEST_ASSERT (memcmp (str_p, expected_string, strlen (expected_string)) == 0);
  TEST_ASSERT (str_p[str_size] == (char)0xFF);
}

static void
test_external_strings_no_free (void)
{
  static char demo_str[] = "Hello";

  jerry_value_t str_ext_a = jerry_create_string_external ((const jerry_char_t *) demo_str, (jerry_size_t) strlen (demo_str), NULL);

  string_compare (str_ext_a, demo_str);
  {
    jerry_value_t test_hello_result = call_function ("arg", "return arg == 'Hello';", &str_ext_a, 1);
    TEST_ASSERT (jerry_value_is_boolean (test_hello_result));

    bool is_equal = jerry_get_boolean_value (test_hello_result);
    TEST_ASSERT (is_equal);

    jerry_release_value (test_hello_result);
  }

  {
    jerry_value_t test_concat_result = call_function ("arg", "return arg + 'Hello';", &str_ext_a, 1);
    string_compare (test_concat_result, "HelloHello");
    jerry_release_value (test_concat_result);
  }

  {
    jerry_value_t test_trim_result = call_function ("arg", "return arg.trim();", &str_ext_a, 1);
    string_compare (test_trim_result, "Hello");

    jerry_release_value (test_trim_result);
  }

  jerry_release_value (str_ext_a);

} /* test_external_strings */

static bool external_free_called;

static void
external_str_free_cb (void *str_data_p)
{
  external_free_called = true;
  free (str_data_p);
}

static void
test_external_strings_free (void)
{
  external_free_called = false;

  const size_t data_size = 50 * sizeof (char);
  char *demo_str = (char *) malloc (data_size);

  memset (demo_str, 0xFF, data_size);
  strncpy (demo_str, "Hello", 5 + 1);

  jerry_value_t str_ext_a = jerry_create_string_external ((const jerry_char_t *) demo_str,
                                                          (jerry_size_t) strlen (demo_str),
                                                          external_str_free_cb);
  string_compare (str_ext_a, demo_str);
  {
    jerry_value_t test_hello_result = call_function ("arg", "return arg == 'Hello';", &str_ext_a, 1);
    TEST_ASSERT (jerry_value_is_boolean (test_hello_result));

    bool is_equal = jerry_get_boolean_value (test_hello_result);
    TEST_ASSERT (is_equal);

    jerry_release_value (test_hello_result);
  }

  {
    jerry_value_t test_concat_result = call_function ("arg", "return arg + 'Hello';", &str_ext_a, 1);
    string_compare (test_concat_result, "HelloHello");
    jerry_release_value (test_concat_result);
  }

  {
    jerry_value_t test_trim_result = call_function ("arg", "return arg.trim();", &str_ext_a, 1);
    string_compare (test_trim_result, "Hello");

    jerry_release_value (test_trim_result);
  }

  jerry_release_value (str_ext_a);

  /* Force the gc to remove the string and call the external free callback. */
  jerry_gc (JERRY_GC_PRESSURE_HIGH);

  TEST_ASSERT (external_free_called == true);
} /* test_external_strings */

int
main (void)
{
  jerry_size_t sz, utf8_sz, cesu8_sz;
  jerry_length_t cesu8_length, utf8_length;
  jerry_value_t args[2];

  TEST_INIT ();
  jerry_init (JERRY_INIT_EMPTY);

  /* Test corner case for jerry_string_to_char_buffer */
  args[0] = jerry_create_string ((jerry_char_t *) "");
  sz = jerry_get_string_size (args[0]);
  TEST_ASSERT (sz == 0);
  jerry_release_value (args[0]);

  /* Test create_jerry_string_from_utf8 with 4-byte long unicode sequences,
   * test string: 'str: {DESERET CAPITAL LETTER LONG I}'
   */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x73\x74\x72\x3a \xf0\x90\x90\x80");
  args[1] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");

  /* These sizes must be equal */
  utf8_sz = jerry_get_string_size (args[0]);
  cesu8_sz = jerry_get_string_size (args[1]);

  JERRY_VLA (char, string_from_utf8, utf8_sz);
  JERRY_VLA (char, string_from_cesu8, cesu8_sz);

  jerry_string_to_char_buffer (args[0], (jerry_char_t *) string_from_utf8, utf8_sz);
  jerry_string_to_char_buffer (args[1], (jerry_char_t *) string_from_cesu8, cesu8_sz);

  TEST_ASSERT (utf8_sz == cesu8_sz);

  TEST_ASSERT (!strncmp (string_from_utf8, string_from_cesu8, utf8_sz));
  jerry_release_value (args[0]);
  jerry_release_value (args[1]);

  /* Test jerry_string_to_utf8_char_buffer, test string: 'str: {DESERET CAPITAL LETTER LONG I}' */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x73\x74\x72\x3a \xf0\x90\x90\x80");
  args[1] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");

  /* Test that the strings are equal / ensure hashes are equal */
  TEST_ASSERT (strict_equals (args[0], args[1]));

  /* These sizes must be equal */
  utf8_sz = jerry_get_utf8_string_size (args[0]);
  cesu8_sz = jerry_get_utf8_string_size (args[1]);

  TEST_ASSERT (utf8_sz == cesu8_sz);

  JERRY_VLA (char, string_from_utf8_string, utf8_sz);
  JERRY_VLA (char, string_from_cesu8_string, cesu8_sz);

  jerry_string_to_utf8_char_buffer (args[0], (jerry_char_t *) string_from_utf8_string, utf8_sz);
  jerry_string_to_utf8_char_buffer (args[1], (jerry_char_t *) string_from_cesu8_string, cesu8_sz);

  TEST_ASSERT (!strncmp (string_from_utf8_string, string_from_cesu8_string, utf8_sz));
  jerry_release_value (args[0]);
  jerry_release_value (args[1]);

  /* Test string: 'str: {MATHEMATICAL FRAKTUR SMALL F}{MATHEMATICAL FRAKTUR SMALL G}' */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4");

  cesu8_length = jerry_get_string_length (args[0]);
  utf8_length = jerry_get_utf8_string_length (args[0]);

  cesu8_sz = jerry_get_string_size (args[0]);
  utf8_sz =  jerry_get_utf8_string_size (args[0]);

  TEST_ASSERT (cesu8_length == 10 && utf8_length == 8);
  TEST_ASSERT (cesu8_sz != utf8_sz);
  TEST_ASSERT (utf8_sz == 14 && cesu8_sz == 18);

  JERRY_VLA (char, test_string, utf8_sz);

  TEST_ASSERT (jerry_string_to_utf8_char_buffer (args[0], (jerry_char_t *) test_string, utf8_sz) == 14);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4", utf8_sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 14);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length + 1, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 14);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 \xf0\x9d\x94\xa4", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], utf8_length, 0, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 0);

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length, (jerry_char_t *) test_string, utf8_sz - 1);
  TEST_ASSERT (sz == 10);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 ", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0], 0, utf8_length - 1, (jerry_char_t *) test_string, utf8_sz);
  TEST_ASSERT (sz == 10);
  TEST_ASSERT (!strncmp (test_string, "\x73\x74\x72\x3a \xf0\x9d\x94\xa3 ", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0],
                                            utf8_length - 2,
                                            utf8_length - 1,
                                            (jerry_char_t *) test_string,
                                            utf8_sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (test_string, " ", sz));

  sz = jerry_substring_to_utf8_char_buffer (args[0],
                                            utf8_length - 3,
                                            utf8_length - 2,
                                            (jerry_char_t *) test_string,
                                            utf8_sz);
  TEST_ASSERT (sz == 4);
  TEST_ASSERT (!strncmp (test_string, "\xf0\x9d\x94\xa3", sz));

  jerry_release_value (args[0]);

  /* Test string: 'str: {DESERET CAPITAL LETTER LONG I}' */
  args[0] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x81\xed\xb0\x80");

  cesu8_length = jerry_get_string_length (args[0]);
  utf8_length = jerry_get_utf8_string_length (args[0]);

  cesu8_sz = jerry_get_string_size (args[0]);
  utf8_sz =  jerry_get_utf8_string_size (args[0]);

  TEST_ASSERT (cesu8_length == 7 && utf8_length == 6);
  TEST_ASSERT (cesu8_sz != utf8_sz);
  TEST_ASSERT (utf8_sz == 9 && cesu8_sz == 11);

  jerry_release_value (args[0]);

  /* Test string: 'price: 10{EURO SIGN}' */
  args[0] = jerry_create_string_from_utf8 ((jerry_char_t *) "\x70\x72\x69\x63\x65\x3a \x31\x30\xe2\x82\xac");

  cesu8_length = jerry_get_string_length (args[0]);
  utf8_length = jerry_get_utf8_string_length (args[0]);

  cesu8_sz = jerry_get_string_size (args[0]);
  utf8_sz =  jerry_get_utf8_string_size (args[0]);

  TEST_ASSERT (cesu8_length == utf8_length);
  TEST_ASSERT (cesu8_length == 10);
  TEST_ASSERT (cesu8_sz == utf8_sz);
  TEST_ASSERT (utf8_sz == 12);
  jerry_release_value (args[0]);

  /* Test string: '3' */
  {
    jerry_value_t test_str = jerry_create_string ((const jerry_char_t *) "3");
    char result_string[1] = { 'E' };
    jerry_size_t copied_utf8 = jerry_substring_to_utf8_char_buffer (test_str,
                                                                    0,
                                                                    1,
                                                                    (jerry_char_t *) result_string,
                                                                    sizeof (result_string));
    TEST_ASSERT (copied_utf8 == 1);
    TEST_ASSERT (result_string[0] == '3');

    result_string[0] = 'E';
    jerry_size_t copied = jerry_substring_to_char_buffer (test_str,
                                                          0,
                                                          1,
                                                          (jerry_char_t *) result_string,
                                                          sizeof (result_string));
    TEST_ASSERT (copied == 1);
    TEST_ASSERT (result_string[0] == '3');

    jerry_release_value (test_str);
  }

  /* Test jerry_substring_to_char_buffer */
  args[0] = jerry_create_string ((jerry_char_t *) "an ascii string");

  /* Buffer size */
  cesu8_sz = 5;

  JERRY_VLA (char, substring, cesu8_sz);
  sz = jerry_substring_to_char_buffer (args[0], 3, 8, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 5);
  TEST_ASSERT (!strncmp (substring, "ascii", sz));

  /* Buffer size is 5, substring length is 11 => copied only the first 5 char */
  sz = jerry_substring_to_char_buffer (args[0], 0, 11, (jerry_char_t *) substring, cesu8_sz);

  TEST_ASSERT (sz == 5);
  TEST_ASSERT (!strncmp (substring, "an as", sz));

  /* Position of the first character is greater than the string length */
  sz = jerry_substring_to_char_buffer (args[0], 16, 21, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 0);

  sz = jerry_substring_to_char_buffer (args[0], 14, 15, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (substring, "g", sz));

  sz = jerry_substring_to_char_buffer (args[0], 0, 1, (jerry_char_t *) substring, cesu8_sz);
  TEST_ASSERT (sz == 1);
  TEST_ASSERT (!strncmp (substring, "a", sz));

  cesu8_length = jerry_get_string_length (args[0]);
  cesu8_sz = jerry_get_string_size (args[0]);
  TEST_ASSERT (cesu8_length == 15);
  TEST_ASSERT (cesu8_length == cesu8_sz);

  JERRY_VLA (char, fullstring, cesu8_sz);
  sz = jerry_substring_to_char_buffer (args[0], 0, cesu8_length, (jerry_char_t *) fullstring, cesu8_sz);
  TEST_ASSERT (sz == 15);
  TEST_ASSERT (!strncmp (fullstring, "an ascii string", sz));

  jerry_release_value (args[0]);

  /* Test jerry_substring_to_char_buffer: '0101' */
  args[0] = jerry_create_string ((jerry_char_t *) "0101");
  cesu8_sz = jerry_get_string_size (args[0]);

  JERRY_VLA (char, number_substring, cesu8_sz);

  sz = jerry_substring_to_char_buffer (args[0], 1, 3, (jerry_char_t *) number_substring, cesu8_sz);
  TEST_ASSERT (sz == 2);
  TEST_ASSERT (!strncmp (number_substring, "10", sz));

  jerry_release_value (args[0]);

  /* Test jerry_substring_to_char_buffer: 'str: {greek zero sign}' */
  args[0] = jerry_create_string ((jerry_char_t *) "\x73\x74\x72\x3a \xed\xa0\x80\xed\xb6\x8a");
  cesu8_sz = jerry_get_string_size (args[0]);
  cesu8_length = jerry_get_string_length (args[0]);
  TEST_ASSERT (cesu8_sz == 11);
  TEST_ASSERT (cesu8_length = 7);

  JERRY_VLA (char, supl_substring, cesu8_sz);

  sz = jerry_substring_to_char_buffer (args[0], 0, cesu8_length, (jerry_char_t *) supl_substring, cesu8_sz);
  TEST_ASSERT (sz == 11);
  TEST_ASSERT (!strncmp (supl_substring, "\x73\x74\x72\x3a \xed\xa0\x80\xed\xb6\x8a", sz));

  /* Decrease the buffer size => the low surrogate char will not fit into the buffer */
  cesu8_sz -= 1;
  sz = jerry_substring_to_char_buffer (args[0], 0, cesu8_length, (jerry_char_t *) supl_substring, cesu8_sz);
  TEST_ASSERT (sz == 8);
  TEST_ASSERT (!strncmp (supl_substring, "\x73\x74\x72\x3a \xed\xa0\x80", sz));

  sz = jerry_substring_to_char_buffer (args[0],
                                       cesu8_length - 1,
                                       cesu8_length,
                                       (jerry_char_t *) supl_substring,
                                       cesu8_sz);
  TEST_ASSERT (sz == 3);
  TEST_ASSERT (!strncmp (supl_substring, "\xed\xb6\x8a", sz));

  sz = jerry_substring_to_char_buffer (args[0],
                                       cesu8_length - 2,
                                       cesu8_length - 1,
                                       (jerry_char_t *) supl_substring,
                                       cesu8_sz);
  TEST_ASSERT (sz == 3);
  TEST_ASSERT (!strncmp (supl_substring, "\xed\xa0\x80", sz));

  jerry_release_value (args[0]);

  test_external_strings_no_free ();
  test_external_strings_free ();

  jerry_cleanup ();

  return 0;
} /* main */
