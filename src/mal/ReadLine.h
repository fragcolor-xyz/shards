/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2020 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#ifndef INCLUDE_READLINE_H
#define INCLUDE_READLINE_H

#include "String.h"

class ReadLine {
public:
  ReadLine(const String &historyFile);
  ~ReadLine();

  bool get(const String &prompt, String &line);

private:
  String m_historyPath;
};

#endif // INCLUDE_READLINE_H
