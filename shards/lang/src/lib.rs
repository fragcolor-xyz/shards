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
      Ok(SVar::NotCloned(s))
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
      Ok(SVar::NotCloned(s))
    }
    Value::Float2(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Float3(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Float4(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Int2(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Int3(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Int4(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Int8(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Int16(val) => {
      todo!("Vector not implemented yet")
    }
    Value::Seq(vec) => {
      let mut seq = SeqVar::new();
      for value in vec {
        let value = as_var(value, line_info, shard, e)?;
        match value {
          SVar::Cloned(v) => {
            // push will clone and v will be dropped when out of this scope
            seq.push(&v.0)
          }
          SVar::NotCloned(v) => seq.push(&v),
        }
      }
      Ok(SVar::Cloned(ClonedVar(seq.0)))
    }
    Value::Table(value) => {
      let mut table = TableVar::new();
      for (key, value) in value {
        let key = as_var(key, line_info, shard, e)?;
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
    Value::EvalExpr(_) => todo!(),
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

fn add_shard(shard: Shard, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let s = ShardRef::create(shard.name.as_str());
  s.setup();
  let s = SShardRef(s);
  let mut idx = 0i32;
  let mut as_idx = true;
  let info = s.0.parameters();
  if let Some(params) = shard.params {
    for param in params {
      let value = as_var(param.value, line_info, Some(s.0), e)?;
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
  let shard = ShardRef::create("Const");
  shard.setup();
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
  let shard = ShardRef::create("Const");
  shard.setup();
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
  let shard = ShardRef::create("Sub");
  shard.setup();
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

fn add_get_shard(name: &str, e: &mut EvalEnv) {
  let shard = ShardRef::create("Get");
  shard.setup();
  let shard = SShardRef(shard);
  let name = Var::ephemeral_string(name);
  shard.0.set_parameter(0, name);
  if let Some(previous) = e.previous {
    shard.0.set_line_info(previous.get_line_info());
  }
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
        BlockContent::Shard(shard) => add_shard(shard, line_info, e),
        BlockContent::Const(value) => {
          // remove the nil shard we injected if this is the first block
          if e.shards.len() == start_idx + 1 {
            e.shards.pop();
          }
          add_const_shard(value, line_info, e)
        }
        BlockContent::TakeTable(_, _) => unimplemented!("TakeTable"),
        BlockContent::TakeSeq(_, _) => unimplemented!("TakeSeq"),
        BlockContent::Operator(_, _) => unimplemented!("Operator"),
      },
      Block::EvalExpr(seq) => {
        let sub_env = eval_sequence(seq, None)?;
        if !sub_env.shards.is_empty() {
          let line_info = sub_env.shards[0].0.get_line_info();
          // create an ephemeral wire, execute and grab result
          let wire = Wire::default();
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
            return Err(
              (
                msg,
                LineInfo {
                  line: line_info.0 as usize,
                  column: line_info.1 as usize,
                },
              )
                .into(),
            );
          } else {
            todo!("Grab result from wire")
          }
        }
        Ok(())
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
          add_get_shard(&tmp_name, e);
        }
        Ok(())
      }
    }?;
  }
  Ok(())
}

fn add_assignment_shard(shard_name: &str, name: &str, e: &mut EvalEnv) {
  if let Some(previous) = e.previous {
    let shard = ShardRef::create(shard_name);
    shard.setup();
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
