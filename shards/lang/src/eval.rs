use crate::ast::*;
use crate::ParamHelper;
use crate::RcStrWrapper;
use crate::ShardsExtension;

use core::convert::TryInto;

use core::slice;
use nanoid::nanoid;
use shards::fourCharacterCode;
use shards::shlog_trace;
use shards::types::common_type;
use shards::types::AutoShardRef;
use shards::types::Type;
use shards::types::FRAG_CC;
use shards::SHType_Object;
use shards::SHType_Type;
use std::cell::RefCell;

use shards::core::destroyVar;
use shards::core::sleep;
use std::collections::HashMap;
use std::collections::HashSet;

use shards::core::findEnumId;
use shards::core::findEnumInfo;
use shards::types::ClonedVar;
use shards::types::Mesh;
use shards::SHType_Enum;
use shards::SHType_String;

use shards::types::SeqVar;
use shards::types::TableVar;
use shards::types::{ShardRef, Var, Wire};

use shards::{SHType_ContextVar, SHType_ShardRef};
use std::ffi::CStr;

struct ShardsGroup {
  args: *const Vec<Value>,
  shards: *const Sequence,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Setting {
  disallow_unsafe: bool,
  disallow_custom_stack_sizes: bool,
  disallow_impure_wires: bool,
}

pub struct EvalEnv {
  pub(crate) parent: Option<*const EvalEnv>,

  shards: Vec<AutoShardRef>,

  deferred_wires: HashMap<RcStrWrapper, (Wire, *const Vec<Param>, LineInfo)>,
  finalized_wires: HashMap<RcStrWrapper, ClonedVar>,

  shards_groups: HashMap<RcStrWrapper, ShardsGroup>,
  macro_groups: HashMap<RcStrWrapper, ShardsGroup>,
  definitions: HashMap<RcStrWrapper, *const Value>,

  // used during @template evaluations, to replace [x y z] arguments
  replacements: HashMap<RcStrWrapper, *const Value>,

  // used during @template evaluation
  suffix: Option<RcStrWrapper>,
  suffix_assigned: HashMap<RcStrWrapper, RcStrWrapper>, // maps var names to their suffix

  // Shards and functions that are forbidden to be used
  pub forbidden_funcs: HashSet<RcStrWrapper>,
  pub settings: Vec<Setting>,

  meshes: HashMap<RcStrWrapper, Mesh>,

  extensions: HashMap<RcStrWrapper, Box<dyn ShardsExtension>>,
}

impl Drop for EvalEnv {
  fn drop(&mut self) {
    // keep this because we want borrow checker warnings
  }
}

impl Default for EvalEnv {
  fn default() -> Self {
    EvalEnv {
      parent: None,
      shards: Vec::new(),
      deferred_wires: HashMap::new(),
      finalized_wires: HashMap::new(),
      shards_groups: HashMap::new(),
      macro_groups: HashMap::new(),
      definitions: HashMap::new(),
      replacements: HashMap::new(),
      suffix: None,
      suffix_assigned: HashMap::new(),
      forbidden_funcs: HashSet::new(),
      settings: Vec::new(),
      meshes: HashMap::new(),
      extensions: HashMap::new(),
    }
  }
}

enum SVar {
  Cloned(ClonedVar),
  NotCloned(Var),
}

impl AsRef<Var> for SVar {
  fn as_ref(&self) -> &Var {
    match self {
      SVar::Cloned(v) => &v.0,
      SVar::NotCloned(v) => v,
    }
  }
}

impl AsMut<Var> for SVar {
  fn as_mut(&mut self) -> &mut Var {
    match self {
      SVar::Cloned(v) => &mut v.0,
      SVar::NotCloned(v) => v,
    }
  }
}

fn process_vector_built_in_ints_block<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  // it's either a Const or a MakeVector in this case

  let (params, len) = get_vec_params::<WIDTH, 16>(func, line_info)?;

  let has_variables = params.iter().any(|x| {
    if let Value::Identifier(_) = &x.value {
      true
    } else {
      false
    }
  });

  if !has_variables {
    let value = extract_ints_vector_var::<WIDTH>(len, params, line_info)?;
    add_const_shard2(value, line_info, e)
  } else {
    let shard = extract_make_ints_shard::<WIDTH>(len, params, line_info, e)?;
    e.shards.push(shard);
    Ok(())
  }
}

fn handle_vector_built_in_ints<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
) -> Result<Var, ShardsError> {
  let (params, len) = get_vec_params::<WIDTH, 16>(func, line_info)?;
  extract_ints_vector_var::<WIDTH>(len, params, line_info)
}

fn extract_make_ints_shard<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  fn error_requires_number(line_info: LineInfo) -> Result<AutoShardRef, ShardsError> {
    Err(
      (
        "vector built-in function requires a floating point number or identifier parameter",
        line_info,
      )
        .into(),
    )
  }

  let shard = match WIDTH {
    2 => ShardRef::create("MakeInt2"),
    3 => ShardRef::create("MakeInt3"),
    4 => ShardRef::create("MakeInt4"),
    8 => ShardRef::create("MakeInt8"),
    16 => ShardRef::create("MakeInt16"),
    _ => {
      return Err(
        (
          "float vector built-in function requires 2, 3, 4, 8 or 16 parameters",
          line_info,
        )
          .into(),
      )
    }
  }
  .unwrap();
  let shard = AutoShardRef(shard);

  for i in 0..len {
    let var = match &params[i].value {
      Value::Identifier(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      Value::Number(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      _ => return error_requires_number(line_info),
    }?;
    shard
      .0
      .set_parameter(i as i32, *var.as_ref()) // Type conversion should be handled by the shard!
      .map_err(|err| {
        (
          format!(
            "Error setting parameter for MakeInt{}, error: {}",
            WIDTH, err
          ),
          line_info,
        )
          .into()
      })?;
  }
  Ok(shard)
}

fn extract_ints_vector_var<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
) -> Result<shards::SHVar, ShardsError> {
  let mut vector_values: [i64; WIDTH] = [0; WIDTH];

  fn error_requires_number(line_info: LineInfo) -> Result<Var, ShardsError> {
    Err(
      (
        "vector built-in function requires an integer number parameter",
        line_info,
      )
        .into(),
    )
  }

  for i in 0..len {
    vector_values[i] = match &params[i].value {
      Value::Number(n) => match n {
        Number::Integer(n) => *n,
        _ => return error_requires_number(line_info),
      },
      _ => return error_requires_number(line_info),
    };
  }

  if len == 1 {
    // fill with first value
    for i in 1..WIDTH {
      vector_values[i] = vector_values[0];
    }
  }

  match WIDTH {
    2 => Ok((vector_values[0], vector_values[1]).into()),
    3 => {
      let (x_result, y_result, z_result) = (
        i32::try_from(vector_values[0]),
        i32::try_from(vector_values[1]),
        i32::try_from(vector_values[2]),
      );

      match (x_result, y_result, z_result) {
        (Ok(x), Ok(y), Ok(z)) => Ok((x, y, z).into()),
        _ => Err(
          (
            "vector built-in function requires 3 integer parameters",
            line_info,
          )
            .into(),
        ),
      }
    }
    4 => {
      let (x_result, y_result, z_result, w_result) = (
        i32::try_from(vector_values[0]),
        i32::try_from(vector_values[1]),
        i32::try_from(vector_values[2]),
        i32::try_from(vector_values[3]),
      );

      match (x_result, y_result, z_result, w_result) {
        (Ok(x), Ok(y), Ok(z), Ok(w)) => Ok((x, y, z, w).into()),
        _ => Err(
          (
            "vector built-in function requires 4 integer parameters",
            line_info,
          )
            .into(),
        ),
      }
    }
    8 => {
      let mut result: [i16; 8] = [0; 8];
      for (i, value) in vector_values.iter().enumerate() {
        match i16::try_from(*value as i64) {
          Ok(int_value) => result[i] = int_value,
          Err(_) => {
            return Err(
              (
                "vector built-in function requires parameters that can be converted to i32",
                line_info,
              )
                .into(),
            );
          }
        }
      }
      let result: &[i16; 8] = &result;
      Ok(result.into())
    }
    16 => {
      let mut result: [i8; 16] = [0; 16];
      for (i, value) in vector_values.iter().enumerate() {
        match i8::try_from(*value as i64) {
          Ok(int_value) => result[i] = int_value,
          Err(_) => {
            return Err(
              (
                "vector built-in function requires parameters that can be converted to i32",
                line_info,
              )
                .into(),
            );
          }
        }
      }
      let result = &result;
      Ok(result.into())
    }
    _ => Err(
      (
        "int vector built-in function requires 2, 3, 4, 8, or 16 parameters",
        line_info,
      )
        .into(),
    ),
  }
}

fn get_vec_params<const WIDTH: usize, const MAX: usize>(
  func: &Function,
  line_info: LineInfo,
) -> Result<(&Vec<Param>, usize), ShardsError> {
  let params = func.params.as_ref().ok_or(
    (
      "vector built-in function requires at least 1 parameter",
      line_info,
    )
      .into(),
  )?;
  let len = params.len();
  if len > 16 {
    return Err(
      (
        "vector built-in function requires at most 16 parameters",
        line_info,
      )
        .into(),
    );
  } else if len != 1 && WIDTH != len {
    return Err(
      (
        "vector built-in function requires 1 or the same number of parameters as the vector width",
        line_info,
      )
        .into(),
    );
  }
  Ok((params, len))
}

fn process_vector_built_in_floats_block<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  // it's either a Const or a MakeVector in this case

  let (params, len) = get_vec_params::<WIDTH, 16>(func, line_info)?;

  let has_variables = params.iter().any(|x| {
    if let Value::Identifier(_) = &x.value {
      true
    } else {
      false
    }
  });

  if !has_variables {
    let value = extract_floats_vector_var::<WIDTH>(len, params, line_info)?;
    add_const_shard2(value, line_info, e)
  } else {
    let shard = extract_make_floats_shard::<WIDTH>(len, params, line_info, e)?;
    e.shards.push(shard);
    Ok(())
  }
}

fn handle_vector_built_in_floats<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
) -> Result<Var, ShardsError> {
  let (params, len) = get_vec_params::<WIDTH, 4>(func, line_info)?;
  extract_floats_vector_var::<WIDTH>(len, params, line_info)
}

