/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2019 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#ifndef INCLUDE_ENVIRONMENT_H
#define INCLUDE_ENVIRONMENT_H

#include "MAL.h"

#include <map>
#include <set>

class malEnv : public RefCounted {
public:
  malEnv(malEnvPtr outer = NULL);
  malEnv(malEnvPtr outer, const StringVec &bindings, malValueIter argsBegin, malValueIter argsEnd);

  ~malEnv();

  malValuePtr get(const MalString &symbol);
  malEnvPtr find(const MalString &symbol);
  malValuePtr set(const MalString &symbol, malValuePtr value);
  malEnvPtr getRoot();

  static void setPrefix(const MalString &prefix);
  static void unsetPrefix();

  // Adds a tacking dependency to a loaded file
  void trackDependency(const char *path);
  const std::set<std::string> &getTrackedDependencies() const { return m_trackedDependencies; }

public:
  bool trackDependencies{};

private:
  typedef std::map<MalString, malValuePtr> Map;
  Map m_map;
  malEnvPtr m_outer;

  std::set<std::string> m_trackedDependencies;
};

#endif // INCLUDE_ENVIRONMENT_H
