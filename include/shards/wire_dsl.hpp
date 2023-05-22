/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#ifndef SH_WIRE_DSL_HPP
#define SH_WIRE_DSL_HPP

#include "./shards.hpp"

#define Variable(_name) shards::Var::ContextVar(#_name)

#define Defwire(_name) auto _name = shards::Wire(#_name)
#define Looped() looped(true)

#define None() .let(shards::Var::Empty)

#define Input() shard("Input")
#define Pass() shard("Pass")

#define Set(_name) .shard("Set", #_name)
#define Ref(_name) .shard("Ref", #_name)
#define Get(_name) shard("Get", #_name)
#define SetTable(_name, _key) .shard("Set", #_name, _key)
#define PushTable(_name, _key) .shard("Push", #_name, _key)
#define GetTable(_name, _key) shard("Get", #_name, _key)
#define GetTable_Default(_name, _key, _default) shard("Get", #_name, _key, false, false, _default)
#define Push(_name) .shard("Push", #_name)
#define Take(_idx_or_key) shard("Take", _idx_or_key)
#define Count(_name) shard("Count", shards::Var::ContextVar(#_name))

#define If(_condition, _then, _else) shard("If", Weave()._condition, Weave()._then, Weave()._else)
#define When(_condition, _then) shard("When", Weave()._condition, Weave()._then)
#define Match()
#define Is(_value) shard("Is", _value)
#define IsNot(_value) shard("IsNot", _value)
#define Maybe(_do, _else) shard("Maybe", Weave()._do, Weave()._else)
#define MaybeSilently(_do, _else) shard("Maybe", Weave()._do, Weave()._else, true)

#define Once(_shards) shard("Once", Weave()._shards)
#define Log() shard("Log")
#define Hash() shard("Hash")
#define ToString() shard("ToString")
#define ToHex() shard("ToHex")
#define HexToBytes() shard("HexToBytes")
#define ToBytes() shard("ToBytes")
#define ToBase58() shard("ToBase58")

#define PrependTo(_var) .shard("PrependTo", shards::Var::ContextVar(#_var))
#define AppendTo(_var) .shard("AppendTo", shards::Var::ContextVar(#_var))

#define FS_Read_Bytes() shard("FS.Read", true)
#define FS_Read() shard("FS.Read")
#define FS_Write_Overwriting(_contents) shard("FS.Write", shards::Var::ContextVar(#_contents), true)
#ifdef LoadImage
// mingw defines this
#undef LoadImage
#endif
#define LoadImage(_imagePath) shard("LoadImage", _imagePath)
#define ToJson() shard("ToJson")
#define FromJson() shard("FromJson")

#define Process_Run(_cmd, _args) shard("Process.Run", _cmd, shards::Var::ContextVar(#_args))
#define Wasm_Run(_cmd, _args) shard("Wasm.Run", _cmd, shards::Var::ContextVar(#_args))

#define GFX_MainWindow(_name, _shards) shard("GFX.MainWindow", _name, shards::Var::Any, shards::Var::Any, Weave()._shards)
#define GFX_Camera() shard("GFX.Camera")
#define GFX_Shader(_vs, _fs) shard("GFX.Shader", shards::Var::ContextVar(#_vs), shards::Var::ContextVar(#_fs))
#define GFX_Texture2D() shard("GFX.Texture2D")
#define GFX_CompileShader() shard("GFX.CompileShader")

#define GLTF_Load() shard("GLTF.Load")
#define GLTF_Load_WithTransformBefore(_transform) shard("GLTF.Load", shards::Var::Any, shards::Var::Any, _transform)
#define GLTF_Load_WithTransformAfter(_transform) \
  shard("GLTF.Load", shards::Var::Any, shards::Var::Any, shards::Var::Any, _transform)
#define GLTF_Load_WithTransforms(_before, _after) shard("GLTF.Load", shards::Var::Any, shards::Var::Any, _before, _after)
#define GLTF_Load_NoShaders() shard("GLTF.Load", shards::Var::Any, false)
#define GLTF_Draw(_model) shard("GLTF.Draw", shards::Var::ContextVar(#_model))
#define GLTF_Draw_WithMaterials(_model, _mats) \
  shard("GLTF.Draw", shards::Var::ContextVar(#_model), shards::Var::ContextVar(#_mats))

#define Brotli_Compress() shard("Brotli.Compress")
#define Brotli_Decompress() shard("Brotli.Decompress")

#endif