fn extract_make_floats_shard<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  fn error_requires_number(line_info: LineInfo) -> Result<AutoShardRef, ShardsError> {
    Err(
      (
        "vector built-in function requires a floating point number or identifier parameter",
        line_info,
      )
        .into(),
    )
  }

  let shard = match WIDTH {
    2 => ShardRef::create("MakeFloat2"),
    3 => ShardRef::create("MakeFloat3"),
    4 => ShardRef::create("MakeFloat4"),
    _ => {
      return Err(
        (
          "float vector built-in function requires 2, 3, or 4 parameters",
          line_info,
        )
          .into(),
      )
    }
  }
  .unwrap();
  let shard = AutoShardRef(shard);

  for i in 0..len {
    let var = match &params[i].value {
      Value::Identifier(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      Value::Number(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      _ => return error_requires_number(line_info),
    }?;
    shard
      .0
      .set_parameter(i as i32, *var.as_ref()) // Type conversion should be handled by the shard!
      .map_err(|err| {
        (
          format!(
            "Error setting parameter for MakeFloat{}, error: {}",
            WIDTH, err
          ),
          line_info,
        )
          .into()
      })?;
  }
  Ok(shard)
}

fn extract_floats_vector_var<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
) -> Result<shards::SHVar, ShardsError> {
  let mut vector_values: [f64; WIDTH] = [0.0; WIDTH];

  fn error_requires_number(line_info: LineInfo) -> Result<Var, ShardsError> {
    Err(
      (
        "vector built-in function requires a floating point number parameter",
        line_info,
      )
        .into(),
    )
  }

  for i in 0..len {
    vector_values[i] = match &params[i].value {
      Value::Number(n) => match n {
        Number::Integer(n) => *n as f64,
        Number::Float(n) => *n,
        _ => return error_requires_number(line_info),
      },
      _ => return error_requires_number(line_info),
    };
  }

  if len == 1 {
    // fill with first value
    for i in 1..WIDTH {
      vector_values[i] = vector_values[0];
    }
  }

  match WIDTH {
    2 => Ok((vector_values[0], vector_values[1]).into()),
    3 => Ok(
      (
        vector_values[0] as f32,
        vector_values[1] as f32,
        vector_values[2] as f32,
      )
        .into(),
    ),
    4 => Ok(
      (
        vector_values[0] as f32,
        vector_values[1] as f32,
        vector_values[2] as f32,
        vector_values[3] as f32,
      )
        .into(),
    ),
    _ => Err(
      (
        "float vector built-in function requires 2, 3, or 4 parameters",
        line_info,
      )
        .into(),
    ),
  }
}

fn process_color_built_in_function(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let (params, len) = get_vec_params::<4, 4>(func, line_info)?;

  let has_variables = params.iter().any(|x| {
    if let Value::Identifier(_) = &x.value {
      true
    } else {
      false
    }
  });

  if !has_variables {
    let value = handle_color_built_in(func, line_info)?;
    add_const_shard2(value, line_info, e)
  } else {
    let shard = extract_make_colors_shard(len, params, line_info, e)?;
    e.shards.push(shard);
    Ok(())
  }
}

fn extract_make_colors_shard(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  fn error_requires_number(line_info: LineInfo) -> Result<AutoShardRef, ShardsError> {
    Err(
      (
        "color built-in function requires a number or identifier parameter",
        line_info,
      )
        .into(),
    )
  }

  let shard = AutoShardRef(ShardRef::create("MakeColor").unwrap());

  for i in 0..len {
    let var = match &params[i].value {
      Value::Identifier(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      Value::Number(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      _ => return error_requires_number(line_info),
    }?;
    shard
      .0
      .set_parameter(i as i32, *var.as_ref()) // Type conversion should be handled by the shard!
      .map_err(|err| {
        (
          format!("Error setting parameter for MakeColor, error: {}", err),
          line_info,
        )
          .into()
      })?;
  }
  Ok(shard)
}

fn handle_color_built_in(func: &Function, line_info: LineInfo) -> Result<Var, ShardsError> {
  let params = func.params.as_ref().ok_or(
    (
      "color built-in function requires at least 1 parameter",
      line_info,
    )
      .into(),
  )?;
  let len = params.len();
  if len > 4 {
    return Err(
      (
        "color built-in function requires at most 4 parameters",
        line_info,
      )
        .into(),
    );
  }

  let color_int = |n: i64| {
    let n: i32 = n.try_into().map_err(|_| {
      (
        "color built-in function requires a number parameter in range of i32",
        line_info,
      )
        .into()
    })?;
    if n < 0 || n > 255 {
      Err(
        (
          "color built-in function requires a number parameter in range of u8",
          line_info,
        )
          .into(),
      )
    } else {
      Ok(n as u8)
    }
  };

  let color_float = |n: f64| {
    let n = n as f32;
    if n < 0.0 || n > 1.0 {
      Err(
        (
          "color built-in function requires a number parameter in range of 0.0 and 1.0",
          line_info,
        )
          .into(),
      )
    } else {
      Ok((n * 255.0) as u8)
    }
  };

  let mut colors = [255; 4];

  fn error_invalid_hex(line_info: LineInfo) -> Result<Var, ShardsError> {
    Err(
      (
        "color built-in function has an invalid hexadecimal parameter",
        line_info,
      )
        .into(),
    )
  }

  fn error_requires_number(line_info: LineInfo) -> Result<Var, ShardsError> {
    Err(
      (
        "color built-in function requires a number parameter",
        line_info,
      )
        .into(),
    )
  }

  let assign_colors = |n: u32, len: usize, colors: &mut [u8; 4]| match len {
    1 => {
      colors[0] = n as u8;
      colors[1] = 0;
      colors[2] = 0;
      colors[3] = 255;
    }
    2 => {
      colors[0] = (n >> 8) as u8;
      colors[1] = n as u8;
      colors[2] = 0;
      colors[3] = 255;
    }
    3 => {
      colors[0] = (n >> 16) as u8;
      colors[1] = (n >> 8) as u8;
      colors[2] = n as u8;
      colors[3] = 255;
    }
    4 => {
      colors[0] = (n >> 24) as u8;
      colors[1] = (n >> 16) as u8;
      colors[2] = (n >> 8) as u8;
      colors[3] = n as u8;
    }
    _ => {}
  };

  if len == 1 {
    match &params[0].value {
      Value::Number(n) => match n {
        Number::Integer(n) => colors.fill(color_int(*n)?),
        Number::Float(n) => colors.fill(color_float(*n)?),
        Number::Hexadecimal(n) => {
          let n = &n.as_str()[2..];
          let s_len = n.len();
          if s_len > 8 {
            return error_invalid_hex(line_info);
          }
          if let Ok(n) = u32::from_str_radix(n, 16) {
            assign_colors(n, s_len / 2, &mut colors);
          } else {
            return error_invalid_hex(line_info);
          }
        }
      },
      _ => return error_requires_number(line_info),
    }
  } else {
    for i in 0..params.len() {
      colors[i] = match &params[i].value {
        Value::Number(n) => match n {
          Number::Integer(n) => color_int(*n)?,
          Number::Float(n) => color_float(*n)?,
          Number::Hexadecimal(_) => return error_invalid_hex(line_info),
        },
        _ => return error_requires_number(line_info),
      };
    }
  }

  Ok(Var::color_u8s(colors[0], colors[1], colors[2], colors[3]))
}

fn find_mesh<'a>(name: &'a RcStrWrapper, env: &'a mut EvalEnv) -> Option<&'a mut Mesh> {
  if let Some(mesh) = env.meshes.get_mut(name) {
    Some(mesh)
  } else if let Some(parent) = env.parent {
    find_mesh(name, unsafe { &mut *(parent as *mut EvalEnv) })
  } else {
    None
  }
}

fn find_wire<'a>(name: &'a RcStrWrapper, env: &'a EvalEnv) -> Option<(Var, bool)> {
  if let Some(wire) = env.finalized_wires.get(name) {
    Some((wire.0.into(), true))
  } else if let Some(wire) = env.deferred_wires.get(name) {
    Some((wire.0 .0.into(), false))
  } else if let Some(parent) = env.parent {
    find_wire(name, unsafe { &mut *(parent as *mut EvalEnv) })
  } else {
    None
  }
}

fn find_extension<'a>(
  name: &'a RcStrWrapper,
  env: &'a mut EvalEnv,
) -> Option<&'a mut Box<dyn ShardsExtension>> {
  if let Some(extension) = env.extensions.get_mut(name) {
    Some(extension)
  } else if let Some(parent) = env.parent {
    find_extension(name, unsafe { &mut *(parent as *mut EvalEnv) })
  } else {
    None
  }
}

fn finalize_wire(
  wire: &Wire,
  name: &RcStrWrapper,
  params: *const Vec<Param>,
  line_info: LineInfo,
  env: &mut EvalEnv,
) -> Result<(), ShardsError> {
  wire.set_name(&name.0);

  shlog_trace!("Finalizing wire {}", name);

  let param_helper = ParamHelper::new(unsafe { &*params });

  // ignore first parameter, which is the name

  let mut sub_env = param_helper
    .get_param_by_name_or_index("Shards", 1)
    .map(|param| match &param.value {
      Value::Shards(seq) => eval_sequence(&seq, Some(env)),
      _ => Err(("Shards parameter must be shards", line_info).into()),
    })
    .unwrap_or(Ok(EvalEnv::default()))?;
  finalize_env(&mut sub_env)?;
  for shard in sub_env.shards.drain(..) {
    wire.add_shard(shard.0);
  }

  let looped = param_helper
    .get_param_by_name_or_index("Looped", 2)
    .map(|param| match &param.value {
      Value::Boolean(b) => Ok(*b),
      _ => Err(("Looped parameter must be a boolean", line_info).into()),
    })
    .unwrap_or(Ok(false))?;
  wire.set_looped(looped);

  if env.settings.iter().any(|&s| s.disallow_impure_wires) {
    wire.set_pure(true);
  } else {
    let pure = param_helper
      .get_param_by_name_or_index("Pure", 3)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Pure parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;
    wire.set_pure(pure);
  }

  if !env.settings.iter().any(|&s| s.disallow_unsafe) {
    let unsafe_ = param_helper
      .get_param_by_name_or_index("Unsafe", 4)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Unsafe parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;
    wire.set_unsafe(unsafe_);
  }

  if !env.settings.iter().any(|&s| s.disallow_custom_stack_sizes) {
    let stack_size = param_helper
      .get_param_by_name_or_index("StackSize", 5)
      .map(|param| match &param.value {
        Value::Number(n) => match n {
          Number::Integer(n) => Ok(*n),
          _ => Err(("StackSize parameter must be an integer", line_info).into()),
        },
        _ => Err(("StackSize parameter must be an integer", line_info).into()),
      })
      .unwrap_or(Ok(128 * 1024))?;
    // ensure stack size is a multiple of 4 and minimum 1024 bytes
    let stack_size = if stack_size < 32 * 1024 {
      32 * 1024
    } else if stack_size % 4 != 0 {
      stack_size + 4 - (stack_size % 4)
    } else {
      stack_size
    };
    wire.set_stack_size(stack_size as usize);
  }
  Ok(())
}

