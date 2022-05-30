/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// #include "shared.hpp"
// #include <boost/process/environment.hpp>

// namespace shards {
// namespace OS {
// struct GetEnv {
//   std::string _value;

//   static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
//   static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

//    SHVar activate(SHContext *context, const SHVar &input) {
//     auto envs = boost::this_process::environment();
//     _value = envs[_name(context).payload.stringValue].to_string();
//     return Var(_value);
//   }
// };

// struct SetEnv {
//   static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
//   static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

//    SHVar activate(SHContext *context, const SHVar &input) {
//     auto envs = boost::this_process::environment();
//     envs[_name(context).payload.stringValue] = input.payload.stringValue;
//     return input;
//   }
// };

// struct AddEnv {
//   static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
//   static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

//    SHVar activate(SHContext *context, const SHVar &input) {
//     auto envs = boost::this_process::environment();
//     envs[_name(context).payload.stringValue] += input.payload.stringValue;
//     return input;
//   }
// };

// struct UnsetEnv {
//   static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
//   static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

//    SHVar activate(SHContext *context, const SHVar &input) {
//     auto envs = boost::this_process::environment();
//     envs[_name(context).payload.stringValue].clear();
//     return input;
//   }
// };

// } // namespace OS

// void registerOSShards() {
//   REGISTER_SHARD("OS.GetEnv", OS::GetEnv);
//   REGISTER_SHARD("OS.SetEnv", OS::SetEnv);
//   REGISTER_SHARD("OS.AddEnv", OS::AddEnv);
//   REGISTER_SHARD("OS.UnsetEnv", OS::UnsetEnv);
// }
// } // namespace shards
