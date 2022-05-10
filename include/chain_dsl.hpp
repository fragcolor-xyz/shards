/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#ifndef CB_CHAIN_DSL_HPP
#define CB_CHAIN_DSL_HPP

#include "./chainblocks.hpp"

#define Variable(_name) chainblocks::Var::ContextVar(#_name)

#define DefChain(_name) auto _name = chainblocks::Chain(#_name)
#define Looped() looped(true)

#define None() .let(chainblocks::Var::Empty)

#define Input() block("Input")
#define Pass() block("Pass")

#define Set(_name) .block("Set", #_name)
#define Ref(_name) .block("Ref", #_name)
#define Get(_name) block("Get", #_name)
#define SetTable(_name, _key) .block("Set", #_name, _key)
#define PushTable(_name, _key) .block("Push", #_name, _key)
#define GetTable(_name, _key) block("Get", #_name, _key)
#define GetTable_Default(_name, _key, _default) block("Get", #_name, _key, false, false, _default)
#define Push(_name) .block("Push", #_name)
#define Take(_idx_or_key) block("Take", _idx_or_key)
#define Count(_name) block("Count", chainblocks::Var::ContextVar(#_name))

#define If(_condition, _then, _else) block("If", Blocks()._condition, Blocks()._then, Blocks()._else)
#define When(_condition, _then) block("When", Blocks()._condition, Blocks()._then)
#define Match()
#define Is(_value) block("Is", _value)
#define IsNot(_value) block("IsNot", _value)
#define Maybe(_do, _else) block("Maybe", Blocks()._do, Blocks()._else)
#define MaybeSilently(_do, _else) block("Maybe", Blocks()._do, Blocks()._else, true)

#define Once(_blocks) block("Once", Blocks()._blocks)
#define Log() block("Log")
#define Hash() block("Hash")
#define ToString() block("ToString")
#define ToHex() block("ToHex")
#define HexToBytes() block("HexToBytes")
#define ToBytes() block("ToBytes")
#define ToBase58() block("ToBase58")

#define PrependTo(_var) .block("PrependTo", chainblocks::Var::ContextVar(#_var))
#define AppendTo(_var) .block("AppendTo", chainblocks::Var::ContextVar(#_var))

#define FS_Read_Bytes() block("FS.Read", true)
#define FS_Read() block("FS.Read")
#define FS_Write_Overwriting(_contents) block("FS.Write", chainblocks::Var::ContextVar(#_contents), true)
#ifdef LoadImage
// mingw defines this
#undef LoadImage
#endif
#define LoadImage(_imagePath) block("LoadImage", _imagePath)
#define ToJson() block("ToJson")
#define FromJson() block("FromJson")

#define Process_Run(_cmd, _args) block("Process.Run", _cmd, chainblocks::Var::ContextVar(#_args))
#define Wasm_Run(_cmd, _args) block("Wasm.Run", _cmd, chainblocks::Var::ContextVar(#_args))

#define GFX_MainWindow(_name, _blocks) \
  block("GFX.MainWindow", _name, chainblocks::Var::Any, chainblocks::Var::Any, Blocks()._blocks)
#define GFX_Camera() block("GFX.Camera")
#define GFX_Shader(_vs, _fs) block("GFX.Shader", chainblocks::Var::ContextVar(#_vs), chainblocks::Var::ContextVar(#_fs))
#define GFX_Texture2D() block("GFX.Texture2D")
#define GFX_CompileShader() block("GFX.CompileShader")

#define GLTF_Load() block("GLTF.Load")
#define GLTF_Load_WithTransformBefore(_transform) block("GLTF.Load", chainblocks::Var::Any, chainblocks::Var::Any, _transform)
#define GLTF_Load_WithTransformAfter(_transform) \
  block("GLTF.Load", chainblocks::Var::Any, chainblocks::Var::Any, chainblocks::Var::Any, _transform)
#define GLTF_Load_WithTransforms(_before, _after) \
  block("GLTF.Load", chainblocks::Var::Any, chainblocks::Var::Any, _before, _after)
#define GLTF_Load_NoShaders() block("GLTF.Load", chainblocks::Var::Any, false)
#define GLTF_Draw(_model) block("GLTF.Draw", chainblocks::Var::ContextVar(#_model))
#define GLTF_Draw_WithMaterials(_model, _mats) \
  block("GLTF.Draw", chainblocks::Var::ContextVar(#_model), chainblocks::Var::ContextVar(#_mats))

#define Brotli_Compress() block("Brotli.Compress")
#define Brotli_Decompress() block("Brotli.Decompress")

#endif
