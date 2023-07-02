use crate::ast::*;
use crate::ParamHelper;
use crate::RcStrWrapper;

use core::convert::TryInto;

use core::slice;
use nanoid::nanoid;

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

struct SShardRef(pub(crate) ShardRef);

impl Drop for SShardRef {
  fn drop(&mut self) {
    self.0.destroy();
  }
}

struct ShardsGroup {
  args: *const Vec<Value>,
  shards: *const Sequence,
}

#[repr(C)]
pub struct Setting {
  allow_unsafe: bool,
  allow_custom_stack_sizes: bool,
  allow_only_pure_wires: bool,
}

pub struct EvalEnv {
  parent: Option<*const EvalEnv>,

  shards: Vec<SShardRef>,

  deferred_wires: HashMap<RcStrWrapper, (Wire, *const Vec<Param>, LineInfo)>,
  finalized_wires: HashMap<RcStrWrapper, Wire>,

  shards_groups: HashMap<RcStrWrapper, ShardsGroup>,
  definitions: HashMap<RcStrWrapper, *const Value>,

  // used during @shards evaluations, to replace [x y z] arguments
  replacements: HashMap<RcStrWrapper, *const Value>,

  // used during @shards evaluation
  suffix: Option<RcStrWrapper>,
  suffix_assigned: HashSet<RcStrWrapper>,

  // Shards and functions that are forbidden to be used
  pub forbidden_funcs: HashSet<RcStrWrapper>,
  pub settings: Vec<Setting>,

