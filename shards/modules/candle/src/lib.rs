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

use candle_core::Tensor;

mod tokenizer;
// mod tensor;

struct TensorWrapper(Tensor);
ref_counted_object_type_impl!(TensorWrapper);

lazy_static! {
  pub static ref TENSOR_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"cTEN")); // last letter used as version
  pub static ref TENSOR_TYPE_VEC: Vec<Type> = vec![*TENSOR_TYPE];
  pub static ref TENSOR_VAR_TYPE: Type = Type::context_variable(&TENSOR_TYPE_VEC);
}

#[no_mangle]
pub extern "C" fn shardsRegister_candle_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<tokenizer::CandleTokenizer>();
  register_shard::<tokenizer::TokensShard>();
}
