use once_cell::sync::OnceCell;
use pest::Parser;
use shards::{
  core::cloneVar,
  types::{AutoTableVar, Mesh, Var},
  util::from_raw_parts_allow_null,
};
use shards_lang::ast::{LineInfo, Rule, ShardsParser};
use std::{
  collections::HashMap,
  sync::{atomic::AtomicBool, Arc},
};

static GLOBAL_MAP: OnceCell<AutoTableVar> = OnceCell::new();

pub fn new_cancellation_token() -> Arc<AtomicBool> {
  Arc::new(AtomicBool::new(false))
}

pub(crate) fn get_global_map() -> &'static AutoTableVar {
  GLOBAL_MAP.get_or_init(setup_global_map)
}

fn setup_global_map() -> AutoTableVar {
  let mut directory = AutoTableVar::new();
  let mut result = include_shards!("directory.shs");
  assert!(result.0.is_table());
  // swap the table into the directory
  std::mem::swap(&mut directory.0 .0, &mut result.0);
  directory
}
