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

typedef BlockWrapper<GetEnv> GetEnvBlock;
typedef BlockWrapper<SetEnv> SetEnvBlock;
typedef BlockWrapper<AddEnv> AddEnvBlock;
typedef BlockWrapper<UnsetEnv> UnsetEnvBlock;
} // namespace OS

void registerOSBlocks() {
  registerBlock("OS.GetEnv", &OS::GetEnvBlock::create);
  registerBlock("OS.SetEnv", &OS::SetEnvBlock::create);
  registerBlock("OS.AddEnv", &OS::AddEnvBlock::create);
  registerBlock("OS.UnsetEnv", &OS::UnsetEnvBlock::create);
}
} // namespace chainblocks
