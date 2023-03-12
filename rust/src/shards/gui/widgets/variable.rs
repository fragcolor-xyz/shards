use super::Variable;
use super::WireVariable;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::UIRenderer;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::Table;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::WireRef;
use crate::types::ANY_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;
use std::os::raw::c_char;

static ANY_VAR_ONLY_SLICE: &[Type] = &[common_type::any_var];

const WIRE_VAR_NAMES: &[RawString] = &[shstr!("Wire"), shstr!("Name")];
static WIRE_VAR_TYPES: &[Type] = &[common_type::wire, common_type::string];

lazy_static! {
  static ref VARIABLE_PARAMETERS: Parameters = vec![(
    cstr!("Variable"),
    shccstr!("The variable that holds the value."),
    ANY_VAR_ONLY_SLICE,
  )
    .into(),];
  static ref WIRE_VAR_TTYPE: Type = Type::table(WIRE_VAR_NAMES, &WIRE_VAR_TYPES);
  static ref WIRE_VAR_INPUT: Vec<Type> = vec![*WIRE_VAR_TTYPE];
}

static HEADERS_TYPES: &[Type] = &[
  common_type::none,
  common_type::string_table,
  common_type::string_table_var,
];

extern "C" {
  fn getWireVariable(wire: WireRef, name: *const c_char, nameLen: u32) -> *mut Var;
}

impl Default for Variable {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      variable: ParamVar::default(),
      mutable: true,
    }
  }
}

impl Shard for Variable {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Variable")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Variable-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Variable"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&VARIABLE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.variable.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.variable.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if self.variable.is_variable() {
      let shared: ExposedTypes = data.shared.into();
      for var in shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.variable.get_name()),
          )
        };
        if CStr::cmp(a, b) == Ordering::Equal {
          self.mutable = var.isMutable;
          break;
        }
      }
      Ok(data.inputType)
    } else {
      Err("Variable is not a variable")
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.variable.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.variable.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      ui.horizontal(|ui| {
        let label = unsafe { CStr::from_ptr(self.variable.get_name()) }
          .to_str()
          .unwrap_or_default();
        ui.label(label);
        self.variable.get_mut().render(!self.mutable, ui);
      });

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

impl Default for WireVariable {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for WireVariable {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.WireVariable")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.WireVariable-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.WireVariable"
  }

  fn inputTypes(&mut self) -> &Types {
    &WIRE_VAR_INPUT
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let input: Table = input.try_into()?;
    let name: &str = input.get_fast_static(cstr!("Name")).try_into()?;
    let wire: WireRef = input.get_fast_static(cstr!("Wire")).try_into()?;
    let varPtr = unsafe {
      getWireVariable(wire, name.as_ptr() as *const c_char, name.len() as u32) as *mut Var
    };
    if varPtr == std::ptr::null_mut() {
      return Err("Variable not found");
    }
    let varRef = unsafe { &mut *varPtr };

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      ui.horizontal(|ui| {
        ui.label(name);
        varRef.render(false, ui);
      });

      Ok(*varRef)
    } else {
      Err("No UI parent")
    }
  }
}