  meshes: HashMap<RcStrWrapper, Mesh>,
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
      definitions: HashMap::new(),
      replacements: HashMap::new(),
      suffix: None,
      suffix_assigned: HashSet::new(),
      forbidden_funcs: HashSet::new(),
      settings: Vec::new(),
      meshes: HashMap::new(),
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

fn handle_color_built_in_function(
  func: &Function,
  line_info: LineInfo,
) -> Result<Var, ShardsError> {
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

fn find_wire<'a>(name: &'a RcStrWrapper, env: &'a EvalEnv) -> Option<(&'a Wire, bool)> {
  if let Some(wire) = env.finalized_wires.get(name) {
    Some((wire, true))
  } else if let Some(wire) = env.deferred_wires.get(name) {
    Some((&wire.0, false))
  } else if let Some(parent) = env.parent {
    find_wire(name, unsafe { &mut *(parent as *mut EvalEnv) })
  } else {
    None
  }
}

fn finalize_wire(
  wire: Wire,
  name: &RcStrWrapper,
  params: *const Vec<Param>,
  line_info: LineInfo,
  env: &mut EvalEnv,
) -> Result<Wire, ShardsError> {
  wire.set_name(&name.0);

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

  if let Some(_) = env.settings.iter().find(|&s| s.allow_only_pure_wires) {
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

  if let Some(_) = env.settings.iter().find(|&s| s.allow_unsafe) {
    let unsafe_ = param_helper
      .get_param_by_name_or_index("Unsafe", 4)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Unsafe parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;
    wire.set_unsafe(unsafe_);
  }

  if let Some(_) = env.settings.iter().find(|&s| s.allow_custom_stack_sizes) {
    let stack_size = param_helper
      .get_param_by_name_or_index("StackSize", 5)
      .map(|param| match &param.value {
        Value::Number(n) => match n {
          Number::Integer(n) => Ok(*n),
          _ => Err(("StackSize parameter must be an integer", line_info).into()),
        },
        _ => Err(("StackSize parameter must be an integer", line_info).into()),
      })
      .unwrap_or(Ok(0))?;
    // ensure stack size is a multiple of 4 and minimum 1024 bytes
    let stack_size = if stack_size < 1024 {
      1024
    } else if stack_size % 4 != 0 {
      stack_size + 4 - (stack_size % 4)
    } else {
      stack_size
    };
    wire.set_stack_size(stack_size as usize);
  }

  Ok(wire)
}

fn finalize_env(env: &mut EvalEnv) -> Result<(), ShardsError> {
  for (name, (wire, params, line_info)) in env.deferred_wires.drain().collect::<Vec<_>>() {
    let wire = finalize_wire(wire, &name, params, line_info, env)?;
    env.finalized_wires.insert(name, wire);
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

fn eval_sequence(seq: &Sequence, parent: Option<&mut EvalEnv>) -> Result<EvalEnv, ShardsError> {
  let mut sub_env = EvalEnv::default();
  // inherit previous state for certain things
  if let Some(parent) = parent {
    sub_env.parent = Some(parent as *mut EvalEnv);
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
    if let Some(suffix) = &env.suffix {
      if env.suffix_assigned.contains(name) {
        return Some(suffix);
      }
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
      if let Some((wire, _finalized)) = find_wire(name, e) {
        let wire: Var = wire.0.into();
        Ok(SVar::NotCloned(wire))
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
        table.insert(*key_ref, value_ref);
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
          .map_err(|_| ("Failed to set parameter", line_info).into())?;
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
          .map_err(|_| ("Failed to set parameter", line_info).into())?;
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
          .map_err(|_| ("Failed to set parameter", line_info).into())?;
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
      "color" => Ok(SVar::NotCloned(handle_color_built_in_function(
        func, line_info,
      )?)),
      _ => {
        if let Some(defined_value) = find_defined(&func.name, e) {
          let replacement = unsafe { &*defined_value };
          as_var(replacement, line_info, shard, e)
        } else {
          Err((format!("Undefined function {}", func.name), line_info).into())
        }
      }
    },
  }
}

fn add_shard(shard: &Function, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
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
  let s = SShardRef(s);
  let mut idx = 0i32;
  let mut as_idx = true;
  let info = s.0.parameters();
  if let Some(ref params) = shard.params {
    for param in params {
      let value = as_var(&param.value, line_info, Some(s.0), e).map_err(|e| e)?;
      if let Some(ref name) = param.name {
        as_idx = false;
        let mut found = false;
        for (i, info) in info.iter().enumerate() {
          let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() };
          if param_name == name.as_str() {
            if let Err(_) = s
              .0
              .set_parameter(i.try_into().expect("Too many parameters"), *value.as_ref())
            {
              return Err(("Failed to set parameter", line_info).into());
            }
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
          return Err(("Named parameter after unnamed parameter", line_info).into());
        }
        if let Err(_) = s.0.set_parameter(idx, *value.as_ref()) {
          return Err(("Failed to set parameter", line_info).into());
        }
      }
      idx += 1;
    }
  }
  e.shards.push(s);
  Ok(())
}

fn add_const_shard2(value: Var, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Const").unwrap();
  let shard = SShardRef(shard);
  shard
    .0
    .set_parameter(0, value)
    .map_err(|_| ("Failed to set parameter", line_info).into())?;
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
        let shard = ShardRef::create("Const").unwrap();
        let shard = SShardRef(shard);
        let value = as_var(&replacement.clone(), line_info, Some(shard.0), e)?;
        shard
          .0
          .set_parameter(0, *value.as_ref())
          .map_err(|_| ("Failed to set parameter", line_info).into())?;
        shard
      } else {
        let shard = ShardRef::create("Get").unwrap();
        let shard = SShardRef(shard);
        let value = as_var(value, line_info, Some(shard.0), e)?;
        shard
          .0
          .set_parameter(0, *value.as_ref())
          .map_err(|_| ("Failed to set parameter", line_info).into())?;
        shard
      }
    }
    _ => {
      let shard = ShardRef::create("Const").unwrap();
      let shard = SShardRef(shard);
      let value = as_var(value, line_info, Some(shard.0), e)?;
      shard
        .0
        .set_parameter(0, *value.as_ref())
        .map_err(|_| ("Failed to set parameter", line_info).into())?;
      shard
    }
  };
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  e.shards.push(shard);
  Ok(())
}

fn make_sub_shard(shards: Vec<SShardRef>, line_info: LineInfo) -> Result<SShardRef, ShardsError> {
  let shard = ShardRef::create("Sub").unwrap();
  let shard = SShardRef(shard);
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
    .map_err(|_| ("Failed to set parameter", line_info).into())?;
  destroyVar(&mut seq.0);
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  Ok(shard)
}

fn add_take_shard(target: &Var, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Take").unwrap();
  let shard = SShardRef(shard);
  shard
    .0
    .set_parameter(0, *target)
    .map_err(|_| ("Failed to set parameter", line_info).into())?;
  shard.0.set_line_info((
    line_info.line.try_into().unwrap(),
    line_info.column.try_into().unwrap(),
  ));
  e.shards.push(shard);
  Ok(())
}

fn add_get_shard(name: &RcStrWrapper, line: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Get").unwrap();
  let shard = SShardRef(shard);
  if let Some(suffix) = find_suffix(name, e) {
    let name = format!("{}{}", name, suffix);
    let name = Var::ephemeral_string(&name);
    shard
      .0
      .set_parameter(0, name)
      .map_err(|_| ("Failed to set parameter", line).into())?;
  } else {
    let name = Var::ephemeral_string(name.as_str());
    shard
      .0
      .set_parameter(0, name)
      .map_err(|_| ("Failed to set parameter", line).into())?;
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
  let shard = SShardRef(shard);
  let name = Var::ephemeral_string(name);
  shard
    .0
    .set_parameter(0, name)
    .map_err(|_| ("Failed to set parameter", line).into())?;
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
        block.line_info,
      )
        .into()
    }),
    _ => Err(
      (
        "run built-in function requires a mesh parameter",
        block.line_info,
      )
        .into(),
    ),
  }
}

