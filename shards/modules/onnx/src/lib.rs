#![feature(allocator_api)]

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

#[macro_use]
extern crate compile_time_crc32;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::shardsc::SHObjectTypeInfo;
use shards::types::{InstanceData, OptionalString};
use shards::types::{
  ExposedInfo, ExposedTypes, ParamVar, Seq, NONE_TYPES, SEQ_OF_FLOAT_TYPES, SEQ_OF_INT_TYPES,
  STRING_TYPES,
};
use shards::{
  core::register_legacy_shard,
  shard::LegacyShard,
  types::{common_type, ClonedVar, Context, Parameters, Type, Types, Var, FRAG_CC},
};
use std::alloc::Global;
use std::rc::Rc;
use tract_onnx::prelude::*;

pub type OnnxModel =
  SimplePlan<TypedFact, Box<dyn TypedOp, Global>, Graph<TypedFact, Box<dyn TypedOp, Global>>>;

lazy_static! {
  static ref MODEL_TYPE: Type = Type::object(
    FRAG_CC,
    0x6f6e6e78 // 'onnx'
  );
  static ref MODEL_TYPE_VEC: Vec<Type> = vec![*MODEL_TYPE];
  static ref MODEL_VAR_TYPE: Type = Type::context_variable(&MODEL_TYPE_VEC);
  static ref MODEL_TYPE_VEC_VAR: Vec<Type> = vec![*MODEL_VAR_TYPE];

  static ref LOAD_PARAMETERS: Parameters = vec![
    (
      cstr!("Path"),
      shccstr!("The path to the onnx model to load."),
      &STRING_TYPES[..]
    )
      .into(),
    (
      cstr!("InputShape"),
      shccstr!("The shape of the input tensor."),
      &SEQ_OF_INT_TYPES[..]
    )
      .into(),
  ];

  static ref ACTIVATE_PARAMETERS: Parameters = vec![
    (
      cstr!("Model"),
      shccstr!("The ONNX model to use to perform the activation."),
      &MODEL_TYPE_VEC_VAR[..]
    )
      .into(),
  ];
}

struct ModelWrapper {
  model: OnnxModel,
  shape: Vec<usize>,
}

#[derive(Default)]
struct Load {
  model: Option<Rc<ModelWrapper>>,
  path: ClonedVar,
  shape: ClonedVar,
}

impl LegacyShard for Load {
  fn registerName() -> &'static str {
    cstr!("ONNX.Load")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ONNX.Load-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ONNX.Load"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("This shard creates a loaded ONNX model from the file specified in the Path parameter. The model will also expect the input shape specified in the InputShape parameter."))
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The input of this shard is ignored."))
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("Returns the ONNX model object."))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &MODEL_TYPE_VEC
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LOAD_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.path = value.into()),
      1 => Ok(self.shape = value.into()),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.path.0,
      1 => self.shape.0,
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let path: &str = self.path.0.as_ref().try_into()?;
    let mut model = tract_onnx::onnx()
      // load the model
      .model_for_path(path)
      .map_err(|e| {
        shlog!("Error: {}", e);
        "Failed to load model from given input path"
      })?;

    // specify input type and shape
    let shape = self.shape.0.as_seq()?;
    let shape: Vec<usize> = shape
      .iter()
      .map(|v| {
        v.as_ref()
          .try_into()
          .expect("Shards validation should prevent this")
      })
      .collect();
    let fact = ShapeFact::from_dims(shape.iter().map(TDim::from));
    let fact = TypedFact::dt_shape(DatumType::F32, fact);

    model.set_input_fact(0, fact.into()).map_err(|e| {
      shlog!("Error: {}", e);
      "Failed to setup model inputs"
    })?;

    // optimize the model
    let model = model
      .into_optimized()
      .map_err(|e| {
        shlog!("Error: {}", e);
        "Failed to optimize model"
      })?
      // make the model runnable and fix its inputs and outputs
      .into_runnable()
      .map_err(|e| {
        shlog!("Error: {}", e);
        "Failed to make model runnable"
      })?;

    self.model = Some(Rc::new(ModelWrapper { model, shape }));

    let model_ref = self.model.as_ref().unwrap();
    Ok(Some(Var::new_object(model_ref, &MODEL_VAR_TYPE)))
  }
}

