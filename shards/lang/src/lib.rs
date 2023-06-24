extern crate pest;
#[macro_use]
extern crate pest_derive;

mod read;
mod types;

use crate::read::*;
use crate::types::*;
use core::convert::TryInto;
use nanoid::nanoid;
use pest::{iterators::Pair, Parser, Position};
use serde::{Deserialize, Serialize};
use shards::SHType_String;
use shards::types::ClonedVar;
use shards::types::Mesh;
use shards::types::Seq;
use shards::types::SeqVar;
use shards::types::TableVar;
use shards::types::{ShardRef, Var, Wire, WireRef};
use shards::ShardPtr;
use shards::{SHType_ContextVar, SHType_ShardRef};
use std::ffi::CStr;
use std::fmt;

struct SShardRef(pub(crate) ShardRef);

impl Drop for SShardRef {
  fn drop(&mut self) {
    self.0.destroy();
  }
}

struct EvalEnv {
  shards: Vec<SShardRef>,
  previous: Option<ShardRef>,
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

fn eval_eval_expr(seq: Sequence) -> Result<(ClonedVar, LineInfo), ShardsError> {
  let sub_env = eval_sequence(seq, None)?;
  if !sub_env.shards.is_empty() {
    let line_info = sub_env.shards[0].0.get_line_info();
    // create an ephemeral wire, execute and grab result
    let wire = Wire::default();
    wire.set_name("eval-ephemeral");
    for shard in sub_env.shards {
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
      let msg = unsafe { CStr::from_ptr(info.failureMessage) }
        .to_str()
        .expect("Invalid UTF8");
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
        unsafe { *info.finalOutput }.into(),
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

fn eval_sequence(seq: Sequence, previous: Option<ShardRef>) -> Result<EvalEnv, ShardsError> {
  let mut sub_env = EvalEnv {
    shards: Vec::new(),
    previous,
  };
  for stmt in seq.statements {
    eval_statement(stmt, &mut sub_env)?;
  }
  Ok(sub_env)
}

fn create_take_table_chain(
  var_name: String,
  path: Vec<String>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(&var_name, line, e);
  for path_part in path {
    let s = Var::ephemeral_string(&path_part);
    add_take_shard(&s, e);
  }
  Ok(())
}

fn create_take_seq_chain(
  var_name: String,
  path: Vec<u32>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(&var_name, line, e);
  for path_part in path {
    let idx = path_part.try_into().unwrap(); // read should have caught this
    add_take_shard(&idx, e);
  }
  Ok(())
}

// we need to implement Value into Var conversion
fn as_var(
  value: Value,
  line_info: LineInfo,
  shard: Option<ShardRef>,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  match value {
    Value::None => Ok(SVar::NotCloned(Var::default())),
    Value::Boolean(value) => Ok(SVar::NotCloned(value.into())),
    Value::Identifier(value) => {
      let mut s = Var::ephemeral_string(value.as_str());
      s.valueType = SHType_ContextVar;
      Ok(SVar::Cloned(s.into()))
    }
    Value::Number(num) => match num {
      Number::Integer(n) => Ok(SVar::NotCloned(n.into())),
      Number::Float(n) => Ok(SVar::NotCloned(n.into())),
      Number::Hexadecimal(s) => {
        let s = s.as_str();
        let s = &s[2..]; // remove 0x
        let z = i64::from_str_radix(s, 16).expect("Invalid hexadecimal number"); // read should have caught this
        Ok(SVar::NotCloned(z.into()))
      }
    },
    Value::String(s) => {
      let s = Var::ephemeral_string(s.as_str());
      Ok(SVar::Cloned(s.into()))
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
      let mut seq = SeqVar::new();
      for value in vec {
        let value = as_var(value, line_info, shard, e)?;
        seq.push(value.as_ref());
      }
      Ok(SVar::Cloned(ClonedVar(seq.0)))
    }
    Value::Table(value) => {
      let mut table = TableVar::new();
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
      let sub_env = eval_sequence(seq, shard)?;
      let mut seq = SeqVar::new();
      for shard in sub_env.shards {
        let s = shard.0 .0;
        let s: Var = s.into();
        debug_assert!(s.valueType == SHType_ShardRef);
        seq.push(&s);
      }
      Ok(SVar::Cloned(ClonedVar(seq.0)))
    }
    Value::EvalExpr(seq) => {
      let value = eval_eval_expr(seq)?;
      Ok(SVar::Cloned(value.0))
    }
    Value::Expr(seq) => {
      let start_idx = e.shards.len();
      let mut sub_env = eval_sequence(seq, e.previous)?;
      if !sub_env.shards.is_empty() {
        // create a temporary variable to hold the result of the expression
        let tmp_name = nanoid!();
        // ensure name starts with a letter
        let tmp_name = format!("t{}", tmp_name);
        add_assignment_shard("Ref", &tmp_name, &mut sub_env);
        // wrap into a Sub Shard
        let line_info = sub_env.shards[0].0.get_line_info();
        let sub = make_sub_shard(
          sub_env.shards,
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
    Value::TakeTable(_, _) => todo!(),
    Value::TakeSeq(_, _) => todo!(),
  }
}

enum AddShardError {
  ShardsError(ShardsError),
  InvalidShardName(String),
}

fn add_shard(shard: Shard, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), AddShardError> {
  let s =
    ShardRef::create(shard.name.as_str()).ok_or(AddShardError::InvalidShardName(shard.name))?;
  let s = SShardRef(s);
  let mut idx = 0i32;
  let mut as_idx = true;
  let info = s.0.parameters();
  if let Some(params) = shard.params {
    for param in params {
      let value =
        as_var(param.value, line_info, Some(s.0), e).map_err(|e| AddShardError::ShardsError(e))?;
      if let Some(name) = param.name {
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
          return Err(AddShardError::ShardsError((msg, line_info).into()));
        }
      } else {
        if !as_idx {
          return Err(AddShardError::ShardsError(
            ("Named parameter after unnamed parameter", line_info).into(),
          ));
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

fn add_const_shard(value: Value, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Const").unwrap();
  let shard = SShardRef(shard);
  let value = as_var(value, line_info, Some(shard.0), e)?;
  shard.0.set_parameter(0, *value.as_ref());
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
  let mut seq = SeqVar::new();
  for shard in shards {
    let s = shard.0 .0;
    let s: Var = s.into();
    debug_assert!(s.valueType == SHType_ShardRef);
    seq.push(&s);
  }
  shard.0.set_parameter(0, seq.0.into());
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

fn add_get_shard(name: &str, line: LineInfo, e: &mut EvalEnv) {
  let shard = ShardRef::create("Get").unwrap();
  let shard = SShardRef(shard);
  let name = Var::ephemeral_string(name);
  shard.0.set_parameter(0, name);
  shard.0.set_line_info((
    line.line.try_into().unwrap(),
    line.column.try_into().unwrap(),
  ));
  e.previous = Some(shard.0);
  e.shards.push(shard);
}

fn eval_pipeline(pipeline: Pipeline, e: &mut EvalEnv) -> Result<(), ShardsError> {
  // a pipeline starts with no previous shard and an implicit nil
  // this is useful for many shards such as Http.
  add_const_shard2(Var::default(), LineInfo::default(), e)?;
  e.previous = None;
  let start_idx = e.shards.len();
  for block in pipeline.blocks {
    let _ = match block {
      Block::BlockContent(content, line_info) => match content {
        BlockContent::Shard(shard) => {
          // ok the reality is that we could also be a variable, so we need to check that
          let has_params = shard.params.is_some();
          let res = add_shard(shard, line_info, e);
          match res {
            Ok(_) => Ok(()),
            Err(err) => {
              match err {
                AddShardError::ShardsError(err) => Err(err),
                AddShardError::InvalidShardName(name) => {
                  if !has_params {
                    // we assume we are a variable
                    Ok(add_get_shard(&name, line_info, e))
                  } else {
                    Err((format!("Invalid shard name: {}", name), line_info).into())
                  }
                }
              }
            }
          }
        }
        BlockContent::Const(value) => {
          // remove the nil shard we injected if this is the first block
          if e.shards.len() == start_idx + 1 {
            e.shards.pop();
          }
          add_const_shard(value, line_info, e)
        }
        BlockContent::TakeTable(name, path) => create_take_table_chain(name, path, line_info, e),
        BlockContent::TakeSeq(name, path) => create_take_seq_chain(name, path, line_info, e),
        BlockContent::Operator(_, _) => todo!("Operator"),
      },
      Block::EvalExpr(seq) => {
        let value = eval_eval_expr(seq)?;
        add_const_shard2(value.0 .0, value.1, e)
      }
      Block::Expr(seq) => {
        let mut sub_env = eval_sequence(seq, e.previous)?;
        if !sub_env.shards.is_empty() {
          // create a temporary variable to hold the result of the expression
          let tmp_name = nanoid!();
          // ensure name starts with a letter
          let tmp_name = format!("t{}", tmp_name);
          add_assignment_shard("Ref", &tmp_name, &mut sub_env);
          // wrap into a Sub Shard
          let line_info = sub_env.shards[0].0.get_line_info();
          let sub = make_sub_shard(
            sub_env.shards,
            LineInfo {
              line: line_info.0 as usize,
              column: line_info.1 as usize,
            },
          )?;
          // add this sub shard before the start of this pipeline!
          e.shards.insert(start_idx, sub);
          // now add a get shard to get the temporary at the end of the pipeline
          add_get_shard(
            &tmp_name,
            LineInfo {
              line: line_info.0 as usize,
              column: line_info.1 as usize,
            },
            e,
          );
        }
        Ok(())
      }
    }?;
  }
  Ok(())
}

fn add_assignment_shard(shard_name: &str, name: &str, e: &mut EvalEnv) {
  if let Some(previous) = e.previous {
    let shard = ShardRef::create(shard_name).unwrap();
    let shard = SShardRef(shard);
    let name = Var::ephemeral_string(name);
    shard.0.set_parameter(0, name);
    shard.0.set_line_info(previous.get_line_info());
    e.previous = Some(shard.0);
    e.shards.push(shard);
  }
}

fn eval_assignment(assignment: Assignment, e: &mut EvalEnv) -> Result<(), ShardsError> {
  match assignment {
    Assignment::AssignRef(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Ref", &name, e);
      Ok(())
    }
    Assignment::AssignSet(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Set", &name, e);
      Ok(())
    }
    Assignment::AssignUpd(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Update", &name, e);
      Ok(())
    }
    Assignment::AssignPush(pipe, name) => {
      eval_pipeline(pipe, e)?;
      add_assignment_shard("Push", &name, e);
      Ok(())
    }
  }
}

fn eval_statement(stmt: Statement, e: &mut EvalEnv) -> Result<(), ShardsError> {
  match stmt {
    Statement::Assignment(a) => eval_assignment(a, e),
    Statement::Pipeline(p) => eval_pipeline(p, e),
  }
}

pub fn read(code: &str) -> Result<Sequence, ShardsError> {
  let successful_parse = ShardsParser::parse(Rule::Sequence, code).expect("Code parsing failed");
  process_sequence(successful_parse.into_iter().next().unwrap())
}

pub fn eval(seq: Sequence, name: &str) -> Result<Wire, ShardsError> {
  let env = eval_sequence(seq, None)?;
  let wire = Wire::default();
  wire.set_name(name);
  for shard in env.shards {
    wire.add_shard(shard.0);
  }
  Ok(wire)
}

use std::ffi::CString;
use std::os::raw::{c_char, c_void};
use std::panic::{catch_unwind, UnwindSafe};

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
  let result = catch_unwind(|| eval(unsafe { *Box::from_raw(sequence) }, name));
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
