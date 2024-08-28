use shards::shard::Shard;
use shards::types::InstanceData;
use shards::types::{common_type, ClonedVar, Context, Type, Types, Var, FRAG_CC};
use shards::types::{
  AutoSeqVar, ExposedTypes, ParamVar, NONE_TYPES, SEQ_OF_INT_TYPES, STRING_TYPES,
};
use shards::{fourCharacterCode, ref_counted_object_type_impl, shlog_error};

use candle_core::Tensor;

struct TensorWrapper(Tensor);
ref_counted_object_type_impl!(TensorWrapper);

lazy_static! {
  pub static ref TENSOR_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"cTEN")); // last letter used as version
  pub static ref TENSOR_TYPE_VEC: Vec<Type> = vec![*TENSOR_TYPE];
  pub static ref TENSOR_VAR_TYPE: Type = Type::context_variable(&TENSOR_TYPE_VEC);
}

#[derive(shards::shard)]
#[shard_info(
  "CandleTensor",
  "Creates a Candle tensor object from a regular sequence input (not all types supported)."
)]
struct CandleTensorShard {
  #[shard_required]
  required: ExposedTypes,
}

impl Default for CandleTensorShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CandleTensorShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    Ok(None)
  }
}
