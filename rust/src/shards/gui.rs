/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shard::Shard;
use crate::types::Context;
use crate::types::Type;
use crate::types::Var;
use crate::types::NONE_TYPES;


#[derive(Default)]
struct Label {

}

impl Shard for Label {
  fn registerName() -> &'static str {
    cstr!("eGUI.Label")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("eGUI.Label-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "eGUI.Label"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let label = egui::Label::new("Hello World!");
    
    // FIXME: context should be shared
    let ctx = egui::Context::default();
    // FIXME: layout should be from parent shards (or use a default)
    egui::CentralPanel::default().show(&ctx, |ui| {
      ui.add(label);
    });

    Ok(*input)
  }
}

pub fn registerShards() {
  registerShard::<Label>();
}
