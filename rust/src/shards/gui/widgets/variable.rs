use super::Variable;
use super::WireVariable;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::UIRenderer;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::ClonedVar;
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
use crate::types::BOOL_TYPES;
use crate::SHType_Seq;
use std::cmp::Ordering;
use std::ffi::CStr;
use std::os::raw::c_char;

static ANY_VAR_ONLY_SLICE: &[Type] = &[common_type::any_var];

const WIRE_VAR_NAMES: &[RawString] = &[shstr!("Wire"), shstr!("Name")];
static WIRE_VAR_TYPES: &[Type] = &[common_type::wire, common_type::string];

lazy_static! {
  static ref VARIABLE_PARAMETERS: Parameters = vec![
    (
      cstr!("Variable"),
      shccstr!("The variable that holds the value."),
      ANY_VAR_ONLY_SLICE,
    )
      .into(),
    (
      cstr!("Labeled"),
      shccstr!("If the name of the variable should be visible as a label."),
      &BOOL_TYPES[..],
    )
      .into()
  ];
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
  fn triggerVarValueChange(context: *mut Context, name: *const Var, var: *const Var);
  fn getWireContext(wire: WireRef) -> *mut Context;
}

impl Default for Variable {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      variable: ParamVar::default(),
      labeled: false,
      name: ClonedVar::default(),
      mutable: true,
      inner_type: None,
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
      1 => Ok(self.labeled = value.try_into()?),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.variable.get_param(),
      1 => self.labeled.into(),
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
          if var.exposedType.basicType == SHType_Seq {
            let slice = unsafe {
              let ptr = var.exposedType.details.seqTypes.elements;
              std::slice::from_raw_parts(ptr, var.exposedType.details.seqTypes.len as usize)
            };
            if slice.len() != 1 {
              return Err("UI.Variable: Sequence must have only one type");
            }
            self.inner_type = Some(slice[0]);
          }
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

    self.name = if self.variable.is_variable() {
      let name = unsafe { CStr::from_ptr(self.variable.get_name()) };
      name.into()
    } else {
      ClonedVar::default()
    };

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.variable.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let label: &str = self.name.as_ref().try_into()?;
      ui.horizontal(|ui| {
        if self.labeled {
          ui.label(label);
        }
        let varRef = self.variable.get_mut();
        if varRef
          .render(!self.mutable, self.inner_type.as_ref(), ui)
          .changed()
        {
          unsafe {
            triggerVarValueChange(
              context as *const Context as *mut Context,
              self.name.as_ref() as *const Var,
              varRef as *const Var,
            );
            varRef.__bindgen_anon_1.version += 1
          };
        }
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

    let name_var = input.get_fast_static(cstr!("Name"));
    let name: &str = name_var.try_into()?;
    let wire_var = input.get_fast_static(cstr!("Wire"));
    let wire: WireRef = wire_var.try_into()?;

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
        if varRef.render(false, None, ui).changed() {
          unsafe {
            triggerVarValueChange(
              getWireContext(wire),
              name_var as *const Var,
              varRef as *const Var,
            );
            varRef.__bindgen_anon_1.version += 1
          };
        }
      });

      Ok(*varRef)
    } else {
      Err("No UI parent")
    }
  }
}
