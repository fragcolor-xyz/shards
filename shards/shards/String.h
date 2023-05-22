/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#ifndef INCLUDE_STRING_H
#define INCLUDE_STRING_H

#include <string>
#include <vector>

typedef std::string MalString;
typedef std::vector<MalString> StringVec;

#define STRF stringPrintf
#define PLURAL(n) &("s"[(n) == 1])

extern MalString stringPrintf(const char *fmt, ...);
extern MalString copyAndFree(char *mallocedString);
extern MalString escape(const MalString &s);
extern MalString unescape(const MalString &s);

#endif // INCLUDE_STRING_H
