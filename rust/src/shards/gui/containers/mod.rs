/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

struct Scope {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

/// Standalone window.
struct Window {
  instance: ParamVar,
  requiring: ExposedTypes,
  title: ParamVar,
  contents: ShardsVar,
  parents: ParamVar,
}

macro_rules! decl_panel {
  ($name:ident) => {
    struct $name {
      instance: ParamVar,
      requiring: ExposedTypes,
      contents: ShardsVar,
      parents: ParamVar,
      exposing: ExposedTypes,
    }
  };
}

decl_panel!(BottomPanel);
decl_panel!(CentralPanel);
decl_panel!(LeftPanel);
decl_panel!(RightPanel);
decl_panel!(TopPanel);

mod panels;
mod scope;
mod window;

pub fn registerShards() {
  registerShard::<Scope>();
  registerShard::<Window>();
  registerShard::<BottomPanel>();
  registerShard::<CentralPanel>();
  registerShard::<LeftPanel>();
  registerShard::<RightPanel>();
  registerShard::<TopPanel>();
}
