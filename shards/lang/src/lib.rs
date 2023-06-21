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
use shards::SHType_ContextVar;
use std::ffi::CStr;

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
fn as_var(value: Value, line_info: LineInfo, e: &mut EvalEnv) -> Result<SVar, ShardsError> {
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
        let value = as_var(value, line_info, e)?;
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
        let value = as_var(value, line_info, e)?;
        let key = Var::ephemeral_string(key.as_str());
        match value {
          SVar::Cloned(v) => {
            // insert will clone and v will be dropped when out of this scope
            table.insert(key, &v.0);
          }
          SVar::NotCloned(v) => {
            table.insert(key, &v);
          }
        }
      }
      Ok(SVar::Cloned(ClonedVar(table.0)))
    }
    Value::Shards(_) => todo!(),
    Value::EvalExpr(_) => todo!(),
    Value::Expr(_) => todo!(),
    Value::TakeTable(_, _) => todo!(),
    Value::TakeSeq(_, _) => todo!(),
  }
}

struct EvalEnv {
  wire: Wire,
  previous: Option<ShardRef>,
}

fn add_shard(shard: Shard, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let s = ShardRef::create(shard.name.as_str());
  e.wire.add_shard(s); // add immediately to wire to prevent leaks if we have errors
  let mut idx = 0i32;
  let mut as_idx = true;
  let info = s.parameters();
  if let Some(params) = shard.params {
    for param in params {
      let value = as_var(param.value, line_info, e)?;
      if let Some(name) = param.name {
        as_idx = false;
        for (i, info) in info.iter().enumerate() {
          let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() };
          if param_name == name {
            s.set_parameter(i.try_into().expect("Too many parameters"), *value.as_ref());
            break;
          }
        }
      } else {
        if !as_idx {
          return Err(("Named parameter after unnamed parameter", line_info).into());
        }
        s.set_parameter(idx, *value.as_ref());
      }
      idx += 1;
    }
  }
  e.previous = Some(s);
  Ok(())
}

fn add_const_shard(value: Value, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = ShardRef::create("Const");
  e.wire.add_shard(shard); // add immediately to wire to prevent leaks if we have errors
  let value = as_var(value, line_info, e)?;
  shard.set_parameter(0, *value.as_ref());
  shard.set_line_info((
    line_info.line.try_into().expect("Too many lines"),
    line_info.column.try_into().expect("Oversized column"),
  ));
  e.previous = Some(shard);
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
    e.wire.add_shard(shard); // add immediately to wire to prevent leaks if we have errors
    let name = Var::ephemeral_string(name);
    shard.set_parameter(0, name);
    shard.set_line_info(previous.get_line_info());
    e.previous = Some(shard);
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

pub fn eval(seq: Sequence) -> Result<Wire, ShardsError> {
  // eval_statements
  let mut eval_env = EvalEnv {
    wire: Wire::default(),
    previous: None,
  };
  for stmt in seq.statements {
    eval_statement(stmt, &mut eval_env)?;
  }
  Ok(eval_env.wire)
}

use std::ffi::{CString};
use std::os::raw::{c_char, c_void};
use std::panic::{catch_unwind, UnwindSafe};

#[repr(C)]
pub struct SHLError {
  message: *mut c_char,
}

#[no_mangle]
pub extern "C" fn shards_read(code: *const c_char) -> *mut Sequence {
  let code = unsafe { CStr::from_ptr(code).to_str().unwrap() };
  let result = catch_unwind(|| read(code));
  match result {
    Ok(Ok(sequence)) => Box::into_raw(Box::new(sequence)),
    Ok(Err(error)) => {
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
      };
      Box::into_raw(Box::new(shards_error)) as *mut Sequence
    }
    Err(_) => std::ptr::null_mut(),
  }
}

#[no_mangle]
pub extern "C" fn shards_eval(sequence: *mut Sequence) -> *mut Wire {
  let result = catch_unwind(|| eval(unsafe { *Box::from_raw(sequence) }));
  match result {
    Ok(Ok(wire)) => Box::into_raw(Box::new(wire)),
    Ok(Err(error)) => {
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
      };
      Box::into_raw(Box::new(shards_error)) as *mut Wire
    }
    Err(_) => std::ptr::null_mut(),
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

/*
// Reads a Shards code string and returns a pointer to a Sequence struct.
// If there is an error, returns a pointer to a SHLError struct.
// The returned pointer must be freed with shards_free_sequence.
struct Sequence* shards_read(const char* code);

// Evaluates a Sequence and returns a pointer to a Wire struct.
// If there is an error, returns a pointer to a SHLError struct.
// The returned pointer must be freed with shards_free_wire.
struct Wire* shards_eval(struct Sequence* sequence);

// Frees a Sequence struct.
void shards_free_sequence(struct Sequence* sequence);

// Frees a Wire struct.
void shards_free_wire(struct Wire* wire);

// Frees a SHLError struct.
void shards_free_error(struct SHLError* error);
*/