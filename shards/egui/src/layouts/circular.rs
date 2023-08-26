use crate::{util, CONTEXTS_NAME, EGUI_CTX_TYPE, HELP_VALUE_IGNORED};
use egui::Vec2;
use shards::{
  core::registerShard,
  shard::Shard,
  types::{
    common_type, ClonedVar, Context, ExposedInfo, ExposedTypes, InstanceData, OptionalString,
    ParamVar, Parameters, ShardsVar, Type, Types, Var, NONE_TYPES, SHARDS_OR_NONE_TYPES,
  },
};

lazy_static! {
//   static ref VEC2_TYPES: Types = vec![common_type::float2, common_type::float2_var];
  static ref OUTPUT_TYPES: Types = vec![];
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("Center"),
      shccstr!("Which property to read from the UI."),
      &VEC2_TYPES[..],
    )
      .into(),
    (
      cstr!("Radius"),
      shccstr!("Which property to read from the UI."),
      &VEC2_TYPES[..],
    )
      .into()
  ];
}

pub struct Circular {
  instance: ParamVar,
  requiring: ExposedTypes,
  //   property: Var,
}

// impl Default for Circular {
//   fn default() -> Self {
//     let mut instance = ParamVar::default();
//     instance.set_name(CONTEXTS_NAME);
//     Self {
//       instance,
//       requiring: Vec::new(),
//       //   property: Var::default(),
//     }
//   }
// }

// impl Circular {}

// impl Shard for Circular {
//   fn registerName() -> &'static str
//   where
//     Self: Sized,
//   {
//     cstr!("UI.CircularLayout")
//   }

//   fn hash() -> u32
//   where
//     Self: Sized,
//   {
//     compile_time_crc32::crc32!("UI.Circular-rust-0x20200101")
//   }

//   fn name(&mut self) -> &str {
//     "UI.Circular"
//   }

//   fn help(&mut self) -> OptionalString {
//     OptionalString(shccstr!("Creates UI layouts around a central point."))
//   }

//   fn inputTypes(&mut self) -> &Types {
//     &NONE_TYPES
//   }

//   fn inputHelp(&mut self) -> OptionalString {
//     *HELP_VALUE_IGNORED
//   }

//   fn outputTypes(&mut self) -> &Types {
//     &OUTPUT_TYPES
//   }

//   fn outputHelp(&mut self) -> OptionalString {
//     OptionalString(shccstr!("The value produced."))
//   }

//   fn parameters(&mut self) -> Option<&Parameters> {
//     Some(&PARAMETERS)
//   }

//   fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
//     match index {
//       //   0 => Ok(self.property = *value),
//       _ => Err("Invalid parameter index"),
//     }
//   }

//   fn getParam(&mut self, index: i32) -> Var {
//     match index {
//       //   0 => self.property,
//       _ => Var::default(),
//     }
//   }

//   fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
//     self.requiring.clear();

//     let exp_info = ExposedInfo {
//       exposedType: EGUI_CTX_TYPE,
//       name: self.instance.get_name(),
//       help: cstr!("The exposed UI context.").into(),
//       ..ExposedInfo::default()
//     };
//     self.requiring.push(exp_info);

//     Some(&self.requiring)
//   }

//   fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
//     self.instance.warmup(ctx);
//     self.parents.warmup(ctx);

//     Ok(())
//   }

//   fn cleanup(&mut self) -> Result<(), &str> {
//     self.parents.cleanup();
//     self.instance.cleanup();

//     Ok(())
//   }

//   fn hasCompose() -> bool {
//     true
//   }

//   fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
//     Ok(_data.inputType)
//   }

//   fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
//     let ctx = util::get_current_context(&self.instance)?;
//   }
// }

lazy_static! {
  static ref ABS_PARAMETERS: Parameters = vec![
    (
      cstr!("Center"),
      shccstr!("Which property to read from the UI."),
      &VEC2_TYPES[..],
    )
      .into(),
    (
      cstr!("Size"),
      shccstr!("Which property to read from the UI."),
      &VEC2_TYPES[..],
    )
      .into()
  ];
}
pub struct AbsoluteUI {
  instance: ParamVar,
  requiring: ExposedTypes,
  center: ParamVar,
  size: ParamVar,
}

impl Default for AbsoluteUI {
  fn default() -> Self {
    let mut instance = ParamVar::default();
    instance.set_name(CONTEXTS_NAME);
    Self {
      instance,
      requiring: Vec::new(),
      center: ParamVar::default(),
      size: ParamVar::default(),
    }
  }
}

impl AbsoluteUI {}

impl Shard for AbsoluteUI {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Absolute")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Circular-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Circular"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Creates UI layouts around a central point."))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value produced."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&ABS_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.center.set_cloning(value)),
      1 => Ok(self.size.set_cloning(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.center.parameter.0,
      1 => self.size.parameter.0,
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    if self.size.is_variable() {
      let exp_info = ExposedInfo {
        exposedType: EGUI_CTX_TYPE,
        name: self.size.get_name(),
        ..ExposedInfo::default()
      };
      self.requiring.push(exp_info);
    }

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.center.warmup(ctx);
    self.size.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.instance.cleanup();
    self.center.cleanup();
    self.size.cleanup();

    Ok(())
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    Ok(_data.inputType)
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let center: Vec2 = {
      let v = self.center.get();
      let f2 = unsafe { &v.payload.__bindgen_anon_1.float2Value };
      Vec2::new(f2[0] as f32, f2[1] as f32)
    };
    let ctx = util::get_current_context(&self.instance)?;
    Ok(Var::default())
  }
}

lazy_static! {
  static ref VEC2_TYPES: Types = vec![common_type::float2, common_type::float2_var];
  static ref NAME_TYPES: Types = vec![common_type::string];
}

shard! {
  struct TestMacroShard2("TestMacroShard", "desc") {
    #[Param("Name", "The name of the test shard", NAME_TYPES)]
    pub name: ClonedVar,
    #[Param("Size", "The size", VEC2_TYPES)]
    pub size: ParamVar,
    #[Param("Position", "The position", VEC2_TYPES)]
    pub position: ParamVar,
    #[Param("Test", "", [common_type::float4])]
    pub test: ClonedVar,
    #[Param("Action", "", SHARDS_OR_NONE_TYPES)]
    pub action: ShardsVar,
  }

  impl Shard for TestMacroShard2 {
    fn inputTypes(&mut self) -> &Types {
      &NONE_TYPES
    }
    fn outputTypes(&mut self) -> &Types {
      &NONE_TYPES
    }
    fn inputHelp(&mut self) -> OptionalString {
      *HELP_VALUE_IGNORED
    }
    fn warmup(&mut self, context: &Context) -> Result<(), &str> {
      self.warmup_helper(context)?;
      Ok(())
    }
    fn cleanup(&mut self) -> Result<(), &str> {
      self.cleanup_helper()?;
      Ok(())
    }
    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
      self.compose_helper(data)?;
      let cr = self.action.compose(data)?;
      for req in &cr.requiredInfo {
          self.required.push(req);
        }
      Ok(NONE_TYPES[0])
    }
    fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
      unsafe {
        println!("pos: {}", self.position.get().payload.__bindgen_anon_1.float2Value[0]);
        println!("size: {}", self.size.get().payload.__bindgen_anon_1.float2Value[0]);
        println!("test: {}", self.test.0.payload.__bindgen_anon_1.float4Value[0]);
      }
      Ok(Var::default())
    }
  }
}

pub fn registerShards() {
  registerShard::<TestMacroShard2>();
}
