use crate::shardsc::SHTypeInfo_Details_Object;
use crate::types::{
  ParamVar, Seq, NONE_TYPES, SEQ_OF_FLOAT_TYPES, SEQ_OF_SEQ_OF_FLOAT_TYPES,
  SEQ_OF_SEQ_OF_INT_TYPES, STRINGS_TYPES, STRING_TYPES,
};
use crate::{
  core::registerShard,
  shard::Shard,
  types::{common_type, Context, Parameters, Type, Types, Var, ANY_TYPES, FRAG_CC},
};
use std::alloc::Global;
use std::ffi::CString;
use std::rc::Rc;
use tract_onnx::prelude::*;

pub type OnnxModel =
  SimplePlan<TypedFact, Box<dyn TypedOp, Global>, Graph<TypedFact, Box<dyn TypedOp, Global>>>;

lazy_static! {
  static ref MODEL_TYPE: Type = {
    let mut t = common_type::object;
    t.details.object = SHTypeInfo_Details_Object {
      vendorId: FRAG_CC, // 'frag'
      typeId: 0x6f6e6e78, // 'onnx'
    };
    t
  };
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
      cstr!("Input Shape"),
      shccstr!("The shape of the input tensor."),
      &SEQ_OF_SEQ_OF_INT_TYPES[..]
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

#[derive(Default)]
struct Load {
  model: Option<Rc<OnnxModel>>,
  path: CString,
  shape: Seq,
}

impl Shard for Load {
  fn registerName() -> &'static str {
    cstr!("ONNX.Load")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ONNX.Load-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ONNX.Load"
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
      0 => Ok(self.path = value.try_into()?),
      1 => Ok(self.shape = value.try_into()?),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.path.as_ref().into(),
      1 => self.shape.as_ref().into(),
      _ => unreachable!(),
    }
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let path = self.path.to_str().map_err(|_| "Invalid path utf8 string")?;
    let mut model = tract_onnx::onnx()
      // load the model
      .model_for_path(path)
      .map_err(|e| {
        shlog!("Error: {}", e);
        "Failed to load model from given input path"
      })?;

    for (i, v) in self.shape.iter().enumerate() {
      // specify input type and shape
      let shape: Seq = v.try_into()?;
      let shape: Vec<usize> = shape
        .iter()
        .map(|v| {
          v.as_ref()
            .try_into()
            .expect("Shards validation should prevent this")
        })
        .collect();
      let fact = ShapeFact::from_dims(shape.iter().map(TDim::from));
      let shape = TypedFact::dt_shape(DatumType::F32, fact);

      model.set_input_fact(i, shape.into()).map_err(|e| {
        shlog!("Error: {}", e);
        "Failed to setup model inputs"
      })?;
    }

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

    self.model = Some(Rc::new(model));

    let model_ref = self.model.as_ref().unwrap();
    Ok(Var::new_object(model_ref, &MODEL_VAR_TYPE))
  }
}

#[derive(Default)]
struct Activate {
  model_var: ParamVar,
  previous_model: Option<Var>,
  model: Option<Rc<OnnxModel>>,
}

impl Shard for Activate {
  fn registerName() -> &'static str {
    cstr!("ONNX.Activate")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ONNX.Activate-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ONNX.Activate"
  }

  fn inputTypes(&mut self) -> &Types {
    &SEQ_OF_SEQ_OF_FLOAT_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &SEQ_OF_SEQ_OF_FLOAT_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&ACTIVATE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.model_var.set_param(value)),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.model_var.get_param(),
      _ => unreachable!(),
    }
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.model_var.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.model_var.cleanup();
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let current_model = self.model_var.get();
    let model = match self.previous_model {
      None => {
        self.model = Some(Var::from_object_as_clone(*current_model, &MODEL_VAR_TYPE)?);
        unsafe {
          let model_ptr = Rc::as_ptr(self.model.as_ref().unwrap()) as *mut OnnxModel;
          &*model_ptr
        }
      }
      Some(ref previous_model) => {
        if previous_model != current_model {
          self.model = Some(Var::from_object_as_clone(*current_model, &MODEL_VAR_TYPE)?);
          unsafe {
            let model_ptr = Rc::as_ptr(self.model.as_ref().unwrap()) as *mut OnnxModel;
            &*model_ptr
          }
        } else {
          unsafe {
            let model_ptr = Rc::as_ptr(self.model.as_ref().unwrap()) as *mut OnnxModel;
            &*model_ptr
          }
        }
      }
    };
    Ok(().into())
  }
}

// impl Load {
//   fn test_me() -> TractResult<()> {
//     let model = tract_onnx::onnx()
//       // load the model
//       .model_for_path("mobilenetv2-7.onnx")?
//       // specify input type and shape
//       .with_input_fact(0, f32::fact(&[1, 3, 224, 224]).into())?
//       // optimize the model
//       .into_optimized()?
//       // make the model runnable and fix its inputs and outputs
//       .into_runnable()?;

//     // open image, resize it and make a Tensor out of it
//     let image = image::open("grace_hopper.jpg").unwrap().to_rgb8();
//     let resized =
//       image::imageops::resize(&image, 224, 224, ::image::imageops::FilterType::Triangle);
//     let image: Tensor = tract_ndarray::Array4::from_shape_fn((1, 3, 224, 224), |(_, c, y, x)| {
//       let mean = [0.485, 0.456, 0.406][c];
//       let std = [0.229, 0.224, 0.225][c];
//       (resized[(x as _, y as _)][c] as f32 / 255.0 - mean) / std
//     })
//     .into();

//     // run the model on the input
//     let result = model.run(tvec!(image))?;

//     // find and display the max value with its index
//     let best = result[0]
//       .to_array_view::<f32>()?
//       .iter()
//       .cloned()
//       .zip(2..)
//       .max_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
//     println!("result: {:?}", best);
//     Ok(())
//   }
// }

pub fn registerShards() {
  registerShard::<Load>();
  registerShard::<Activate>();
}