fn finalize_env(env: &mut EvalEnv) -> Result<(), ShardsError> {
  for wire in &env.deferred_wires {
    env
      .finalized_wires
      .insert(wire.0.clone(), wire.1 .0 .0.into());
  }
  for (name, (wire, params, line_info)) in env.deferred_wires.drain().collect::<Vec<_>>() {
    finalize_wire(&wire, &name, params, line_info, env)?;
  }
  Ok(())
}

fn eval_eval_expr(seq: &Sequence, env: &mut EvalEnv) -> Result<(ClonedVar, LineInfo), ShardsError> {
  let mut sub_env = eval_sequence(seq, Some(env))?;
  if !sub_env.shards.is_empty() {
    let line_info = sub_env.shards[0].0.get_line_info();
    // create an ephemeral wire, execute and grab result
    let wire = Wire::default();
    wire.set_name("eval-ephemeral");
    finalize_env(&mut sub_env)?;
    for shard in sub_env.shards.drain(..) {
      wire.add_shard(shard.0);
    }
    let mut mesh = Mesh::default();
    mesh.schedule(wire.0);
    loop {
      if !mesh.tick() {
        break;
      }
    }
    let info = wire.get_info();
    if info.failed {
      let msg = std::str::from_utf8(unsafe {
        slice::from_raw_parts(
          info.failureMessage.string as *const u8,
          info.failureMessage.len,
        )
      })
      .unwrap();
      Err(
        (
          msg,
          LineInfo {
            line: line_info.0 as usize,
            column: line_info.1 as usize,
          },
        )
          .into(),
      )
    } else {
      let result = unsafe { *info.finalOutput };
      Ok((
        result.into(),
        LineInfo {
          line: line_info.0 as usize,
          column: line_info.1 as usize,
        },
      ))
    }
  } else {
    Ok((ClonedVar(Var::default()), LineInfo::default()))
  }
}

pub(crate) fn eval_sequence(
  seq: &Sequence,
  parent: Option<&mut EvalEnv>,
) -> Result<EvalEnv, ShardsError> {
  let mut sub_env = EvalEnv::default();
  // inherit previous state for certain things
  if let Some(parent) = parent {
    sub_env.parent = Some(parent as *mut EvalEnv);
    sub_env.settings = parent.settings.clone(); // clone parent settings
  }
  for stmt in &seq.statements {
    eval_statement(stmt, &mut sub_env)?;
  }
  Ok(sub_env)
}

fn create_take_table_chain(
  var_name: &RcStrWrapper,
  path: &Vec<RcStrWrapper>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(var_name, line, e)?;
  for path_part in path {
    let s = Var::ephemeral_string(path_part.as_str());
    add_take_shard(&s, line, e)?;
  }
  Ok(())
}

fn create_take_seq_chain(
  var_name: &RcStrWrapper,
  path: &Vec<u32>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(var_name, line, e)?;
  for path_part in path {
    let idx = (*path_part).try_into().unwrap(); // read should have caught this
    add_take_shard(&idx, line, e)?;
  }
  Ok(())
}

fn is_forbidden_func(name: &RcStrWrapper, e: &EvalEnv) -> bool {
  //recurse env and check
  let mut env = e;
  loop {
    if env.forbidden_funcs.contains(name) {
      return true;
    }
    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return false;
    }
  }
}

/// Recurse into environment and find the suffix
fn find_current_suffix<'a>(e: &'a EvalEnv) -> Option<&'a RcStrWrapper> {
  let mut env = e;
  loop {
    if let Some(suffix) = &env.suffix {
      return Some(suffix);
    }
    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return None;
    }
  }
}

/// Recurse into environment and find the suffix for a given variable name if it exists
fn find_suffix<'a>(name: &'a RcStrWrapper, e: &'a EvalEnv) -> Option<&'a RcStrWrapper> {
  let mut env = e;
  loop {
    if let Some(suffix) = env.suffix_assigned.get(name) {
      return Some(suffix);
    }
    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return None;
    }
  }
}

/// Recurse into environment and find the replacement for a given variable name if it exists
fn find_replacement<'a>(name: &'a RcStrWrapper, e: &'a EvalEnv) -> Option<&'a Value> {
  let mut env = e;
  loop {
    if let Some(replacement) = env.replacements.get(name) {
      let replacement = *replacement;
      let replacement = unsafe { &*replacement };
      return Some(replacement);
    }
    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return None;
    }
  }
}

