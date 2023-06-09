/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#include "Types.h"

int checkArgsIs(const char *name, int expected, int got, malValuePtr val) {
  MAL_CHECK(got == expected, "\"%s\" expects %d arg%s, %d supplied, line: %u", name, expected, PLURAL(expected), got, val->line);
  return got;
}

int checkArgsBetween(const char *name, int min, int max, int got, malValuePtr val) {
  MAL_CHECK((got >= min) && (got <= max), "\"%s\" expects between %d and %d arg%s, %d supplied, line: %u", name, min, max,
            PLURAL(max), got, val->line);
  return got;
}

int checkArgsAtLeast(const char *name, int min, int got, malValuePtr val) {
  MAL_CHECK(got >= min, "\"%s\" expects at least %d arg%s, %d supplied, line: %u", name, min, PLURAL(min), got, val->line);
  return got;
}

int checkArgsEven(const char *name, int got, malValuePtr val) {
  MAL_CHECK(got % 2 == 0, "\"%s\" expects an even number of args, %d supplied, line: %u", name, got, val->line);
  return got;
}
