/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_OPS_INTERNAL
#define SH_CORE_OPS_INTERNAL

#include <ops.hpp>

#include <spdlog/spdlog.h>

#include "spdlog/fmt/ostr.h" // must be included

std::ostream &operator<<(std::ostream &os, const SHVar &var);
std::ostream &operator<<(std::ostream &os, const SHTypeInfo &t);
std::ostream &operator<<(std::ostream &os, const SHTypesInfo &ts);

#endif // SH_CORE_OPS_INTERNAL
