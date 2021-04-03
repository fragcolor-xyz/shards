/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#ifndef CB_OPS_INTERNAL_HPP
#define CB_OPS_INTERNAL_HPP

#include <ops.hpp>

#include <spdlog/spdlog.h>

#include "spdlog/fmt/ostr.h" // must be included

std::ostream &operator<<(std::ostream &os, const CBVar &var);
std::ostream &operator<<(std::ostream &os, const CBTypeInfo &t);
std::ostream &operator<<(std::ostream &os, const CBTypesInfo &ts);

#endif
