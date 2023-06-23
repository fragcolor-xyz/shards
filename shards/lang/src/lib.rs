extern crate pest;
#[macro_use]
extern crate pest_derive;

mod read;
mod types;

use crate::read::*;
use crate::types::*;
use core::convert::TryInto;
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

// we need to implement Value into Var conversion
fn as_var(
  value: Value,
  line_info: LineInfo,
  shard: Option<ShardRef>,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  match value {
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
    Value::Seq(vec) => {
      let mut seq = SeqVar::new();
      for value in vec {
        // clone of a clone of a clone 😭
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
      let mut sub_env = EvalEnv {
        shards: Vec::new(),
        previous: shard,
      };
      for stmt in seq.statements {
        eval_statement(stmt, &mut sub_env)?;
      }
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
    Value::Expr(_) => todo!(),
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
        for (i, info) in info.iter().enumerate() {
          let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() };
          if param_name == name {
            s.0
              .set_parameter(i.try_into().expect("Too many parameters"), *value.as_ref());
            break;
          }
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

fn eval_pipeline(pipeline: Pipeline, e: &mut EvalEnv) -> Result<(), ShardsError> {
  for block in pipeline.blocks {
    let _ = match block {
      Block::BlockContent(content, line_info) => match content {
        BlockContent::Shard(shard) => add_shard(shard, line_info, e),
        BlockContent::Const(value) => add_const_shard(value, line_info, e),
        BlockContent::TakeTable(_, _) => unimplemented!("TakeTable"),
        BlockContent::TakeSeq(_, _) => unimplemented!("TakeSeq"),
      },
      Block::EvalExpr(_) => {
        unimplemented!("EvalExpr")
      }
      Block::Expr(_) => {
        unimplemented!("Expr")
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
  // eval_statements
  let mut eval_env = EvalEnv {
    shards: Vec::new(),
    previous: None,
  };
  for stmt in seq.statements {
    eval_statement(stmt, &mut eval_env)?;
  }
  let wire = Wire::default();
  wire.set_name(name);
  for shard in eval_env.shards {
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
