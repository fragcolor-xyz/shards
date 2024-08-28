#[macro_use]
extern crate lazy_static;

use std::str::FromStr;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::InstanceData;
use shards::types::{common_type, ClonedVar, Context, Type, Types, Var, FRAG_CC};
use shards::types::{
  AutoSeqVar, ExposedTypes, ParamVar, NONE_TYPES, SEQ_OF_FLOAT_TYPES, SEQ_OF_INT_TYPES,
  STRING_TYPES,
};
use shards::{fourCharacterCode, ref_counted_object_type_impl, shlog_error};

mod tokenizer;

#[no_mangle]
pub extern "C" fn shardsRegister_candle_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<tokenizer::CandleTokenizer>();
  register_shard::<tokenizer::TokensShard>();
}
