/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#include "shared.hpp"
#include <boost/process/environment.hpp>

namespace chainblocks {
namespace OS {
struct GetEnv {
  std::string _value;
  ParamVar _name;

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto envs = boost::this_process::environment();
    _value = envs[_name(context).payload.stringValue].to_string();
    return Var(_value);
  }
};

struct SetEnv {
  ParamVar _name;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto envs = boost::this_process::environment();
    envs[_name(context).payload.stringValue] = input.payload.stringValue;
    return input;
  }
};

struct AddEnv {
  ParamVar _name;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto envs = boost::this_process::environment();
    envs[_name(context).payload.stringValue] += input.payload.stringValue;
    return input;
  }
};

struct UnsetEnv {
  ParamVar _name;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ALWAYS_INLINE CBVar activate(CBContext *context, const CBVar &input) {
    auto envs = boost::this_process::environment();
    envs[_name(context).payload.stringValue].clear();
    return input;
  }
};

} // namespace OS

void registerOSBlocks() {
  REGISTER_CBLOCK("OS.GetEnv", OS::GetEnv);
  REGISTER_CBLOCK("OS.SetEnv", OS::SetEnv);
  REGISTER_CBLOCK("OS.AddEnv", OS::AddEnv);
  REGISTER_CBLOCK("OS.UnsetEnv", OS::UnsetEnv);
}
} // namespace chainblocks