fn eval_pipeline(pipeline: &Pipeline, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let start_idx = e.shards.len();
  for block in &pipeline.blocks {
    let _ = match &block.content {
      BlockContent::Shard(shard) => add_shard(shard, block.line_info, e),
      BlockContent::Shards(seq) => {
        let mut sub_env = eval_sequence(&seq, Some(e))?;

        // if we have a sub env, we need to finalize it
        if !sub_env.shards.is_empty() {
          // sub shards leaks assignments to the parent env
          for name in sub_env.suffix_assigned.drain() {
            e.suffix_assigned.insert(name);
          }
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(sub_env.shards.drain(..).collect(), block.line_info)?;
          e.shards.push(sub);
        }

        Ok(())
      }
      BlockContent::Const(value) => add_const_shard(value, block.line_info, e),
      BlockContent::TakeTable(name, path) => {
        create_take_table_chain(name, path, block.line_info, e)
      }
      BlockContent::TakeSeq(name, path) => create_take_seq_chain(name, path, block.line_info, e),
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
          return Err((format!("Forbidden function {}", func.name), block.line_info).into());
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
                  block.line_info,
                )
                  .into(),
              )?;

              let value = param_helper.get_param_by_name_or_index("Value", 1).ok_or(
                (
                  "define built-in function requires Value parameter",
                  block.line_info,
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
                  e.definitions.insert(name.clone(), &value.value);
                  Ok(())
                }
                _ => Err(
                  (
                    "define built-in function requires Name parameter to be an identifier",
                    block.line_info,
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "const built-in function requires proper parameters",
                  block.line_info,
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
                  block.line_info,
                )
                  .into(),
              )?;

              match &name.value {
                Value::Identifier(name) => {
                  let params_ptr = func.params.as_ref().unwrap() as *const Vec<Param>;
                  e.deferred_wires
                    .insert(name.clone(), (Wire::default(), params_ptr, block.line_info));
                  Ok(())
                }
                _ => Err(
                  (
                    "wire built-in function requires a string parameter",
                    block.line_info,
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "wire built-in function requires proper parameters",
                  block.line_info,
                )
                  .into(),
              )
            }
          }
          "shards" => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "shards built-in function requires a Name parameter",
                  block.line_info,
                )
                  .into(),
              )?;

              let args = param_helper.get_param_by_name_or_index("Args", 1).ok_or(
                (
                  "shards built-in function requires an Args parameter",
                  block.line_info,
                )
                  .into(),
              )?;

              let shards = param_helper.get_param_by_name_or_index("Shards", 2).ok_or(
                (
                  "shards built-in function requires a Shards parameter",
                  block.line_info,
                )
                  .into(),
              )?;

              match (&name.value, &args.value, &shards.value) {
                (Value::Identifier(name), Value::Seq(args), Value::Shards(shards)) => {
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
                    block.line_info,
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "shards built-in function requires 3 parameters",
                  block.line_info,
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
                  block.line_info,
                )
                  .into(),
              )?;

              match &name.value {
                Value::Identifier(name) => {
                  e.meshes.insert(name.clone(), Mesh::default());
                  Ok(())
                }
                _ => Err(
                  (
                    "mesh built-in function requires an identifier parameter",
                    block.line_info,
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "mesh built-in function requires a parameter",
                  block.line_info,
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
                  block.line_info,
                )
                  .into(),
              )?;
              let wire_id = param_helper.get_param_by_name_or_index("Wire", 1).ok_or(
                (
                  "schedule built-in function requires a wire parameter",
                  block.line_info,
                )
                  .into(),
              )?;

              // wire is likely lazy so we need to evaluate it
              let wire = match &wire_id.value {
                // can be only identifier or string
                Value::Identifier(name) => {
                  if let Some((wire, finalized)) = find_wire(name, e) {
                    if !finalized {
                      // wire is not finalized, we need to finalize it now
                      let wire = e.deferred_wires.remove(name).unwrap();
                      let wire = finalize_wire(wire.0, name, wire.1, wire.2, e)?;
                      e.finalized_wires.insert(name.clone(), wire);
                      let wire = e.finalized_wires.get(name).unwrap();
                      Ok(wire.0)
                    } else {
                      Ok(wire.0)
                    }
                  } else {
                    Err(
                      (
                        "schedule built-in function requires a valid wire parameter",
                        block.line_info,
                      )
                        .into(),
                    )
                  }
                }
                _ => Err(
                  (
                    "schedule built-in function requires a wire parameter",
                    block.line_info,
                  )
                    .into(),
                ),
              }?;

              let mesh = get_mesh(&mesh_id, find_mesh, e, block)?;

              mesh.schedule(wire);

              Ok(())
            } else {
              Err(
                (
                  "schedule built-in function requires 2 parameters",
                  block.line_info,
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
                  block.line_info,
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
                        block.line_info,
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
                            block.line_info,
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
                        block.line_info,
                      )
                        .into(),
                    )
                  }
                }
                (Some(_), Some(_)) => Err(
                  (
                    "run built-in function requires either a rate or fps parameter",
                    block.line_info,
                  )
                    .into(),
                ),
                _ => Ok(None),
              }?;

              let iterations = if let Some(iterations) = iterations_param {
                if let Value::Number(Number::Integer(n)) = &iterations.value {
                  let iterations = u64::try_from(*n).map_err(|_| (
                        "run built-in function requires an integer number in range of i64 iterations parameter",
                        block.line_info,
                    ).into())?;
                  Ok(Some(iterations))
                } else {
                  Err(
                    (
                      "run built-in function requires an integer number iterations parameter",
                      block.line_info,
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
                    break;
                  }
                }
              }

              Ok(())
            } else {
              Err(
                (
                  "run built-in function requires a parameter",
                  block.line_info,
                )
                  .into(),
              )
            }
          }
          "color" => {
            let value = handle_color_built_in_function(func, block.line_info)?;
            add_const_shard2(value, block.line_info, e)
          }
          unknown => {
            let mut shards_env = {
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
                      block.line_info,
                    )
                      .into(),
                  );
                }

                let mut sub_env = EvalEnv::default();
                sub_env.parent = Some(e as *const EvalEnv);

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
                          block.line_info,
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
                        block.line_info,
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

                Some(sub_env)
              } else {
                None
              }
            };

            let defined_value = {
              if let Some(val) = find_defined(&func.name, e) {
                Some(val)
              } else {
                None
              }
            };

            if let Some(shards_env) = &mut shards_env {
              for shard in shards_env.shards.drain(..) {
                e.shards.push(shard);
              }
              Ok(())
            } else if let Some(defined_value) = defined_value {
              let replacement = unsafe { &*defined_value };
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
                | Value::Table(_) => add_const_shard(replacement, block.line_info, e)?,
                Value::Shards(seq) => {
                  // purely include the ast of the sequence
                  for stmt in &seq.statements {
                    eval_statement(stmt, e)?;
                  }
                }
                Value::EvalExpr(seq) => {
                  let value = eval_eval_expr(&seq, e)?;
                  add_const_shard2(value.0 .0, block.line_info, e)?
                }
                Value::Expr(seq) => eval_expr(seq, e, block, start_idx)?,
                _ => {
                  return Err((format!("Invalid value: {:?}", replacement), block.line_info).into())
                }
              }
              Ok(())
            } else {
              Err((format!("Unknown function: {}", func.name), block.line_info).into())
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
    add_assignment_shard_no_suffix("Ref", &tmp_name, block.line_info, &mut sub_env)
      .map_err(|_| ("Failed to set parameter", block.line_info).into())?;
    // wrap into a Sub Shard
    finalize_env(&mut sub_env)?;
    let sub = make_sub_shard(sub_env.shards.drain(..).collect(), block.line_info)?;
    // add this sub shard before the start of this pipeline!
    e.shards.insert(start_idx, sub);
    // now add a get shard to get the temporary at the end of the pipeline
    add_get_shard_no_suffix(&tmp_name, block.line_info, e)?;
  })
}

