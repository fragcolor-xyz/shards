use shards::shard::Shard;
use shards::shlog_error;
use shards::types::common_type;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::SEQ_OF_INT_OR_FLOAT_TYPES;
use shards::types::SEQ_OF_INT_TYPES;
use shards::types::STRING_TYPES;
use shards::types::{ClonedVar, Context, SeqVar, Type, Types, Var};
use shards::{SHType_Float, SHType_Int};

use candle_core::{Device, Shape, Tensor};

use crate::{TensorType, TensorWrapper, TENSORTYPE_TYPES, TENSOR_TYPE, TENSOR_TYPE_VEC};

#[derive(shards::shard)]
#[shard_info("Tensor.ToString", "Outputs a string representation of a tensor.")]
pub(crate) struct MLTensorToStringShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for MLTensorToStringShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for MLTensorToStringShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
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

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let string = format!("{}", tensor.0);
    let string = Var::ephemeral_string(string.as_str());
    self.output = string.into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info(
  "Tensor",
  "Creates a tensor from a sequence (or nested sequences) of variables."
)]
pub(crate) struct TensorShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param(
    "Shape",
    "The shape of the tensor to create. If not provided, the shape will be inferred from the input.",
    [common_type::ints, common_type::ints_var]
  )]
  shape: ParamVar,

  #[shard_param(
    "Type",
    "The data type of the tensor to create. If not provided, the data type will be inferred from the input.",
    TENSORTYPE_TYPES
  )]
  dtype: ClonedVar,

  // #[shard_param(
  //   "GPU",
  //   "The GPU ID (Supports only CUDA and Metal) to use. If not provided, the CPU will be used.",
  //   [common_type::none, common_type::int]
  // )]
  // device: ClonedVar,
  shape_candle: Option<Shape>,

  ints_cache: Vec<i64>,
  floats_cache: Vec<f64>,
  previous_shape: ClonedVar,

  output: ClonedVar,
}

impl Default for TensorShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      shape: ParamVar::default(),
      dtype: TensorType::F32.into(),
      shape_candle: None,
      output: ClonedVar::default(),
      ints_cache: vec![],
      floats_cache: vec![],
      previous_shape: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_INT_OR_FLOAT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.shape.is_none() {
      return Err("Shape is required");
    }

    if !self.shape.is_variable() {
      // turn shards shape into candle shape
      let shape = self.shape.get_param();
      let shape = shape.as_seq()?;
      let shape = shape
        .iter()
        .map(|v| usize::try_from(&v).unwrap_or_default())
        .collect::<Vec<_>>();
      self.shape_candle = Some(Shape::from(shape));
    } else {
      self.shape_candle = None;
    }

    if unsafe { data.inputType.details.seqTypes.len } != 1 {
      return Err("Invalid input type");
    }

    let tensor_type: TensorType = self.dtype.0.as_ref().try_into().unwrap();

    let input_type = unsafe { data.inputType.details.seqTypes.elements.offset(0).as_ref() };
    if let Some(input_type) = input_type {
      match input_type.basicType {
        SHType_Int => {
          if !matches!(
            tensor_type,
            TensorType::I64 | TensorType::U32 | TensorType::U8
          ) {
            return Err("Invalid input type: expected integer tensor type");
          }
        }
        SHType_Float => {
          if !matches!(
            tensor_type,
            TensorType::F32 | TensorType::F64 | TensorType::F16 | TensorType::BF16
          ) {
            return Err("Invalid input type: expected float tensor type");
          }
        }
        _ => return Err("Invalid input type: expected int or float"),
      }
    } else {
      return Err("Invalid input type: input type is None");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let seq: SeqVar = input.try_into()?;

    let shape = if let Some(shape) = &self.shape_candle {
      shape
    } else {
      let shape = self.shape.get();
      if shape != &self.previous_shape.0 {
        self.previous_shape = shape.into();
        // turn shards shape into candle shape
        let shape = shape.as_seq()?;
        let shape = shape
          .iter()
          .map(|v| usize::try_from(&v).unwrap_or_default())
          .collect::<Vec<_>>();
        self.shape_candle = Some(Shape::from(shape));
      }
      self.shape_candle.as_ref().unwrap()
    };

    let device = Device::Cpu;

    let tensor_type: TensorType = self.dtype.0.as_ref().try_into().unwrap();

    // Directly convert the input sequence into the desired tensor type
    let tensor = match tensor_type {
      TensorType::BF16 | TensorType::F16 | TensorType::F32 | TensorType::F64 => {
        self.floats_cache.clear();
        self
          .floats_cache
          .extend(seq.iter().map(|v| f64::try_from(&v).unwrap()));
        Tensor::from_slice(&self.floats_cache, shape, &device).map_err(|e| {
          shlog_error!("Failed to create tensor: {}", e);
          "Failed to create tensor"
        })?
      }
      TensorType::I64 | TensorType::U32 | TensorType::U8 => {
        self.ints_cache.clear();
        self
          .ints_cache
          .extend(seq.iter().map(|v| i64::try_from(&v).unwrap()));
        Tensor::from_slice(&self.ints_cache, shape, &device).map_err(|e| {
          shlog_error!("Failed to create tensor: {}", e);
          "Failed to create tensor"
        })?
      }
    };

    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();

    Ok(Some(self.output.0))
  }
}