fn as_var(
  value: &Value,
  line_info: LineInfo,
  shard: Option<ShardRef>,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  match value {
    Value::None => Ok(SVar::NotCloned(Var::default())),
    Value::Boolean(value) => Ok(SVar::NotCloned((*value).into())),
    Value::Identifier(ref name) => {
      // could be wire or mesh as "special" cases
      if let Some((wire, _finalized)) = find_wire(name, e) {
        Ok(SVar::Cloned(wire.into()))
      } else if let Some(mesh) = find_mesh(name, e) {
        // CoreCC, 'brcM'
        let mesh_cc = fourCharacterCode(*b"brcM");
        let mut var = Var::default();
        var.valueType = SHType_Object;
        var.payload.__bindgen_anon_1.__bindgen_anon_1.objectVendorId = FRAG_CC;
        var.payload.__bindgen_anon_1.__bindgen_anon_1.objectTypeId = mesh_cc;
        var.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue = mesh.0 as *mut _;
        Ok(SVar::NotCloned(var))
      } else if let Some(replacement) = find_replacement(name, e) {
        as_var(&replacement.clone(), line_info, shard, e) // cloned to make borrow checker happy...
      } else {
        if let Some(suffix) = find_suffix(name, e) {
          let name = format!("{}{}", name, suffix);
          let mut s = Var::ephemeral_string(name.as_str());
          s.valueType = SHType_ContextVar;
          Ok(SVar::Cloned(s.into()))
        } else {
          let mut s = Var::ephemeral_string(name.as_str());
          s.valueType = SHType_ContextVar;
          Ok(SVar::NotCloned(s))
        }
      }
    }
    Value::Enum(prefix, value) => {
      let id = findEnumId(prefix.as_str());
      if let Some(id) = id {
        // decompose bits to split id into vendor and type
        // consider this is how id is composed: int64_t id = (int64_t)vendorId << 32 | typeId;
        let vendor_id = (id >> 32) as i32;
        let type_id = id as i32;
        let info = findEnumInfo(vendor_id, type_id).unwrap();
        for i in 0..info.labels.len {
          let c_str = unsafe { CStr::from_ptr(*info.labels.elements.offset(i as isize)) };
          if value == c_str.to_str().unwrap() {
            // we found the enum value
            let mut enum_var = Var::default();
            enum_var.valueType = SHType_Enum;
            let value = unsafe { *info.values.elements.offset(i as isize) };
            enum_var.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue = value;
            enum_var
              .payload
              .__bindgen_anon_1
              .__bindgen_anon_3
              .enumVendorId = vendor_id;
            enum_var
              .payload
              .__bindgen_anon_1
              .__bindgen_anon_3
              .enumTypeId = type_id;
            return Ok(SVar::NotCloned(enum_var));
          }
        }
        Err(
          (
            format!("Enum value {}.{} not found", prefix, value),
            line_info,
          )
            .into(),
        )
      } else {
        Err((format!("Enum {} not found", prefix), line_info).into())
      }
    }
    Value::Number(num) => match num {
      Number::Integer(n) => Ok(SVar::NotCloned((*n).into())),
      Number::Float(n) => Ok(SVar::NotCloned((*n).into())),
      Number::Hexadecimal(s) => {
        let s = s.as_str();
        let s = &s[2..]; // remove 0x
        let z = i64::from_str_radix(s, 16).expect("Invalid hexadecimal number"); // read should have caught this
        Ok(SVar::NotCloned(z.into()))
      }
    },
    Value::String(ref s) => {
      let s = Var::ephemeral_string(s.as_str());
      Ok(SVar::NotCloned(s))
    }
    Value::Bytes(ref b) => {
      let bytes = b.0.as_ref();
      Ok(SVar::NotCloned(bytes.into()))
    }
    Value::Float2(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Float3(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Float4(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Int2(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Int3(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Int4(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Int8(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Int16(ref val) => Ok(SVar::NotCloned(val.into())),
    Value::Seq(vec) => {
      let mut seq = SeqVar::leaking_new();
      for value in vec {
        let value = as_var(value, line_info, shard, e)?;
        seq.push(value.as_ref());
      }
      Ok(SVar::Cloned(ClonedVar(seq.0)))
    }
    Value::Table(value) => {
      let mut table = TableVar::leaking_new();
      for (key, value) in value {
        let mut key = as_var(key, line_info, shard, e)?;
        if key.as_ref().is_context_var() {
          // if the key is a context var, we need to convert it to a string
          // this allows us to have nice keys without quotes
          key.as_mut().valueType = SHType_String;
        }
        let value = as_var(value, line_info, shard, e)?;
        let key_ref = key.as_ref();
        let value_ref = value.as_ref();
        table.insert_fast(*key_ref, value_ref);
      }
      Ok(SVar::Cloned(ClonedVar(table.0)))
    }
    Value::Shards(seq) => {
      let mut sub_env = eval_sequence(&seq, Some(e))?;
      let mut seq = SeqVar::leaking_new();
      finalize_env(&mut sub_env)?;
      for shard in sub_env.shards.drain(..) {
        let s = shard.0 .0;
        let s: Var = s.into();
        debug_assert!(s.valueType == SHType_ShardRef);
        seq.push(&s);
      }
      Ok(SVar::Cloned(ClonedVar(seq.0)))
    }
    Value::Shard(shard) => {
      let s = create_shard(shard, line_info, e)?;
      let s: Var = s.0 .0.into();
      debug_assert!(s.valueType == SHType_ShardRef);
      Ok(SVar::Cloned(s.into()))
    }
    Value::EvalExpr(seq) => {
      let value = eval_eval_expr(&seq, e)?;
      Ok(SVar::Cloned(value.0))
    }
    Value::Expr(seq) => {
      let start_idx = e.shards.len();
      let mut sub_env = eval_sequence(&seq, Some(e))?;
      if !sub_env.shards.is_empty() {
        // create a temporary variable to hold the result of the expression
        let tmp_name = nanoid!(16);
        // ensure name starts with a letter
        let tmp_name = format!("t{}", tmp_name);
        // debug info
        let line_info = sub_env.shards[0].0.get_line_info();
        let line_info = LineInfo {
          line: line_info.0 as usize,
          column: line_info.1 as usize,
        };
        add_assignment_shard_no_suffix("Ref", &tmp_name, line_info, &mut sub_env)
          .map_err(|e| (format!("{:?}", e), line_info).into())?;
        // wrap into a Sub Shard
        finalize_env(&mut sub_env)?;
        let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
        // add this sub shard before the start of this pipeline!
        e.shards.insert(start_idx, sub);
        // now add a get shard to get the temporary at the end of the pipeline
        let mut s = Var::ephemeral_string(&tmp_name);
        s.valueType = SHType_ContextVar;
        Ok(SVar::Cloned(s.into()))
      } else {
        Ok(SVar::NotCloned(().into()))
      }
    }
    Value::TakeTable(var_name, path) => {
      let start_idx = e.shards.len();
      let mut sub_env = EvalEnv::default();
      create_take_table_chain(var_name, path, line_info, &mut sub_env)?;
      if !sub_env.shards.is_empty() {
        // create a temporary variable to hold the result of the expression
        let tmp_name = nanoid!(16);
        // ensure name starts with a letter
        let tmp_name = format!("t{}", tmp_name);
        add_assignment_shard_no_suffix("Ref", &tmp_name, line_info, &mut sub_env)
          .map_err(|e| (format!("{:?}", e), line_info).into())?;
        // wrap into a Sub Shard
        finalize_env(&mut sub_env)?;
        let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
        // add this sub shard before the start of this pipeline!
        e.shards.insert(start_idx, sub);
        // now add a get shard to get the temporary at the end of the pipeline
        add_get_shard_no_suffix(&tmp_name, line_info, e)?;

        let mut s = Var::ephemeral_string(tmp_name.as_str());
        s.valueType = SHType_ContextVar;
        Ok(SVar::Cloned(s.into()))
      } else {
        panic!("TakeTable should always return a shard")
      }
    }
    Value::TakeSeq(var_name, path) => {
      let start_idx = e.shards.len();
      let mut sub_env = EvalEnv::default();
      create_take_seq_chain(var_name, path, line_info, &mut sub_env)?;
      if !sub_env.shards.is_empty() {
        // create a temporary variable to hold the result of the expression
        let tmp_name = nanoid!(16);
        // ensure name starts with a letter
        let tmp_name = format!("t{}", tmp_name);
        add_assignment_shard_no_suffix("Ref", &tmp_name, line_info, &mut sub_env)
          .map_err(|e| (format!("{:?}", e), line_info).into())?;
        // wrap into a Sub Shard
        finalize_env(&mut sub_env)?;
        let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
        // add this sub shard before the start of this pipeline!
        e.shards.insert(start_idx, sub);
        // now add a get shard to get the temporary at the end of the pipeline
        add_get_shard_no_suffix(&tmp_name, line_info, e)?;

        let mut s = Var::ephemeral_string(tmp_name.as_str());
        s.valueType = SHType_ContextVar;
        Ok(SVar::Cloned(s.into()))
      } else {
        panic!("TakeTable should always return a shard")
      }
    }
    Value::Func(func) => match func.name.as_str() {
      "color" => Ok(SVar::NotCloned(handle_color_built_in(func, line_info)?)),
      "i2" => Ok(SVar::NotCloned(handle_vector_built_in_ints::<2>(
        func, line_info,
      )?)),
      "i3" => Ok(SVar::NotCloned(handle_vector_built_in_ints::<3>(
        func, line_info,
      )?)),
      "i4" => Ok(SVar::NotCloned(handle_vector_built_in_ints::<4>(
        func, line_info,
      )?)),
      "i8" => Ok(SVar::NotCloned(handle_vector_built_in_ints::<8>(
        func, line_info,
      )?)),
      "i16" => Ok(SVar::NotCloned(handle_vector_built_in_ints::<16>(
        func, line_info,
      )?)),
      "f2" => Ok(SVar::NotCloned(handle_vector_built_in_floats::<2>(
        func, line_info,
      )?)),
      "f3" => Ok(SVar::NotCloned(handle_vector_built_in_floats::<3>(
        func, line_info,
      )?)),
      "f4" => Ok(SVar::NotCloned(handle_vector_built_in_floats::<4>(
        func, line_info,
      )?)),
      "type" => process_type(func, line_info, e),
      "ast" => process_ast(func, line_info, e),
      _ => {
        if let Some(defined_value) = find_defined(&func.name, e) {
          let replacement = unsafe { &*defined_value };
          as_var(replacement, line_info, shard, e)
        } else if let Some(ast_json) = process_macro(func, func.name.as_str(), line_info, e)? {
          let ast_json: &str = ast_json.as_ref().try_into().map_err(|_| {
            (
              "macro built-in function Shards should output a Json string",
              line_info,
            )
              .into()
          })?;

          thread_local! {
            pub static TMP_VALUE: RefCell<Option<Value>> = RefCell::new(None);
          }

          // in this case we expect the ast to be a value
          let decoded_json: Value = serde_json::from_str(ast_json).map_err(|e| {
            (
              format!(
                "macro built-in function Shards should return a valid Json string: {}",
                e
              ),
              line_info,
            )
              .into()
          })?;

          TMP_VALUE.with(|f| {
            let mut v = f.borrow_mut();
            *v = Some(decoded_json.clone());
            as_var(v.as_ref().unwrap(), line_info, shard, e)
          })
        } else if let Some(extension) = find_extension(&func.name, e) {
          let v = extension.process_to_var(func, line_info)?;
          Ok(SVar::Cloned(v))
        } else {
          Err((format!("Undefined function {}", func.name), line_info).into())
        }
      }
    },
  }
}

fn process_ast(func: &Function, line_info: LineInfo, e: &mut EvalEnv) -> Result<SVar, ShardsError> {
  // ast to json
  //serde_json::to_str(func.params[0].v)
  let first_param = func.params.as_ref().unwrap().get(0).ok_or(
    (
      "ast built-in function requires at least one parameter",
      line_info,
    )
      .into(),
  )?;

  // if param is an identifier, we need to resolve it
  let first_param = match &first_param.value {
    Value::Identifier(name) => {
      if let Some(replacement) = find_replacement(name, e) {
        replacement
      } else {
        &first_param.value
      }
    }
    _ => &first_param.value,
  };

  let json = serde_json::to_string(&first_param).map_err(|e| {
    (
      format!("ast built-in function failed to convert to json: {}", e),
      line_info,
    )
      .into()
  })?;

  let s = Var::ephemeral_string(json.as_str());
  Ok(SVar::Cloned(s.into()))
}

fn process_type(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  if let Some(ref params) = func.params {
    let param_helper = ParamHelper::new(params);

    let type_ = param_helper.get_param_by_name_or_index("Type", 0).ok_or(
      (
        "type built-in function requires a Type parameter",
        line_info,
      )
        .into(),
    )?;

    let is_var = param_helper
      .get_param_by_name_or_index("Variable", 1)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Variable parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;

    let vendor_id = param_helper.get_param_by_name_or_index("ObjectVendor", 2);
    let object_id = param_helper.get_param_by_name_or_index("ObjectTypeId", 3);

    let mut type_ = process_type_desc(&type_.value, line_info, e)?;

    match (vendor_id, object_id) {
        (Some(vendor_id), Some(object_id)) => {
          // fix up the type
            let native_type = unsafe {type_.as_mut().payload.__bindgen_anon_1.typeValue};
            let native_type = unsafe {&mut *native_type};
            if native_type.basicType != SHType_Object {
              return Err(
                (
                  "type built-in function, when Type.Object, requires both ObjectVendor and ObjectTypeId parameters",
                  line_info,
                )
                  .into(),
              )
            }

          fn parse_vendor_or_object_id(
              value: &Value,
              err_msg: &'static str,
              line_info: &LineInfo // assuming this type, replace with the actual one
          ) -> Result<i32, ShardsError> { // assuming this Error type, replace with the actual one
              match value {
                  Value::Number(n) => match n {
                      Number::Integer(v) => i32::try_from(*v).map_err(|_| {
                          (format!("{} failed to parse parameter as integer", err_msg), *line_info).into()
                      }),
                      Number::Hexadecimal(v) => i32::from_str_radix(v.as_str(), 16).map_err(|_| {
                          (format!("{} failed to parse parameter as hexadecimal", err_msg), *line_info).into()
                      }),
                      _ => Err((
                          format!("{} requires both parameters as integer or hexadecimal", err_msg),
                          *line_info,
                      ).into())
                  },
                  _ => Err((
                      format!("{} requires both parameters", err_msg),
                      *line_info,
                  ).into())
              }
          }

          let vendor_id = parse_vendor_or_object_id(&vendor_id.value, "type built-in function, when Type.Object", &line_info)?;
          let object_id = parse_vendor_or_object_id(&object_id.value, "type built-in function, when Type.Object", &line_info)?;

          native_type.details.object.vendorId = vendor_id;
          native_type.details.object.typeId = object_id;
        },
        (None, None) => {},
        _ => {
          return Err(
            (
              "type built-in function, when Type.Object, requires both ObjectVendor and ObjectTypeId parameters",
              line_info,
            )
              .into(),
          )
        }
      }

    if is_var {
      let inner_type = unsafe { *type_.as_ref().payload.__bindgen_anon_1.typeValue };
      let inner_types = [inner_type];
      Ok(SVar::Cloned(ClonedVar::from(Type::context_variable(
        &inner_types,
      ))))
    } else {
      Ok(type_)
    }
  } else {
    Err(
      (
        "type built-in function requires at least a Type parameter",
        line_info,
      )
        .into(),
    )
  }
}

fn process_type_desc(
  value: &Value,
  line_info: LineInfo,
  env: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  let type_ = match &value {
    Value::Shards(seq) => {
      // ensure there is a single shard
      // and ensure that shard only has a single output type
      let sub_env = eval_sequence(&seq, None)?;
      if sub_env.shards.len() != 1 {
        return Err(
          (
            "Type Shards parameter must contain a single shard",
            line_info,
          )
            .into(),
        );
      }
      let shard = &sub_env.shards[0].0;
      let output_types = shard.output_types();
      if output_types.len() != 1 {
        return Err(
          (
            "Type Shards parameter must contain a shard with a single output type",
            line_info,
          )
            .into(),
        );
      }
      Ok(SVar::Cloned(ClonedVar::from(output_types[0])))
    }
    Value::Enum(_, _) => process_type_enum(&value, line_info),
    Value::Seq(seq) => {
      // iterate all and as_var them, ensure it's a Type Type though

      let mut types = Vec::new(); // actual storage
      for value in seq {
        let value = process_type_desc(value, line_info, env)?;
        if value.as_ref().valueType != SHType_Type {
          return Err(("Type Seq parameter can only contain Type values", line_info).into());
        }
        types.push(value);
      }

      let mut inner_types = Vec::new(); // actually weak storage
      for inner_type in &types {
        let inner_type = inner_type.as_ref();
        if inner_type.valueType != SHType_Type {
          return Err(("Type Seq parameter can only contain Type values", line_info).into());
        }
        let inner_type = unsafe { &*inner_type.payload.__bindgen_anon_1.typeValue };
        inner_types.push(*inner_type);
      }

      Ok(SVar::Cloned(ClonedVar::from(Type::seq(&inner_types))))
    }
    Value::Table(pairs) => {
      let mut keys = Vec::new(); // actual storage
      let mut types = Vec::new(); // actual storage
      for (key, value) in pairs {
        let key = as_var(key, line_info, None, env)?;
        let value = process_type_desc(value, line_info, env)?;
        keys.push(key);
        types.push(value);
      }

      // we need to wrap it into a Table Type
      let mut inner_keys = Vec::new(); // actually weak storage
      let mut inner_types = Vec::new(); // actually weak storage
      for (ref key, ref value) in keys.into_iter().zip(types.into_iter()) {
        let key = key.as_ref();
        inner_keys.push(*key);

        let value = value.as_ref();
        if value.valueType != SHType_Type {
          return Err(
            (
              "Type Table parameter can only contain Type values",
              line_info,
            )
              .into(),
          );
        }
        let value = unsafe { &*value.payload.__bindgen_anon_1.typeValue };
        inner_types.push(*value);
      }

      Ok(SVar::Cloned(ClonedVar::from(Type::table(
        &inner_keys,
        &inner_types,
      ))))
    }
    Value::Func(_) => {
      // just as_var bypass it
      as_var(&value, line_info, None, env)
    }
    Value::Shard(shard) => {
      let s = create_shard(shard, line_info, env)?;
      let output_types = s.0.output_types();
      if output_types.len() != 1 {
        return Err(
          (
            "Type Shards parameter must contain a shard with a single output type",
            line_info,
          )
            .into(),
        );
      }
      Ok(SVar::Cloned(ClonedVar::from(output_types[0])))
    }
    _ => Err(
      (
        "Type parameter can be any of the following: Enum, Seq, Table, Func",
        line_info,
      )
        .into(),
    ),
  }?;
  Ok(type_)
}

fn process_type_enum(value: &Value, line_info: LineInfo) -> Result<SVar, ShardsError> {
  let (prefix, value) = match value {
    Value::Enum(prefix, value) => (prefix, value),
    _ => return Err(("Type Enum parameter must be an Enum", line_info).into()),
  };
  if prefix == "Type" {
    match value.as_str() {
      "None" => Ok(SVar::Cloned(ClonedVar::from(common_type::none))),
      "Any" => Ok(SVar::Cloned(ClonedVar::from(common_type::any))),
      "Bool" => Ok(SVar::Cloned(ClonedVar::from(common_type::bool))),
      "Int" => Ok(SVar::Cloned(ClonedVar::from(common_type::int))),
      "Int2" => Ok(SVar::Cloned(ClonedVar::from(common_type::int2))),
      "Int3" => Ok(SVar::Cloned(ClonedVar::from(common_type::int3))),
      "Int4" => Ok(SVar::Cloned(ClonedVar::from(common_type::int4))),
      "Int8" => Ok(SVar::Cloned(ClonedVar::from(common_type::int8))),
      "Int16" => Ok(SVar::Cloned(ClonedVar::from(common_type::int16))),
      "Float" => Ok(SVar::Cloned(ClonedVar::from(common_type::float))),
      "Float2" => Ok(SVar::Cloned(ClonedVar::from(common_type::float2))),
      "Float3" => Ok(SVar::Cloned(ClonedVar::from(common_type::float3))),
      "Float4" => Ok(SVar::Cloned(ClonedVar::from(common_type::float4))),
      "Color" => Ok(SVar::Cloned(ClonedVar::from(common_type::color))),
      "Wire" => Ok(SVar::Cloned(ClonedVar::from(common_type::wire))),
      "Shard" => Ok(SVar::Cloned(ClonedVar::from(common_type::shard))),
      "Bytes" => Ok(SVar::Cloned(ClonedVar::from(common_type::bytes))),
      "String" => Ok(SVar::Cloned(ClonedVar::from(common_type::string))),
      "Image" => Ok(SVar::Cloned(ClonedVar::from(common_type::image))),
      "Audio" => Ok(SVar::Cloned(ClonedVar::from(common_type::audio))),
      "Object" => Ok(SVar::Cloned(ClonedVar::from(common_type::object))),
      _ => Err((format!("Unknown Type enum value {}", value), line_info).into()),
    }
  } else {
    let id = findEnumId(prefix.as_str())
      .ok_or((format!("Enum {} not found", prefix), line_info).into())?;
    let vendor_id = (id >> 32) as i32;
    let type_id = id as i32;
    Ok(SVar::Cloned(ClonedVar::from(Type::enumeration(
      vendor_id, type_id,
    ))))
  }
}

fn add_shard(shard: &Function, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let s = create_shard(shard, line_info, e)?;
  e.shards.push(s);
  Ok(())
}

fn create_shard(
  shard: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  if is_forbidden_func(&shard.name, e) {
    return Err((format!("Forbidden shard {}", shard.name), line_info).into());
  }
  let s = ShardRef::create(shard.name.as_str()).ok_or(
    (
      format!("Shard {} does not exist", shard.name.as_str()),
      line_info,
    )
      .into(),
  )?;
  let s = AutoShardRef(s);
  let mut idx = 0i32;
  let mut as_idx = true;
  let info = s.0.parameters();
  if let Some(ref params) = shard.params {
    for param in params {
      if let Some(ref name) = param.name {
        as_idx = false;
        let mut found = false;
        for (i, info) in info.iter().enumerate() {
          let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() };
          if param_name == name.as_str() {
            set_shard_parameter(info, e, &param.value, &s, i, line_info)?;
            found = true;
            break;
          }
        }
        if !found {
          let msg = format!("Unknown parameter '{}'", name);
          return Err((msg, line_info).into());
        }
      } else {
        if !as_idx {
          return Err(("Unnamed parameter after named parameter", line_info).into());
        }
        set_shard_parameter(
          &info[idx as usize],
          e,
          &param.value,
          &s,
          idx as usize,
          line_info,
        )?;
      }
      idx += 1;
    }
  }
  Ok(s)
}

fn set_shard_parameter(
  info: &shards::SHParameterInfo,
  env: &mut EvalEnv,
  value: &Value,
  s: &AutoShardRef,
  i: usize,
  line_info: LineInfo,
) -> Result<(), ShardsError> {
  let var_value = as_var(value, line_info, Some(s.0), env).map_err(|e| e)?;
  if info.variableSetter {
    let name = match value {
      Value::Identifier(name) => name,
      _ => panic!("Expected an identifier"), // The actual Shard is violating the standard - panic here
    };
    if var_value.as_ref().valueType != SHType_ContextVar {
      panic!("Expected a context variable") // The actual Shard is violating the standard - panic here
    }
    let suffix = find_current_suffix(env);
    if let Some(suffix) = suffix {
      // fix up the value to be a suffixed variable if we have a suffix
      let new_name = format!("{}{}", name, suffix);
      // also add to suffix_assigned
      env
        .suffix_assigned
        .insert(name.clone().into(), suffix.clone());
      let mut new_name = Var::ephemeral_string(new_name.as_str());
      new_name.valueType = SHType_ContextVar;
      if let Err(e) = s.0.set_parameter(
        i.try_into().expect("Too many parameters"),
        *new_name.as_ref(),
      ) {
        Err(
          (
            format!("Failed to set parameter (1), error: {}", e),
            line_info,
          )
            .into(),
        )
      } else {
        Ok(())
      }
    } else {
      if let Err(e) = s.0.set_parameter(
        i.try_into().expect("Too many parameters"),
        *var_value.as_ref(),
      ) {
        Err(
          (
            format!("Failed to set parameter (2), error: {}", e),
            line_info,
          )
            .into(),
        )
      } else {
        Ok(())
      }
    }
  } else {
    if let Err(e) = s.0.set_parameter(
      i.try_into().expect("Too many parameters"),
      *var_value.as_ref(),
    ) {
      Err(
        (
          format!("Failed to set parameter (3), error: {}", e),
          line_info,
        )
          .into(),
      )
    } else {
      Ok(())
    }
  }
}

fn add_const_shard2(value: Var, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Const").unwrap();
  let shard = AutoShardRef(shard);
  shard
    .0
    .set_parameter(0, value)
    .map_err(|e| (e, line_info).into())?;
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  e.shards.push(shard);
  Ok(())
}

fn add_const_shard(value: &Value, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = match value {
    Value::Identifier(name) => {
      // we might be a replacement though!
      if let Some(replacement) = find_replacement(name, e) {
        // we need to evaluate the replacement as not everything can be a const
        match replacement {
          Value::None
          | Value::Boolean(_)
          | Value::Enum(_, _)
          | Value::Number(_)
          | Value::String(_)
          | Value::Bytes(_)
          | Value::Int2(_)
          | Value::Int3(_)
          | Value::Int4(_)
          | Value::Int8(_)
          | Value::Int16(_)
          | Value::Float2(_)
          | Value::Float3(_)
          | Value::Float4(_)
          | Value::Seq(_)
          | Value::EvalExpr(_)
          | Value::Expr(_)
          | Value::TakeTable(_, _)
          | Value::TakeSeq(_, _)
          | Value::Func(_)
          | Value::Table(_) => {
            let shard = ShardRef::create("Const").unwrap();
            let shard = AutoShardRef(shard);
            let value = as_var(&replacement.clone(), line_info, Some(shard.0), e)?;
            shard
              .0
              .set_parameter(0, *value.as_ref())
              .map_err(|e| (format!("{}", e), line_info).into())?;
            Some(shard)
          }
          Value::Identifier(_) => {
            let shard = ShardRef::create("Get").unwrap();
            let shard = AutoShardRef(shard);
            // todo - avoid clone
            let value = as_var(&replacement.clone(), line_info, Some(shard.0), e)?;
            shard
              .0
              .set_parameter(0, *value.as_ref())
              .map_err(|e| (format!("{}", e), line_info).into())?;
            Some(shard)
          }
          Value::Shard(shard) => {
            // add ourselves
            // todo - avoid clone
            Some(create_shard(&shard.clone(), line_info, e)?)
          }
          Value::Shards(seq) => {
            // purely include the ast of the sequence
            let seq = seq.clone(); // todo - avoid clone
            for stmt in &seq.statements {
              eval_statement(stmt, e)?;
            }
            None
          }
        }
      } else {
        let shard = ShardRef::create("Get").unwrap();
        let shard = AutoShardRef(shard);
        let value = as_var(value, line_info, Some(shard.0), e)?;
        shard
          .0
          .set_parameter(0, *value.as_ref())
          .map_err(|e| (format!("{}", e), line_info).into())?;
        Some(shard)
      }
    }
    _ => {
      let shard = ShardRef::create("Const").unwrap();
      let shard = AutoShardRef(shard);
      let value = as_var(value, line_info, Some(shard.0), e)?;
      shard
        .0
        .set_parameter(0, *value.as_ref())
        .map_err(|e| (format!("{}", e), line_info).into())?;
      Some(shard)
    }
  };
  if let Some(shard) = shard {
    shard.0.set_line_info((
      line_info.line.try_into().expect("Too many lines"),
      line_info.column.try_into().expect("Oversized column"),
    ));
    e.shards.push(shard);
  }
  Ok(())
}

fn make_sub_shard(
  shards: Vec<AutoShardRef>,
  line_info: LineInfo,
) -> Result<AutoShardRef, ShardsError> {
  let shard = ShardRef::create("Sub").unwrap();
  let shard = AutoShardRef(shard);
  let mut seq = SeqVar::leaking_new();
  for shard in shards {
    let s = shard.0 .0;
    let s: Var = s.into();
    debug_assert!(s.valueType == SHType_ShardRef);
    seq.push(&s);
  }
  shard
    .0
    .set_parameter(0, seq.0.into())
    .map_err(|e| (format!("{}", e), line_info).into())?;
  destroyVar(&mut seq.0);
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  Ok(shard)
}

fn add_take_shard(target: &Var, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Take").unwrap();
  let shard = AutoShardRef(shard);
  shard
    .0
    .set_parameter(0, *target)
    .map_err(|e| (format!("{}", e), line_info).into())?;
  shard.0.set_line_info((
    line_info.line.try_into().unwrap(),
    line_info.column.try_into().unwrap(),
  ));
  e.shards.push(shard);
  Ok(())
}

fn add_get_shard(name: &RcStrWrapper, line: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Get").unwrap();
  let shard = AutoShardRef(shard);
  if let Some(suffix) = find_suffix(name, e) {
    let name = format!("{}{}", name, suffix);
    let name = Var::ephemeral_string(&name);
    shard
      .0
      .set_parameter(0, name)
      .map_err(|e| (format!("{}", e), line).into())?;
  } else {
    let name = Var::ephemeral_string(name.as_str());
    shard
      .0
      .set_parameter(0, name)
      .map_err(|e| (format!("{}", e), line).into())?;
  }
  shard.0.set_line_info((
    line.line.try_into().unwrap(),
    line.column.try_into().unwrap(),
  ));
  e.shards.push(shard);
  Ok(())
}

fn add_get_shard_no_suffix(name: &str, line: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Get").unwrap();
  let shard = AutoShardRef(shard);
  let name = Var::ephemeral_string(name);
  shard
    .0
    .set_parameter(0, name)
    .map_err(|e| (e, line).into())?;
  shard.0.set_line_info((
    line.line.try_into().unwrap(),
    line.column.try_into().unwrap(),
  ));
  e.shards.push(shard);
  Ok(())
}

/// Recurse into environment and find the replacement for a given @ call name if it exists
fn find_shards_group<'a>(name: &'a RcStrWrapper, e: &'a EvalEnv) -> Option<&'a ShardsGroup> {
  let mut env = e;
  loop {
    if let Some(group) = env.shards_groups.get(name) {
      return Some(group);
    }

    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return None;
    }
  }
}

/// Recurse into environment and find the replacement for a given @ call name if it exists
fn find_macro_group<'a>(name: &'a RcStrWrapper, e: &'a EvalEnv) -> Option<&'a ShardsGroup> {
  let mut env = e;
  loop {
    if let Some(group) = env.macro_groups.get(name) {
      return Some(group);
    }

    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return None;
    }
  }
}

/// Recurse into environment and find the replacement for a given @ call name if it exists
fn find_defined<'a>(name: &'a RcStrWrapper, e: &'a EvalEnv) -> Option<*const Value> {
  let mut env = e;
  loop {
    if let Some(val) = env.definitions.get(name) {
      return Some(*val);
    }

    if let Some(parent) = env.parent {
      env = unsafe { &*parent };
    } else {
      return None;
    }
  }
}

fn get_mesh<'a>(
  param: &'a Param,
  find_mesh: impl Fn(&'a RcStrWrapper, &'a mut EvalEnv) -> Option<&'a mut Mesh>,
  e: &'a mut EvalEnv,
  block: &Block,
) -> Result<&'a mut Mesh, ShardsError> {
  match &param.value {
    Value::Identifier(name) => find_mesh(name, e).ok_or_else(|| {
      (
        "run built-in function requires a valid mesh parameter",
        block.line_info.unwrap_or_default(),
      )
        .into()
    }),
    _ => Err(
      (
        "run built-in function requires a mesh parameter",
        block.line_info.unwrap_or_default(),
      )
        .into(),
    ),
  }
}

fn process_macro(
  func: &Function,
  unknown: &str,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<Option<ClonedVar>, ShardsError> {
  if let Some(group) = find_macro_group(&func.name, e) {
    let args = unsafe { &*group.args };
    let shards = unsafe { &*group.shards };

    if args.len() != func.params.as_ref().unwrap().len() {
      return Err(
        (
          format!("Macro {} requires {} parameters", unknown, args.len()),
          line_info,
        )
          .into(),
      );
    }

    let mut eval_env = EvalEnv::default();
    eval_env.parent = Some(e as *const EvalEnv);
    eval_env.settings = e.settings.clone(); // clone parent settings

    // set a random suffix
    eval_env.suffix = Some(nanoid!(16).into());

    for i in 0..args.len() {
      let arg = &args[i];
      // arg has to be Identifier
      let arg = match arg {
        Value::Identifier(arg) => arg,
        _ => {
          return Err(
            (
              format!(
                "Shards template {} parameters should be identifiers",
                unknown,
              ),
              line_info,
            )
              .into(),
          );
        }
      };
      let param = &func.params.as_ref().unwrap()[i];
      if param.name.is_some() {
        return Err(
          (
            format!(
              "Shards template {} does not accept named parameters",
              unknown
            ),
            line_info,
          )
            .into(),
        );
      }

      // and add new replacement
      let value_ptr = &param.value as *const _;
      eval_env.replacements.insert(arg.to_owned(), value_ptr);
    }

    // ok so a macro is AST in Shards tables that we translate into Json and deserialize as AST
    let (ast_json, _) = eval_eval_expr(shards, &mut eval_env)?;
    Ok(Some(ast_json))
  } else {
    Ok(None)
  }
}

fn process_shards(
  func: &Function,
  unknown: &str,
  block: &Block,
  e: &mut EvalEnv,
) -> Result<Option<EvalEnv>, ShardsError> {
  if let Some(group) = find_shards_group(&func.name, e) {
    let args = unsafe { &*group.args };
    let shards = unsafe { &*group.shards };

    if args.len() != func.params.as_ref().unwrap().len() {
      return Err(
        (
          format!(
            "Shards template {} requires {} parameters",
            unknown,
            args.len()
          ),
          block.line_info.unwrap_or_default(),
        )
          .into(),
      );
    }

    let mut sub_env = EvalEnv::default();
    sub_env.parent = Some(e as *const EvalEnv);
    sub_env.settings = e.settings.clone(); // clone parent settings

    // set a random suffix
    sub_env.suffix = Some(nanoid!(16).into());

    for i in 0..args.len() {
      let arg = &args[i];
      // arg has to be Identifier
      let arg = match arg {
        Value::Identifier(arg) => arg,
        _ => {
          return Err(
            (
              format!(
                "Shards template {} parameters should be identifiers",
                unknown,
              ),
              block.line_info.unwrap_or_default(),
            )
              .into(),
          );
        }
      };
      let param = &func.params.as_ref().unwrap()[i];
      if param.name.is_some() {
        return Err(
          (
            format!(
              "Shards template {} does not accept named parameters",
              unknown
            ),
            block.line_info.unwrap_or_default(),
          )
            .into(),
        );
      }

      // and add new replacement
      let value_ptr = &param.value as *const _;
      sub_env.replacements.insert(arg.to_owned(), value_ptr);
    }

    for stmt in &shards.statements {
      eval_statement(stmt, &mut sub_env)?;
    }

    Ok(Some(sub_env))
  } else {
    Ok(None)
  }
}

fn eval_pipeline(pipeline: &Pipeline, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let start_idx = e.shards.len();
  for block in &pipeline.blocks {
    let _ = match &block.content {
      BlockContent::Shard(shard) => add_shard(shard, block.line_info.unwrap_or_default(), e),
      BlockContent::Shards(seq) => {
        let mut sub_env = eval_sequence(&seq, Some(e))?;

        // if we have a sub env, we need to finalize it
        if !sub_env.shards.is_empty() {
          // sub shards leaks assignments to the parent env
          for name in sub_env.suffix_assigned.drain() {
            e.suffix_assigned.insert(name.0, name.1);
          }
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(
            sub_env.shards.drain(..).collect(),
            block.line_info.unwrap_or_default(),
          )?;
          e.shards.push(sub);
        }

        Ok(())
      }
      BlockContent::Const(value) => add_const_shard(value, block.line_info.unwrap_or_default(), e),
      BlockContent::TakeTable(name, path) => {
        create_take_table_chain(name, path, block.line_info.unwrap_or_default(), e)
      }
      BlockContent::TakeSeq(name, path) => {
        create_take_seq_chain(name, path, block.line_info.unwrap_or_default(), e)
      }
      BlockContent::EvalExpr(seq) => {
        let value = eval_eval_expr(&seq, e)?;
        add_const_shard2(value.0 .0, value.1, e)
      }
      BlockContent::Expr(seq) => {
        eval_expr(seq, e, block, start_idx)?;
        Ok(())
      }
      BlockContent::Embed(seq) => {
        // purely include the ast of the sequence
        for stmt in &seq.statements {
          eval_statement(stmt, e)?;
        }
        Ok(())
      }
      BlockContent::Func(func) => {
        if is_forbidden_func(&func.name, e) {
          return Err(
            (
              format!("Forbidden function {}", func.name),
              block.line_info.unwrap_or_default(),
            )
              .into(),
          );
        }
        match func.name.as_str() {
          "ignore" => {
            // ignore is a special function that does nothing
            Ok(())
          }
          "define" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "define built-in function requires Name parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              let value = param_helper.get_param_by_name_or_index("Value", 1).ok_or(
                (
                  "define built-in function requires Value parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              match (name, value) {
                (
                  Param {
                    value: Value::Identifier(name),
                    ..
                  },
                  value,
                ) => {
                  if let Some(_) = find_defined(name, e) {
                    return Err(
                      (
                        format!("{} already defined", name),
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    );
                  }

                  e.definitions.insert(name.clone(), &value.value);
                  Ok(())
                }
                _ => Err(
                  (
                    "define built-in function requires Name parameter to be an identifier",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "const built-in function requires proper parameters",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "wire" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "wire built-in function requires a Name parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              // just add to deferred wires, evaluate later!
              match &name.value {
                Value::Identifier(name) => {
                  if let Some(_) = find_wire(name, e) {
                    return Err(
                      (
                        format!("wire {} already exists", name),
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    );
                  }

                  let params_ptr = func.params.as_ref().unwrap() as *const Vec<Param>;
                  shlog_trace!("Adding deferred wire {}", name);
                  e.deferred_wires.insert(
                    name.clone(),
                    (
                      Wire::default(),
                      params_ptr,
                      block.line_info.unwrap_or_default(),
                    ),
                  );
                  Ok(())
                }
                _ => Err(
                  (
                    "wire built-in function requires a string parameter",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "wire built-in function requires proper parameters",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "template" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "shards built-in function requires a Name parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              let args = param_helper.get_param_by_name_or_index("Args", 1).ok_or(
                (
                  "shards built-in function requires an Args parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              let shards = param_helper.get_param_by_name_or_index("Shards", 2).ok_or(
                (
                  "shards built-in function requires a Shards parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              match (&name.value, &args.value, &shards.value) {
                (Value::Identifier(name), Value::Seq(args), Value::Shards(shards)) => {
                  if let Some(_) = find_shards_group(name, e) {
                    return Err(
                      (
                        format!("template {} already exists", name),
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    );
                  }

                  let args_ptr = args as *const _;
                  let shards_ptr = shards as *const _;
                  e.shards_groups.insert(
                    name.clone(),
                    ShardsGroup {
                      args: args_ptr,
                      shards: shards_ptr,
                    },
                  );
                  Ok(())
                }
                _ => Err(
                  (
                    "shards built-in function requires valid parameters",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "shards built-in function requires a Name, Args and Shards parameters",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "mesh" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "mesh built-in function requires a name parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              match &name.value {
                Value::Identifier(name) => {
                  if let Some(_) = find_mesh(name, e) {
                    return Err(
                      (
                        format!("mesh {} already exists", name),
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    );
                  }

                  e.meshes.insert(name.clone(), Mesh::default());
                  Ok(())
                }
                _ => Err(
                  (
                    "mesh built-in function requires an identifier parameter",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "mesh built-in function requires a parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "schedule" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let mesh_id = param_helper.get_param_by_name_or_index("Mesh", 0).ok_or(
                (
                  "schedule built-in function requires a mesh parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;
              let wire_id = param_helper.get_param_by_name_or_index("Wire", 1).ok_or(
                (
                  "schedule built-in function requires a wire parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              // make sure the env is fully resolved before schedule cos that will calls compose and what not!
              finalize_env(e)?;

              // wire is likely lazy so we need to evaluate it
              let wire = match &wire_id.value {
                // can be only identifier or string
                Value::Identifier(name) => {
                  if let Some(wire) = e.finalized_wires.get(name) {
                    Ok(wire.0)
                  } else {
                    Err(
                      (
                        "schedule built-in function requires a valid wire parameter",
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    )
                  }
                }
                _ => Err(
                  (
                    "schedule built-in function requires a wire parameter",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
              }?;

              let mesh = get_mesh(&mesh_id, find_mesh, e, block)?;

              mesh.schedule(wire.as_ref().try_into().unwrap());

              Ok(())
            } else {
              Err(
                (
                  "schedule built-in function requires 2 parameters",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "run" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let mesh_id = param_helper.get_param_by_name_or_index("Mesh", 0).ok_or(
                (
                  "run built-in function requires a mesh parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;
              let tick_param = param_helper.get_param_by_name_or_index("TickTime", 1);
              let iterations_param = param_helper.get_param_by_name_or_index("Iterations", 2);
              let fps_param = param_helper.get_param_by_name_or_index("FPS", 3);

              let tick = match (tick_param, fps_param) {
                (Some(rate), None) => {
                  if let Value::Number(Number::Float(n)) = &rate.value {
                    Ok(Some(*n))
                  } else {
                    Err(
                      (
                        "run built-in function requires a float number rate parameter",
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    )
                  }
                }
                (None, Some(fps)) => {
                  if let Value::Number(n) = &fps.value {
                    let tick_val = match n {
                      Number::Integer(n) => 1.0 / (*n as f64),
                      Number::Float(n) => 1.0 / *n,
                      _ => {
                        return Err(
                          (
                            "run built-in function requires a float number rate parameter",
                            block.line_info.unwrap_or_default(),
                          )
                            .into(),
                        )
                      }
                    };
                    Ok(Some(tick_val))
                  } else {
                    Err(
                      (
                        "run built-in function requires a float number rate parameter",
                        block.line_info.unwrap_or_default(),
                      )
                        .into(),
                    )
                  }
                }
                (Some(_), Some(_)) => Err(
                  (
                    "run built-in function requires either a rate or fps parameter",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
                _ => Ok(None),
              }?;

              let iterations = if let Some(iterations) = iterations_param {
                if let Value::Number(Number::Integer(n)) = &iterations.value {
                  let iterations = u64::try_from(*n).map_err(|_| (
                        "run built-in function requires an integer number in range of i64 iterations parameter",
                        block.line_info.unwrap_or_default(),
                    ).into())?;
                  Ok(Some(iterations))
                } else {
                  Err(
                    (
                      "run built-in function requires an integer number iterations parameter",
                      block.line_info.unwrap_or_default(),
                    )
                      .into(),
                  )
                }
              } else {
                Ok(None)
              }?;

              fn sleep_and_update(next: &mut Instant, now: Instant, tick: f64) {
                let real_sleep_time = *next - now;

                if real_sleep_time <= Duration::from_secs_f64(0.0) {
                  *next = now + Duration::from_secs_f64(tick);
                } else {
                  sleep(real_sleep_time.as_secs_f64());
                  *next += Duration::from_secs_f64(tick);
                }
              }

              let mesh = get_mesh(&mesh_id, find_mesh, e, block)?;

              use std::time::{Duration, Instant};

              let mut now = Instant::now();
              let mut next = now + Duration::from_secs_f64(tick.unwrap_or(0.0));
              let mut iteration = 0u64;

              loop {
                if let Some(tick) = tick {
                  if !mesh.tick() {
                    break;
                  }

                  now = Instant::now();
                  sleep_and_update(&mut next, now, tick);
                } else {
                  if !mesh.tick() {
                    break;
                  }
                }

                iteration += 1;
                if let Some(max_iterations) = iterations {
                  if iteration >= max_iterations {
                    mesh.terminate();
                    break;
                  }
                }
              }

              Ok(())
            } else {
              Err(
                (
                  "run built-in function requires a parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "color" => process_color_built_in_function(func, block.line_info.unwrap_or_default(), e),
          "i2" => {
            process_vector_built_in_ints_block::<2>(func, block.line_info.unwrap_or_default(), e)
          }
          "i3" => {
            process_vector_built_in_ints_block::<3>(func, block.line_info.unwrap_or_default(), e)
          }
          "i4" => {
            process_vector_built_in_ints_block::<4>(func, block.line_info.unwrap_or_default(), e)
          }
          "i8" => {
            process_vector_built_in_ints_block::<8>(func, block.line_info.unwrap_or_default(), e)
          }
          "i16" => {
            process_vector_built_in_ints_block::<16>(func, block.line_info.unwrap_or_default(), e)
          }
          "f2" => {
            process_vector_built_in_floats_block::<2>(func, block.line_info.unwrap_or_default(), e)
          }
          "f3" => {
            process_vector_built_in_floats_block::<3>(func, block.line_info.unwrap_or_default(), e)
          }
          "f4" => {
            process_vector_built_in_floats_block::<4>(func, block.line_info.unwrap_or_default(), e)
          }
          "macro" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "macro built-in function requires a Name parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              let args = param_helper.get_param_by_name_or_index("Args", 1).ok_or(
                (
                  "macro built-in function requires an Args parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              let shards = param_helper.get_param_by_name_or_index("Shards", 2).ok_or(
                (
                  "macro built-in function requires a Shards parameter",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )?;

              match (&name.value, &args.value, &shards.value) {
                (Value::Identifier(name), Value::Seq(args), Value::Shards(shards)) => {
                  let args_ptr = args as *const _;
                  let shards_ptr = shards as *const _;
                  e.macro_groups.insert(
                    name.clone(),
                    ShardsGroup {
                      args: args_ptr,
                      shards: shards_ptr,
                    },
                  );
                  Ok(())
                }
                _ => Err(
                  (
                    "macro built-in function requires a Name, Args and Shards parameters",
                    block.line_info.unwrap_or_default(),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "macro built-in function requires parameters",
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              )
            }
          }
          "type" => {
            let info = process_type(func, block.line_info.unwrap_or_default(), e)?;
            add_const_shard2(*info.as_ref(), block.line_info.unwrap_or_default(), e)
          }
          "ast" => {
            let info = process_ast(func, block.line_info.unwrap_or_default(), e)?;
            add_const_shard2(*info.as_ref(), block.line_info.unwrap_or_default(), e)
          }
          unknown => {
            match (
              // Notice, By precedence!
              find_defined(&func.name, e),
              process_shards(func, unknown, block, e)?,
              process_macro(func, unknown, block.line_info.unwrap_or_default(), e)?,
              find_extension(&func.name, e),
            ) {
              (None, None, None, Some(extension)) => {
                let shard =
                  extension.process_to_shard(func, block.line_info.unwrap_or_default())?;
                e.shards.push(shard);
                Ok(())
              }
              (None, Some(mut shards_env), _, _) => {
                // shards
                for shard in shards_env.shards.drain(..) {
                  e.shards.push(shard);
                }
                Ok(())
              }
              (None, None, Some(ast_json), _) => {
                // macro
                let ast_json: &str = ast_json.as_ref().try_into().map_err(|_| {
                  (
                    "macro built-in function Shards should return a Json string",
                    block.line_info.unwrap_or_default(),
                  )
                    .into()
                })?;

                // in this case we expect the ast to be a sequence of statements
                let decoded_json: Sequence = serde_json::from_str(ast_json).map_err(|e| {
                  (
                    format!(
                      "macro built-in function Shards should return a valid Json string: {}",
                      e
                    ),
                    block.line_info.unwrap_or_default(),
                  )
                    .into()
                })?;

                // which we directly evaluate
                for stmt in &decoded_json.statements {
                  eval_statement(stmt, e)?;
                }

                Ok(())
              }
              (Some(value), _, _, _) => {
                // defined
                let replacement = unsafe { &*value };
                match replacement {
                  Value::None
                  | Value::Identifier(_)
                  | Value::Boolean(_)
                  | Value::Enum(_, _)
                  | Value::Number(_)
                  | Value::String(_)
                  | Value::Bytes(_)
                  | Value::Int2(_)
                  | Value::Int3(_)
                  | Value::Int4(_)
                  | Value::Int8(_)
                  | Value::Int16(_)
                  | Value::Float2(_)
                  | Value::Float3(_)
                  | Value::Float4(_)
                  | Value::Seq(_)
                  | Value::Func(_)
                  | Value::TakeTable(_, _)
                  | Value::TakeSeq(_, _)
                  | Value::Table(_) => {
                    add_const_shard(replacement, block.line_info.unwrap_or_default(), e)?
                  }
                  Value::Shards(seq) => {
                    // purely include the ast of the sequence
                    for stmt in &seq.statements {
                      eval_statement(stmt, e)?;
                    }
                  }
                  Value::EvalExpr(seq) => {
                    let value = eval_eval_expr(&seq, e)?;
                    add_const_shard2(value.0 .0, block.line_info.unwrap_or_default(), e)?
                  }
                  Value::Expr(seq) => eval_expr(seq, e, block, start_idx)?,
                  Value::Shard(shard) => add_shard(shard, block.line_info.unwrap_or_default(), e)?,
                }
                Ok(())
              }
              _ => Err(
                (
                  format!("unknown built-in function: {}", unknown),
                  block.line_info.unwrap_or_default(),
                )
                  .into(),
              ),
            }
          }
        }
      }
    }?;
  }
  Ok(())
}

fn eval_expr(
  seq: &Sequence,
  e: &mut EvalEnv,
  block: &Block,
  start_idx: usize,
) -> Result<(), ShardsError> {
  let mut sub_env = eval_sequence(&seq, Some(e))?;
  Ok(if !sub_env.shards.is_empty() {
    // create a temporary variable to hold the result of the expression
    let tmp_name = nanoid!(16);
    // ensure name starts with a letter
    let tmp_name = format!("t{}", tmp_name);
    add_assignment_shard_no_suffix(
      "Ref",
      &tmp_name,
      block.line_info.unwrap_or_default(),
      &mut sub_env,
    )
    .map_err(|e| (format!("{:?}", e), block.line_info.unwrap_or_default()).into())?;
    // wrap into a Sub Shard
    finalize_env(&mut sub_env)?;
    let sub = make_sub_shard(
      sub_env.shards.drain(..).collect(),
      block.line_info.unwrap_or_default(),
    )?;
    // add this sub shard before the start of this pipeline!
    e.shards.insert(start_idx, sub);
    // now add a get shard to get the temporary at the end of the pipeline
    add_get_shard_no_suffix(&tmp_name, block.line_info.unwrap_or_default(), e)?;
  })
}

fn add_assignment_shard(
  shard_name: &str,
  name: &RcStrWrapper,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = ShardRef::create(shard_name).unwrap();
  let shard = AutoShardRef(shard);

  let (assigned, suffix) = match (find_replacement(name, e), find_current_suffix(e)) {
    (Some(Value::Identifier(name)), _) => {
      let name = Var::ephemeral_string(name);
      shard
        .0
        .set_parameter(0, name)
        .map_err(|e| (e, line_info).into())?;
      (false, None)
    }
    (None, Some(suffix)) => {
      let name = format!("{}{}", name, suffix);
      let name = Var::ephemeral_string(&name);
      shard
        .0
        .set_parameter(0, name)
        .map_err(|e| (e, line_info).into())?;
      (true, Some(suffix.clone()))
    }
    (None, None) => {
      let name = Var::ephemeral_string(name);
      shard
        .0
        .set_parameter(0, name)
        .map_err(|e| (e, line_info).into())?;
      (false, None)
    }
    _ => unreachable!(), // Read should prevent this...
  };

  if assigned {
    e.suffix_assigned.insert(name.clone(), suffix.unwrap());
  }
  shard.0.set_line_info((
    line_info.line.try_into().unwrap(),
    line_info.column.try_into().unwrap(),
  ));
  e.shards.push(shard);
  Ok(())
}

fn add_assignment_shard_no_suffix(
  shard_name: &str,
  name: &str,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = ShardRef::create(shard_name).unwrap();
  let shard = AutoShardRef(shard);
  let name = Var::ephemeral_string(name);
  shard
    .0
    .set_parameter(0, name)
    .map_err(|e| (e, line_info).into())?;
  shard.0.set_line_info((
    line_info.line.try_into().unwrap(),
    line_info.column.try_into().unwrap(),
  ));
  e.shards.push(shard);
  Ok(())
}

fn eval_assignment(assignment: &Assignment, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let (pipe, op, name) = match assignment {
    Assignment::AssignRef(pipe, name) => (pipe, "Ref", name),
    Assignment::AssignSet(pipe, name) => (pipe, "Set", name),
    Assignment::AssignUpd(pipe, name) => (pipe, "Update", name),
    Assignment::AssignPush(pipe, name) => (pipe, "Push", name),
  };
  eval_pipeline(pipe, e)?;
  // find last added shard
  let last = e.shards.last().unwrap();
  let line_info = last.0.get_line_info();
  let line_info = LineInfo {
    line: line_info.0.try_into().unwrap(),
    column: line_info.1.try_into().unwrap(),
  };
  add_assignment_shard(op, &name, line_info, e)
    .map_err(|e| (format!("{:?}", e), line_info).into())?;
  Ok(())
}

pub(crate) fn eval_statement(stmt: &Statement, e: &mut EvalEnv) -> Result<(), ShardsError> {
  match stmt {
    Statement::Assignment(a) => eval_assignment(a, e),
    Statement::Pipeline(p) => eval_pipeline(p, e),
  }
}

pub fn eval(
  seq: &Sequence,
  name: &str,
  defines: HashMap<String, String>,
) -> Result<Wire, ShardsError> {
  profiling::scope!("eval", name);

  let mut parent = EvalEnv::default();
  // add defines
  let defines: Vec<(RcStrWrapper, Value)> = defines
    .iter()
    .map(|(k, v)| (k.as_str().into(), Value::String(v.as_str().into())))
    .collect::<Vec<_>>();
  for (name, value) in &defines {
    parent.definitions.insert(name.clone(), value);
  }

  let mut env = eval_sequence(seq, Some(&mut parent))?;

  let wire = Wire::default();
  wire.set_name(name);
  finalize_env(&mut env)?;
  for shard in env.shards.drain(..) {
    wire.add_shard(shard.0);
  }
  Ok(wire)
}

/// Register an extension which is a type that implements the `ShardsExtension` trait to the environment.
#[allow(dead_code)]
pub fn register_extension<T: ShardsExtension>(ext: Box<dyn ShardsExtension>, env: &mut EvalEnv) {
  env.extensions.insert(ext.name().into(), ext);
}