fn add_assignment_shard(
  shard_name: &str,
  name: &RcStrWrapper,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = ShardRef::create(shard_name).unwrap();
  let shard = SShardRef(shard);
  let assigned = if let Some(suffix) = find_current_suffix(e) {
    let name = format!("{}{}", name, suffix);
    let name = Var::ephemeral_string(&name);
    shard
      .0
      .set_parameter(0, name)
      .map_err(|_| ("Failed to set parameter", line_info).into())?;
    true
  } else {
    let name = Var::ephemeral_string(name);
    shard
      .0
      .set_parameter(0, name)
      .map_err(|_| ("Failed to set parameter", line_info).into())?;
    false
  };
  if assigned {
    e.suffix_assigned.insert(name.clone());
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
  let shard = SShardRef(shard);
  let name = Var::ephemeral_string(name);
  shard
    .0
    .set_parameter(0, name)
    .map_err(|_| ("Failed to set parameter", line_info).into())?;
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
    .map_err(|_| ("Failed to set parameter", line_info).into())?;
  Ok(())
}

fn eval_statement(stmt: &Statement, e: &mut EvalEnv) -> Result<(), ShardsError> {
  match stmt {
    Statement::Assignment(a) => eval_assignment(a, e),
    Statement::Pipeline(p) => eval_pipeline(p, e),
  }
}

pub fn eval(seq: &Sequence, name: &str) -> Result<Wire, ShardsError> {
  profiling::scope!("eval", name);

  let mut env = eval_sequence(seq, None)?;
  let wire = Wire::default();
  wire.set_name(name);
  finalize_env(&mut env)?;
  for shard in env.shards.drain(..) {
    wire.add_shard(shard.0);
  }
  Ok(wire)
}
