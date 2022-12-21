/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2019 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#ifndef INCLUDE_ENVIRONMENT_H
#define INCLUDE_ENVIRONMENT_H

#include "MAL.h"

#include <map>

class malEnv : public RefCounted {
public:
  malEnv(malEnvPtr outer = NULL);
  malEnv(malEnvPtr outer, const StringVec &bindings, malValueIter argsBegin, malValueIter argsEnd);

  ~malEnv();

  malValuePtr get(const MalString &symbol);
  malEnvPtr find(const MalString &symbol);
  malValuePtr set(const MalString &symbol, malValuePtr value);
  malEnvPtr getRoot();

  void setPrefix(const MalString &prefix) { m_prefix = prefix; }
  void unsetPrefix() { m_prefix.clear(); }

private:
  typedef std::map<MalString, malValuePtr> Map;
  MalString m_prefix;
  Map m_map;
  malEnvPtr m_outer;
};

#endif // INCLUDE_ENVIRONMENT_H
