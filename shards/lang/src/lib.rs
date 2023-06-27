extern crate pest;
#[macro_use]
extern crate pest_derive;

mod print;
mod read;
mod types;

use crate::read::*;
use crate::types::*;
use core::convert::TryInto;
use core::slice;
use nanoid::nanoid;
use pest::Parser;
use print::print_ast;
use shards::core::cloneVar;
use shards::core::destroyVar;
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

struct EvalEnv {
  shards: Vec<SShardRef>,
  previous: Option<ShardRef>,

  deferred_wires: HashMap<String, (Wire, Vec<Param>, LineInfo)>, // ideally we should not clone Vec<Param> (Params)

  shards_groups: HashMap<String, (Vec<Value>, Sequence)>, // more cloning we don't need...

  // used during @shards evaluation, but also by @const
  replacements: HashMap<String, Value>, // more cloning we don't need...

  // used during @shards evaluation
  suffix: Option<String>,
  suffix_assigned: HashSet<String>,
}

impl Drop for EvalEnv {
  fn drop(&mut self) {
    // keep this because we want borrow checker warnings
  }
}

impl Default for EvalEnv {
  fn default() -> Self {
    EvalEnv {
      shards: Vec::new(),
      previous: None,
      deferred_wires: HashMap::new(),
      shards_groups: HashMap::new(),
      replacements: HashMap::new(),
      suffix: None,
      suffix_assigned: HashSet::new(),
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

fn finalize_env(env: &mut EvalEnv) -> Result<(), ShardsError> {
  for (name, (wire, params, line_info)) in env.deferred_wires.drain().collect::<Vec<_>>() {
    wire.set_name(&name);
    let mut idx = 0;
    let mut as_idx = true;
    for param in params {
      if let Some(name) = param.name {
        as_idx = false;
        if name == "Looped" {
          wire.set_looped({
            match param.value {
              Value::Boolean(b) => b,
              _ => return Err(("Looped parameter must be a boolean", line_info).into()),
            }
          })
        } else if name == "Shards" {
          let mut sub_env = match param.value {
            Value::Shards(seq) => eval_sequence(&seq, None, Some(env))?,
            _ => return Err(("Shards parameter must be a sequence", line_info).into()),
          };
          finalize_env(&mut sub_env)?;
          for shard in sub_env.shards.drain(..) {
            wire.add_shard(shard.0);
          }
        } else if name == "Name" {
        } else {
          return Err(("Unknown parameter", line_info).into());
        }
      } else {
        if !as_idx {
          return Err(("Named parameter after unnamed parameter", line_info).into());
        }

        if idx == 0 {
        } else if idx == 1 {
          let mut sub_env = match param.value {
            Value::Shards(seq) => eval_sequence(&seq, None, Some(env))?,
            _ => return Err(("Shards parameter must be a sequence", line_info).into()),
          };
          finalize_env(&mut sub_env)?;
          for shard in sub_env.shards.drain(..) {
            wire.add_shard(shard.0);
          }
        } else if idx == 2 {
          wire.set_looped({
            match param.value {
              Value::Boolean(b) => b,
              _ => return Err(("Looped parameter must be a boolean", line_info).into()),
            }
          })
        } else {
          return Err(("Unknown parameter", line_info).into());
        }
      }
      idx += 1;
    }
  }
  Ok(())
}

fn eval_eval_expr(seq: &Sequence, env: &mut EvalEnv) -> Result<(ClonedVar, LineInfo), ShardsError> {
  let mut sub_env = eval_sequence(seq, None, Some(env))?;
  if !sub_env.shards.is_empty() {
    let line_info = sub_env.shards[0].0.get_line_info();
    // create an ephemeral wire, execute and grab result
    let wire = Wire::default();
    wire.set_name("eval-ephemeral");
    finalize_env(&mut sub_env)?;
    for shard in sub_env.shards.drain(..) {
      wire.add_shard(shard.0);
    }
    let mesh = Mesh::default();
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
      Ok((
        ClonedVar(unsafe { *info.finalOutput }),
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

fn eval_sequence(
  seq: &Sequence,
  previous: Option<ShardRef>,
  parent: Option<&mut EvalEnv>,
) -> Result<EvalEnv, ShardsError> {
  let mut sub_env = EvalEnv::default();
  // inherit previous state for certain things
  sub_env.previous = previous;
  if let Some(parent) = parent {
    sub_env.shards_groups = parent.shards_groups.clone();
    sub_env.replacements = parent.replacements.clone();
    sub_env.suffix = parent.suffix.clone();
    sub_env.suffix_assigned = parent.suffix_assigned.clone();
  }
  for stmt in &seq.statements {
    eval_statement(stmt, &mut sub_env)?;
  }
  Ok(sub_env)
}

fn create_take_table_chain(
  var_name: &String,
  path: &Vec<String>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(&var_name, true, line, e);
  for path_part in path {
    let s = Var::ephemeral_string(&path_part);
    add_take_shard(&s, e);
  }
  Ok(())
}

fn create_take_seq_chain(
  var_name: &String,
  path: &Vec<u32>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(&var_name, true, line, e);
  for path_part in path {
    let idx = (*path_part).try_into().unwrap(); // read should have caught this
    add_take_shard(&idx, e);
  }
  Ok(())
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
      if let Some(wire) = e.deferred_wires.get(name) {
        let wire: Var = wire.0 .0.into();
        Ok(SVar::NotCloned(wire))
      } else if let Some(replacement) = e.replacements.get(name) {
        as_var(&replacement.clone(), line_info, shard, e) // cloned to make borrow checker happy...
      } else {
        match (&e.suffix, e.suffix_assigned.contains(name)) {
          (Some(suffix), true) => {
            let name = format!("{}{}", name, suffix);
            let mut s = Var::ephemeral_string(name.as_str());
            s.valueType = SHType_ContextVar;
            Ok(SVar::Cloned(s.into()))
          }
          _ => {
            let mut s = Var::ephemeral_string(name.as_str());
            s.valueType = SHType_ContextVar;
            Ok(SVar::NotCloned(s))
          }
        }
      }
    }
    Value::Enum(prefix, value) => {
      let id = findEnumId(&prefix);
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
      let s = Var::ephemeral_string(s);
      Ok(SVar::NotCloned(s))
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
      let mut sub_env = eval_sequence(&seq, shard, Some(e))?;
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
      let mut sub_env = eval_sequence(&seq, e.previous, Some(e))?;
      if !sub_env.shards.is_empty() {
        // create a temporary variable to hold the result of the expression
        let tmp_name = nanoid!(16);
        // ensure name starts with a letter
        let tmp_name = format!("t{}", tmp_name);
        add_assignment_shard("Ref", &tmp_name, false, &mut sub_env);
        // wrap into a Sub Shard
        let line_info = sub_env.shards[0].0.get_line_info();
        finalize_env(&mut sub_env)?;
        let sub = make_sub_shard(
          sub_env.shards.drain(..).collect(),
          LineInfo {
            line: line_info.0 as usize,
            column: line_info.1 as usize,
          },
        )?;
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
        add_assignment_shard("Ref", &tmp_name, false, &mut sub_env);
        // wrap into a Sub Shard
        finalize_env(&mut sub_env)?;
        let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
        // add this sub shard before the start of this pipeline!
        e.shards.insert(start_idx, sub);
        // now add a get shard to get the temporary at the end of the pipeline
        add_get_shard(&tmp_name, true, line_info, e);

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
        add_assignment_shard("Ref", &tmp_name, false, &mut sub_env);
        // wrap into a Sub Shard
        finalize_env(&mut sub_env)?;
        let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
        // add this sub shard before the start of this pipeline!
        e.shards.insert(start_idx, sub);
        // now add a get shard to get the temporary at the end of the pipeline
        add_get_shard(&tmp_name, true, line_info, e);

        let mut s = Var::ephemeral_string(tmp_name.as_str());
        s.valueType = SHType_ContextVar;
        Ok(SVar::Cloned(s.into()))
      } else {
        panic!("TakeTable should always return a shard")
      }
    }
    Value::Func(func) => Err(
      (
        format!(
          "Unsupported function as value or parameter {}",
          func.name.as_str()
        ),
        line_info,
      )
        .into(),
    ),
  }
}

fn add_shard(shard: &Function, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
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
          if param_name == name {
            s.0
              .set_parameter(i.try_into().expect("Too many parameters"), *value.as_ref());
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
        s.0.set_parameter(idx, *value.as_ref());
      }
      idx += 1;
    }
  }
  e.previous = Some(s.0);
  e.shards.push(s);
  Ok(())
}

fn add_const_shard2(value: Var, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Const").unwrap();
  let shard = SShardRef(shard);
  shard.0.set_parameter(0, value);
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  e.previous = Some(shard.0);
  e.shards.push(shard);
  Ok(())
}

fn add_const_shard(value: &Value, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = match value {
    Value::Identifier(name) => {
      // we might be a replacement though!
      if let Some(replacement) = e.replacements.get(name) {
        let shard = ShardRef::create("Const").unwrap();
        let shard = SShardRef(shard);
        let value = as_var(&replacement.clone(), line_info, Some(shard.0), e)?;
        shard.0.set_parameter(0, *value.as_ref());
        shard
      } else {
        let shard = ShardRef::create("Get").unwrap();
        let shard = SShardRef(shard);
        let value = as_var(value, line_info, Some(shard.0), e)?;
        shard.0.set_parameter(0, *value.as_ref());
        shard
      }
    }
    _ => {
      let shard = ShardRef::create("Const").unwrap();
      let shard = SShardRef(shard);
      let value = as_var(value, line_info, Some(shard.0), e)?;
      shard.0.set_parameter(0, *value.as_ref());
      shard
    }
  };
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  e.previous = Some(shard.0);
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
  shard.0.set_parameter(0, seq.0.into());
  destroyVar(&mut seq.0);
  shard.0.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  Ok(shard)
}

fn add_take_shard(target: &Var, e: &mut EvalEnv) {
  let shard = ShardRef::create("Take").unwrap();
  let shard = SShardRef(shard);
  shard.0.set_parameter(0, *target);
  if let Some(previous) = e.previous {
    shard.0.set_line_info(previous.get_line_info());
  }
  e.previous = Some(shard.0);
  e.shards.push(shard);
}

fn add_get_shard(name: &str, suffixed: bool, line: LineInfo, e: &mut EvalEnv) {
  let shard = ShardRef::create("Get").unwrap();
  let shard = SShardRef(shard);
  match (suffixed, &e.suffix, e.suffix_assigned.contains(name)) {
    (true, Some(suffix), true) => {
      let name = format!("{}{}", name, suffix);
      let name = Var::ephemeral_string(&name);
      shard.0.set_parameter(0, name);
    }
    _ => {
      let name = Var::ephemeral_string(name);
      shard.0.set_parameter(0, name);
    }
  }
  shard.0.set_line_info((
    line.line.try_into().unwrap(),
    line.column.try_into().unwrap(),
  ));
  e.previous = Some(shard.0);
  e.shards.push(shard);
}

fn eval_pipeline(pipeline: &Pipeline, e: &mut EvalEnv) -> Result<(), ShardsError> {
  // // a pipeline starts with no previous shard and an implicit nil
  // // this is useful for many shards such as Http.
  // // we must avoid doing this for the first shard in an env though
  // let mut added_nil = false;
  // if !e.shards.is_empty() {
  //   add_const_shard2(Var::default(), LineInfo::default(), e)?;
  //   added_nil = true;
  // }

  e.previous = None;
  let start_idx = e.shards.len();
  for block in &pipeline.blocks {
    let _ = match &block.content {
      BlockContent::Shard(shard) => add_shard(shard, block.line_info, e),
      BlockContent::Shards(seq) => {
        let mut sub_env = eval_sequence(&seq, e.previous, Some(e))?;

        // if we have a sub env, we need to finalize it
        if !sub_env.shards.is_empty() {
          // eval leaks assignments to the parent env
          for name in sub_env.suffix_assigned.drain() {
            e.suffix_assigned.insert(name);
          }

          // // remove the injected nil shard
          // sub_env.shards.remove(0);

          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(sub_env.shards.drain(..).collect(), block.line_info)?;
          e.shards.push(sub);
        }

        Ok(())
      }
      BlockContent::Const(value) => {
        // // remove the nil shard we injected if this is the first block
        // if added_nil && e.shards.len() == start_idx {
        //   e.shards.pop();
        // }

        add_const_shard(value, block.line_info, e)
      }
      BlockContent::TakeTable(name, path) => {
        create_take_table_chain(name, path, block.line_info, e)
      }
      BlockContent::TakeSeq(name, path) => create_take_seq_chain(name, path, block.line_info, e),
      BlockContent::EvalExpr(seq) => {
        let value = eval_eval_expr(&seq, e)?;
        add_const_shard2(value.0 .0, value.1, e)
      }
      BlockContent::Expr(seq) => {
        let mut sub_env = eval_sequence(&seq, e.previous, Some(e))?;
        if !sub_env.shards.is_empty() {
          // create a temporary variable to hold the result of the expression
          let tmp_name = nanoid!(16);
          // ensure name starts with a letter
          let tmp_name = format!("t{}", tmp_name);
          add_assignment_shard("Ref", &tmp_name, false, &mut sub_env);
          // wrap into a Sub Shard
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(sub_env.shards.drain(..).collect(), block.line_info)?;
          // add this sub shard before the start of this pipeline!
          e.shards.insert(start_idx, sub);
          // now add a get shard to get the temporary at the end of the pipeline
          add_get_shard(&tmp_name, false, block.line_info, e);
        }
        Ok(())
      }
      BlockContent::Func(func) => match func.name.as_str() {
        "wire" => {
          if let Some(ref params) = func.params {
            // Obtain the name of the wire either from unnamed first parameter or named parameter "Name"
            let name = if params[0].name.is_none() {
              Some(&params[0])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("Name"))
            };

            match name {
              Some(name_param) => match &name_param.value {
                Value::String(name) | Value::Identifier(name) => {
                  e.deferred_wires.insert(
                    name.clone(),
                    (
                      Wire::default(),
                      func.params.as_ref().unwrap().to_vec(),
                      block.line_info,
                    ),
                  );
                  Ok(())
                }
                _ => Err(
                  (
                    "wire built-in function requires a string parameter",
                    block.line_info,
                  )
                    .into(),
                ),
              },
              None => Err(
                (
                  "wire built-in function requires a name parameter",
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
            if params.len() != 3 {
              Err(
                (
                  "shards built-in function requires 3 parameters",
                  block.line_info,
                )
                  .into(),
              )
            } else {
              // Obtain the name of the wire either from unnamed first parameter or named parameter "Name"
              let name = if params[0].name.is_none() {
                Some(&params[0])
              } else {
                params
                  .iter()
                  .find(|param| param.name.as_deref() == Some("Name"))
              };

              // Obtain the name of the wire either from unnamed second parameter or named parameter "Args"
              let args = if params[0].name.is_none() && params[1].name.is_none() {
                Some(&params[1])
              } else {
                params
                  .iter()
                  .find(|param| param.name.as_deref() == Some("Args"))
              };

              // Obtain the name of the wire either from unnamed third parameter or named parameter "Shards"
              let shards =
                if params[0].name.is_none() && params[1].name.is_none() && params[2].name.is_none()
                {
                  Some(&params[2])
                } else {
                  params
                    .iter()
                    .find(|param| param.name.as_deref() == Some("Shards"))
                };

              match (name, args, shards) {
                (Some(name_param), Some(args_param), Some(shards_param)) => {
                  match (&name_param.value, &args_param.value, &shards_param.value) {
                    (
                      Value::String(name) | Value::Identifier(name),
                      Value::Seq(args),
                      Value::Shards(shards),
                    ) => {
                      e.shards_groups
                        .insert(name.to_owned(), (args.clone(), shards.clone()));
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
                }
                _ => Err(
                  (
                    "shards built-in function requires a name parameter",
                    block.line_info,
                  )
                    .into(),
                ),
              }
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
        unknown => {
          let mut shards_env = {
            let shards = e.shards_groups.get(unknown);
            if let Some((args, shards)) = shards {
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

              // inherit current_replacements from parent
              sub_env.replacements = e.replacements.clone();
              // set a random suffix
              sub_env.suffix = Some(nanoid!(16));

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
                sub_env
                  .replacements
                  .insert(arg.to_owned(), param.value.clone());
              }

              for stmt in &shards.statements {
                eval_statement(stmt, &mut sub_env)?;
              }

              Some(sub_env)
            } else {
              None
            }
          };
          if let Some(shards_env) = &mut shards_env {
            for shard in shards_env.shards.drain(..) {
              e.shards.push(shard);
            }
            Ok(())
          } else {
            Err((format!("Unknown function: {}", func.name), block.line_info).into())
          }
        }
      },
    }?;
  }
  Ok(())
}

fn add_assignment_shard(shard_name: &str, name: &str, suffixed: bool, e: &mut EvalEnv) {
  if let Some(previous) = e.previous {
    let shard = ShardRef::create(shard_name).unwrap();
    let shard = SShardRef(shard);
    match (suffixed, &e.suffix) {
      (true, Some(suffix)) => {
        e.suffix_assigned.insert(name.to_owned());
        let name = format!("{}{}", name, suffix);
        let name = Var::ephemeral_string(&name);
        shard.0.set_parameter(0, name);
      }
      _ => {
        let name = Var::ephemeral_string(name);
        shard.0.set_parameter(0, name);
      }
    }
    shard.0.set_line_info(previous.get_line_info());
    e.previous = Some(shard.0);
    e.shards.push(shard);
  }
}

fn eval_assignment(assignment: &Assignment, e: &mut EvalEnv) -> Result<(), ShardsError> {
  match assignment {
    Assignment::AssignRef(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Ref", &name, true, e);
      Ok(())
    }
    Assignment::AssignSet(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Set", &name, true, e);
      Ok(())
    }
    Assignment::AssignUpd(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Update", &name, true, e);
      Ok(())
    }
    Assignment::AssignPush(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Push", &name, true, e);
      Ok(())
    }
  }
}

fn eval_statement(stmt: &Statement, e: &mut EvalEnv) -> Result<(), ShardsError> {
  match stmt {
    Statement::Assignment(a) => eval_assignment(a, e),
    Statement::Pipeline(p) => eval_pipeline(p, e),
  }
}

pub fn read(code: &str) -> Result<Sequence, ShardsError> {
  let successful_parse = ShardsParser::parse(Rule::Program, code).expect("Code parsing failed");
  process_program(successful_parse.into_iter().next().unwrap())
}

pub fn eval(seq: &Sequence, name: &str) -> Result<Wire, ShardsError> {
  let mut env = eval_sequence(seq, None, None)?;
  let wire = Wire::default();
  wire.set_name(name);
  finalize_env(&mut env)?;
  for shard in env.shards.drain(..) {
    wire.add_shard(shard.0);
  }
  Ok(wire)
}

use std::ffi::CString;
use std::os::raw::c_char;
use std::panic::catch_unwind;

#[repr(C)]
pub struct SHLError {
  message: *mut c_char,
}

#[repr(C)]
pub struct SHLAst {
  ast: *mut Sequence,
  error: *mut SHLError,
}

#[repr(C)]
pub struct SHLWire {
  wire: *mut Wire,
  error: *mut SHLError,
}

#[no_mangle]
pub extern "C" fn shards_init(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }
}

#[no_mangle]
pub extern "C" fn shards_read(code: *const c_char) -> SHLAst {
  let code = unsafe { CStr::from_ptr(code).to_str().unwrap() };
  let result = catch_unwind(|| read(code));
  match result {
    Ok(Ok(sequence)) => SHLAst {
      ast: Box::into_raw(Box::new(sequence)),
      error: std::ptr::null_mut(),
    },
    Ok(Err(error)) => {
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
      };
      SHLAst {
        ast: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
    Err(_) => SHLAst {
      ast: std::ptr::null_mut(),
      error: std::ptr::null_mut(),
    },
  }
}

#[no_mangle]
pub extern "C" fn shards_eval(sequence: *mut Sequence, name: *const c_char) -> SHLWire {
  let name = unsafe { CStr::from_ptr(name).to_str().unwrap() };
  // we just want a reference to the sequence, not ownership
  let seq = unsafe { &*sequence };
  let result = catch_unwind(|| eval(seq, name));
  match result {
    Ok(Ok(wire)) => SHLWire {
      wire: Box::into_raw(Box::new(wire)),
      error: std::ptr::null_mut(),
    },
    Ok(Err(error)) => {
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
      };
      SHLWire {
        wire: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
    Err(_) => SHLWire {
      wire: std::ptr::null_mut(),
      error: std::ptr::null_mut(),
    },
  }
}

#[no_mangle]
pub extern "C" fn shards_print_ast(ast: *mut Sequence) -> Var {
  let seq = unsafe { &*ast };
  let s = print_ast(seq);
  let s = Var::ephemeral_string(&s);
  let mut v = Var::default();
  cloneVar(&mut v, &s);
  v
}

#[no_mangle]
pub extern "C" fn shards_free_sequence(sequence: *mut Sequence) {
  unsafe {
    drop(Box::from_raw(sequence));
  }
}

#[no_mangle]
pub extern "C" fn shards_free_wire(wire: *mut Wire) {
  unsafe {
    drop(Box::from_raw(wire));
  }
}

#[no_mangle]
pub extern "C" fn shards_free_error(error: *mut SHLError) {
  unsafe {
    drop(CString::from_raw((*error).message));
    drop(Box::from_raw(error));
  }
}
