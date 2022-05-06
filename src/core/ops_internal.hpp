/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef CB_CORE_OPS_INTERNAL
#define CB_CORE_OPS_INTERNAL

#include <ops.hpp>

#include <spdlog/spdlog.h>

#include "spdlog/fmt/ostr.h" // must be included

std::ostream &operator<<(std::ostream &os, const CBVar &var);
std::ostream &operator<<(std::ostream &os, const CBTypeInfo &t);
std::ostream &operator<<(std::ostream &os, const CBTypesInfo &ts);

#endif // CB_CORE_OPS_INTERNAL