#[derive(Default)]
struct Activate {
  model_var: ParamVar,
  previous_model: Option<Var>,
  model: Option<Rc<ModelWrapper>>,
  output: Seq,
  reqs: ExposedTypes,
}

impl LegacyShard for Activate {
  fn registerName() -> &'static str {
    cstr!("ONNX.Activate")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ONNX.Activate-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ONNX.Activate"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("This shard runs the loaded ONNX model (created using the ONNX.Load shard) specified in the Model parameter. It takes the float sequence input, creates a tensor with a matching shape expected by the model, runs the model on the tensor and returns the output tensor as a sequence of floats."))
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The float sequence to run inference on."))
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output tensor as a sequence of floats."))
  }

  fn inputTypes(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &SEQ_OF_FLOAT_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&ACTIVATE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.model_var.set_param(value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.model_var.get_param(),
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.reqs.clear();
    self.reqs.push(ExposedInfo::new_with_help_from_ptr(
      self.model_var.get_name(),
      shccstr!("The required ONNX model."),
      *MODEL_TYPE,
    ));
    Some(&self.reqs)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.model_var.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.model_var.cleanup(ctx);
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let current_model = self.model_var.get();
    let model = match self.previous_model {
      None => {
        self.model = Some(Var::from_object_as_clone(current_model, &MODEL_VAR_TYPE)?);
        unsafe {
          let model_ptr = Rc::as_ptr(self.model.as_ref().unwrap()) as *mut ModelWrapper;
          &*model_ptr
        }
      }
      Some(ref previous_model) => {
        if previous_model != current_model {
          self.model = Some(Var::from_object_as_clone(current_model, &MODEL_VAR_TYPE)?);
          unsafe {
            let model_ptr = Rc::as_ptr(self.model.as_ref().unwrap()) as *mut ModelWrapper;
            &*model_ptr
          }
        } else {
          unsafe {
            let model_ptr = Rc::as_ptr(self.model.as_ref().unwrap()) as *mut ModelWrapper;
            &*model_ptr
          }
        }
      }
    };

    let mut tensor = unsafe {
      Tensor::uninitialized_dt(DatumType::F32, &model.shape)
        .map_err(|_| "Failed to allocate tensor")?
    };
    let tensor_slice = unsafe { tensor.as_slice_mut_unchecked::<f32>() };

    // TODO add more input types support

    let seq: Seq = input.try_into()?;
    if seq.len() != tensor_slice.len() {
      return Err("Input sequence length does not match model input shape");
    }
    for (i, v) in seq.iter().enumerate() {
      let value: f32 = v.as_ref().try_into()?;
      tensor_slice[i] = value;
    }

    let result = model.model.run(tvec!(tensor.into())).map_err(|e| {
      shlog!("Error: {}", e);
      "Failed to activate model"
    })?;

    let result: Vec<f32> = result[0]
      .to_array_view::<f32>()
      .map_err(|_| "Failed to map result tensor")?
      .iter()
      .cloned()
      .collect();

    self.output.set_len(result.len());
    for (i, v) in result.iter().enumerate() {
      self.output[i] = Var::from(*v);
    }

    Ok(Some(self.output.as_ref().into()))
  }
}

#[derive(shards::shard)]
#[shard_info("CandleTest", "AddDescriptionHere")]
struct CandleTestShard {
  #[shard_required]
  required: ExposedTypes,
}

impl Default for CandleTestShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CandleTestShard {
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
    use candle_core::{Device, Tensor};

    let device = Device::Cpu;

    let a = Tensor::randn(0f32, 1., (2, 3), &device).unwrap();
    let b = Tensor::randn(0f32, 1., (3, 4), &device).unwrap();

    let c = a.matmul(&b).unwrap();
    println!("{c}");

    Ok(None)
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_onnx_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_legacy_shard::<Load>();
  register_legacy_shard::<Activate>();
  register_shard::<CandleTestShard>();
}
