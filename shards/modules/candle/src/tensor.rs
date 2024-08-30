use shards::shard::Shard;
use shards::shlog_error;
use shards::types::common_type;
use shards::types::AutoSeqVar;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::FLOAT_TYPES;
use shards::types::SEQ_OF_INT_OR_FLOAT_TYPES;
use shards::types::SEQ_OF_INT_TYPES;
use shards::types::STRING_TYPES;
use shards::types::{ClonedVar, Context, SeqVar, Type, Types, Var};
use shards::{SHType_Float, SHType_Int};

use candle_core::{Device, Shape, Tensor};

use crate::TENSOR_VAR_TYPE;
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

#[derive(shards::shard)]
#[shard_info(
  "Tensor.ZerosLike",
  "Creates a tensor of zeros with the same shape as the input tensor."
)]
pub(crate) struct TensorZerosLikeShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for TensorZerosLikeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorZerosLikeShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let tensor = tensor.0.zeros_like().map_err(|e| {
      shlog_error!("Failed to create tensor: {}", e);
      "Failed to create tensor"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Mul", "Multiplies two tensors element-wise.")]
pub(crate) struct TensorMulShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Other", "The tensor to multiply with.", [*TENSOR_VAR_TYPE])]
  other: ParamVar,

  output: ClonedVar,
}

impl Default for TensorMulShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      other: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorMulShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.other.is_none() {
      return Err("Other is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let other = unsafe {
      &mut *Var::from_ref_counted_object::<TensorWrapper>(&self.other.get(), &*TENSOR_TYPE)?
    };
    let tensor = (&tensor.0 * &other.0).map_err(|e| {
      shlog_error!("Failed to multiply tensors: {}", e);
      "Failed to multiply tensors"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Add", "Adds two tensors element-wise.")]
pub(crate) struct TensorAddShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Other", "The tensor to add.", [*TENSOR_VAR_TYPE])]
  other: ParamVar,

  output: ClonedVar,
}

impl Default for TensorAddShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      other: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorAddShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.other.is_none() {
      return Err("Other is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let other = unsafe {
      &mut *Var::from_ref_counted_object::<TensorWrapper>(&self.other.get(), &*TENSOR_TYPE)?
    };
    let tensor = (&tensor.0 + &other.0).map_err(|e| {
      shlog_error!("Failed to add tensors: {}", e);
      "Failed to add tensors"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Sub", "Subtracts two tensors element-wise.")]
pub(crate) struct TensorSubShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Other", "The tensor to subtract.", [*TENSOR_VAR_TYPE])]
  other: ParamVar,

  output: ClonedVar,
}

impl Default for TensorSubShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      other: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorSubShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.other.is_none() {
      return Err("Other is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let other = unsafe {
      &mut *Var::from_ref_counted_object::<TensorWrapper>(&self.other.get(), &*TENSOR_TYPE)?
    };
    let tensor = (&tensor.0 - &other.0).map_err(|e| {
      shlog_error!("Failed to subtract tensors: {}", e);
      "Failed to subtract tensors"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Div", "Divides two tensors element-wise.")]
pub(crate) struct TensorDivShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Other", "The tensor to divide by.", [*TENSOR_VAR_TYPE, common_type::float])]
  other: ParamVar,

  output: ClonedVar,
}

impl Default for TensorDivShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      other: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorDivShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.other.is_none() {
      return Err("Other is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let other = self.other.get();
    if other.is_float() {
      let other: f64 = other.try_into()?;
      let tensor = (&tensor.0 / other).map_err(|e| {
        shlog_error!("Failed to divide tensors: {}", e);
        "Failed to divide tensors"
      })?;
      self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    } else {
      let other = unsafe {
        &mut *Var::from_ref_counted_object::<TensorWrapper>(&self.other.get(), &*TENSOR_TYPE)?
      };
      let tensor = (&tensor.0 / &other.0).map_err(|e| {
        shlog_error!("Failed to divide tensors: {}", e);
        "Failed to divide tensors"
      })?;
      self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    }
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info(
  "Tensor.Pow",
  "Raises each element of the tensor to the power of the corresponding element in another tensor."
)]
pub(crate) struct TensorPowShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Other", "The tensor of exponents.", [*TENSOR_VAR_TYPE])]
  other: ParamVar,

  output: ClonedVar,
}

impl Default for TensorPowShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      other: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorPowShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.other.is_none() {
      return Err("Other is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let other = unsafe {
      &mut *Var::from_ref_counted_object::<TensorWrapper>(&self.other.get(), &*TENSOR_TYPE)?
    };
    let tensor = tensor.0.pow(&other.0).map_err(|e| {
      shlog_error!("Failed to raise tensor to power: {}", e);
      "Failed to raise tensor to power"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.MatMul", "Performs matrix multiplication of two tensors.")]
pub(crate) struct TensorMatMulShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Other", "The tensor to multiply with.", [*TENSOR_VAR_TYPE])]
  other: ParamVar,

  output: ClonedVar,
}

impl Default for TensorMatMulShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      other: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorMatMulShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    if self.other.is_none() {
      return Err("Other is required");
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let other = unsafe {
      &mut *Var::from_ref_counted_object::<TensorWrapper>(&self.other.get(), &*TENSOR_TYPE)?
    };
    let tensor = tensor.0.matmul(&other.0).map_err(|e| {
      shlog_error!("Failed to perform matrix multiplication: {}", e);
      "Failed to perform matrix multiplication"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Transpose", "Transposes the dimensions of the tensor.")]
pub(crate) struct TensorTransposeShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for TensorTransposeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorTransposeShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let tensor = tensor.0.transpose(0, 1).map_err(|e| {
      shlog_error!("Failed to transpose tensor: {}", e);
      "Failed to transpose tensor"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Reshape", "Reshapes the tensor to the specified shape.")]
pub(crate) struct TensorReshapeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param(
        "Shape",
        "The new shape for the tensor.",
        [common_type::ints, common_type::ints_var]
    )]
  shape: ParamVar,

  output: ClonedVar,
}

impl Default for TensorReshapeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      shape: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorReshapeShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };
    let shape = self.shape.get();
    let shape = shape.as_seq()?;
    let shape: Vec<usize> = shape
      .iter()
      .map(|v| usize::try_from(&v).unwrap_or_default())
      .collect();
    let tensor = tensor.0.reshape(shape).map_err(|e| {
      shlog_error!("Failed to reshape tensor: {}", e);
      "Failed to reshape tensor"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info(
  "Tensor.Sum",
  "Computes the sum of tensor elements along the specified dimensions."
)]
pub(crate) struct TensorSumShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param(
        "Dims",
        "The dimensions to sum over. If not provided, sum over all dimensions.",
        [common_type::ints, common_type::ints_var, common_type::none]
    )]
  dims: ParamVar,

  output: ClonedVar,
}

impl Default for TensorSumShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      dims: ParamVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorSumShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let tensor =
      unsafe { &mut *Var::from_ref_counted_object::<TensorWrapper>(&input, &*TENSOR_TYPE)? };

    let dims = self.dims.get();

    let tensor = if !dims.is_none() {
      let dims = dims.as_seq()?;
      let dims: Vec<usize> = dims
        .iter()
        .map(|v| usize::try_from(&v).unwrap_or_default())
        .collect();
      tensor.0.sum(dims.as_slice())
    } else {
      tensor.0.sum_all()
    }
    .map_err(|e| {
      shlog_error!("Failed to sum tensor: {}", e);
      "Failed to sum tensor"
    })?;
    self.output = Var::new_ref_counted(TensorWrapper(tensor), &*TENSOR_TYPE).into();
    Ok(Some(self.output.0))
  }
}

#[derive(shards::shard)]
#[shard_info(
  "Tensor.ToFloat",
  "Converts a single-element tensor to a float (f64) scalar."
)]
pub(crate) struct TensorToFloatShard {
  #[shard_required]
  required: ExposedTypes,
}

impl Default for TensorToFloatShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for TensorToFloatShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &FLOAT_TYPES
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

    let scalar = match tensor.0.dtype() {
      candle_core::DType::F32 => {
        let value = tensor.0.to_scalar::<f32>().map_err(|e| {
          shlog_error!("Failed to convert F32 tensor to float: {}", e);
          "Failed to convert F32 tensor to float"
        })?;
        value as f64
      }
      candle_core::DType::F64 => tensor.0.to_scalar::<f64>().map_err(|e| {
        shlog_error!("Failed to convert F64 tensor to float: {}", e);
        "Failed to convert F64 tensor to float"
      })?,
      _ => {
        shlog_error!("Unsupported tensor dtype for conversion to float");
        return Err("Unsupported tensor dtype for conversion to float");
      }
    };

    Ok(Some(scalar.into()))
  }
}

#[derive(shards::shard)]
#[shard_info("Tensor.Shape", "Outputs the shape of the tensor.")]
pub(crate) struct ShapeShard {
  #[shard_required]
  required: ExposedTypes,

  output: AutoSeqVar,
}

impl Default for ShapeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ShapeShard {
  fn input_types(&mut self) -> &Types {
    &TENSOR_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_INT_TYPES
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
    let shape = tensor.0.shape();
    self.output.0.clear();
    for dim in shape.dims().iter() {
      let dim: Var = (*dim as i64).into();
      self.output.0.push(&dim);
    }
    Ok(Some(self.output.0 .0))
  }
}
