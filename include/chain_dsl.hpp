/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#ifndef CB_CHAIN_DSL_HPP
#define CB_CHAIN_DSL_HPP

#include "./chainblocks.hpp"

#define DefChain(_name) auto _name = chainblocks::Chain(#_name)
#define Looped() looped(true)

#define Set(_name) block("Set", #_name)
#define Ref(_name) block("Ref", #_name)
#define Get(_name) block("Get", #_name)
#define SetTable(_name, _key) block("Set", #_name, _key)
#define RefTable(_name, _key) block("Ref", #_name, _key)
#define Push(_name) block("Push", #_name)

#define Once(_blocks) block("Once", Blocks()._blocks)
#define Log() block("Log")

#define FS_Read_Bytes() block("FS.Read", true)
#ifdef LoadImage
// mingw defines this
#undef LoadImage
#endif
#define LoadImage(_imagePath) block("LoadImage", _imagePath)

#define GFX_MainWindow(_name, _blocks)                                         \
  block("GFX.MainWindow", _name, Var::Any, Var::Any, Blocks()._blocks)
#define GFX_Camera() block("GFX.Camera")
#define GFX_Shader(_vs, _fs)                                                   \
  block("GFX.Shader", Var::ContextVar(#_vs), Var::ContextVar(#_fs))
#define GFX_Texture2D() block("GFX.Texture2D")

#define GLTF_Load() block("GLTF.Load")
#define GLTF_Load_NoBitangents() block("GLTF.Load", false)
#define GLTF_Draw(_model) block("GLTF.Draw", Var::ContextVar(#_model))
#define GLTF_Draw_WithMaterials(_model, _mats)                                 \
  block("GLTF.Draw", Var::ContextVar(#_model), Var::ContextVar(#_mats))

#endif
