extern crate pest;
#[macro_use]
extern crate pest_derive;

extern crate clap;

mod ast;
mod ast_visitor;
mod cli;
mod error;
mod eval;
mod formatter;
mod print;
mod read;
mod visual_ast;

use crate::ast::*;

use core::fmt;
use std::collections::HashMap;

use eval::merge_env;
use eval::new_cancellation_token;
use eval::EvalEnv;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shlog_error;
use shards::types::Var;

use std::ops::Deref;

use shards::types::{AutoShardRef, ClonedVar, Wire};
use shards::SHStringWithLen;

use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::hash::Hash;
use std::rc::Rc;

use std::ffi::CString;
use std::os::raw::c_char;

use shards::util::from_raw_parts_allow_null;

#[derive(Debug, Clone)]
pub struct RcBytesWrapper(Rc<[u8]>);

impl Serialize for RcBytesWrapper {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: Serializer,
  {
    serializer.serialize_bytes(&self.0)
  }
}

impl<'de> Deserialize<'de> for RcBytesWrapper {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    let s: &[u8] = Deserialize::deserialize(deserializer)?;
    Ok(RcBytesWrapper(Rc::from(s)))
  }
}

impl RcBytesWrapper {
  pub fn new<S: Into<Rc<[u8]>>>(s: S) -> Self {
    RcBytesWrapper(s.into())
  }

  pub fn to_vec(&self) -> Vec<u8> {
    self.0.to_vec()
  }

  pub fn as_slice(&self) -> &[u8] {
    &self.0
  }
}

impl From<&[u8]> for RcBytesWrapper {
  fn from(s: &[u8]) -> Self {
    RcBytesWrapper::new(s)
  }
}

impl From<Vec<u8>> for RcBytesWrapper {
  fn from(s: Vec<u8>) -> Self {
    RcBytesWrapper::new(s)
  }
}

#[derive(Debug, Clone)]
pub struct RcStrWrapper(Rc<str>);

impl Serialize for RcStrWrapper {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: Serializer,
  {
    serializer.serialize_str(&self.0)
  }
}

impl<'de> Deserialize<'de> for RcStrWrapper {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    let s = String::deserialize(deserializer)?;
    Ok(RcStrWrapper(Rc::from(s)))
  }
}

impl RcStrWrapper {
  pub fn new<S: Into<Rc<str>>>(s: S) -> Self {
    RcStrWrapper(s.into())
  }

  pub fn to_string(&self) -> String {
    self.0.to_string()
  }

  pub fn as_str(&self) -> &str {
    &self.0
  }
}

impl From<&str> for RcStrWrapper {
  fn from(s: &str) -> Self {
    RcStrWrapper::new(s)
  }
}

impl From<String> for RcStrWrapper {
  fn from(s: String) -> Self {
    RcStrWrapper::new(s)
  }
}

impl Eq for RcStrWrapper {}

impl PartialEq<RcStrWrapper> for RcStrWrapper {
  fn eq(&self, other: &RcStrWrapper) -> bool {
    let s: &str = &self.0;
    let o: &str = &other.0;
    s == o
  }
}

impl PartialEq<str> for RcStrWrapper {
  fn eq(&self, other: &str) -> bool {
    let s: &str = &self.0;
    s == other
  }
}

impl Hash for RcStrWrapper {
  fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
    let s: &str = &self.0;
    s.hash(state);
  }
}

impl fmt::Display for RcStrWrapper {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "{}", self.0)
  }
}

impl Deref for RcStrWrapper {
  type Target = str;

  fn deref(&self) -> &Self::Target {
    &self.0
  }
}

pub struct ParamHelper<'a> {
  params: &'a [Param],
}

impl<'a> ParamHelper<'a> {
  pub fn new(params: &'a [Param]) -> Self {
    Self { params }
  }

  pub fn get_param_by_name_or_index(&self, param_name: &str, index: usize) -> Option<&'a Param> {
    if index < self.params.len() {
      if self.params[index].name.is_none() && index > 0 && self.params[index - 1].name.is_some() {
        // Previous parameter is named, we forbid indexed parameters after named parameters
        None
      } else if self.params[index].name.is_none() {
        // Parameter is unnamed and its index is the one we want
        Some(&self.params[index])
      } else {
        // Parameter is named, we look for a parameter with the given name
        self
          .params
          .iter()
          .find(|param| param.name.as_deref() == Some(param_name))
      }
    } else {
      // Index is out of bounds, we look for a parameter with the given name
      self
        .params
        .iter()
        .find(|param| param.name.as_deref() == Some(param_name))
    }
  }
}

pub trait ShardsExtension {
  fn name(&self) -> &str;
  fn process_to_var(
    &mut self,
    func: &Function,
    line_info: LineInfo,
  ) -> Result<ClonedVar, ShardsError>;
  fn process_to_shard(
    &mut self,
    func: &Function,
    line_info: LineInfo,
  ) -> Result<AutoShardRef, ShardsError>;
}

#[repr(C)]
pub struct SHLError {
  message: *mut c_char,
  line: u32,
  column: u32,
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
pub extern "C" fn shards_read(
  name: SHStringWithLen,
  code: SHStringWithLen,
  base_path: SHStringWithLen,
) -> SHLAst {
  let name: &str = name.into();
  let code = code.into();
  let base_path: &str = base_path.into();
  let result = read::read(code, name, base_path);

  match result {
    Ok(p) => SHLAst {
      ast: Box::into_raw(Box::new(p.sequence)),
      error: std::ptr::null_mut(),
    },
    Err(error) => {
      shlog_error!("{}:{}: {}", error.loc.line, error.loc.column, error.message);
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
        line: error.loc.line,
        column: error.loc.column,
      };
      SHLAst {
        ast: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
  }
}

#[no_mangle]
pub extern "C" fn shards_load_ast(bytes: *mut u8, size: u32) -> SHLAst {
  let bytes = unsafe { from_raw_parts_allow_null(bytes, size as usize) };
  let decoded_bin: Result<Sequence, _> = bincode::deserialize(bytes);
  match decoded_bin {
    Ok(sequence) => SHLAst {
      ast: Box::into_raw(Box::new(sequence)),
      error: std::ptr::null_mut(),
    },
    Err(error) => {
      let error_message = CString::new(error.to_string()).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
        line: 0,
        column: 0,
      };
      SHLAst {
        ast: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
  }
}

#[no_mangle]
pub extern "C" fn shards_save_ast(ast: *mut Sequence) -> Var {
  let ast = unsafe { &*ast };
  let encoded_bin = bincode::serialize(ast).unwrap();
  let v: ClonedVar = encoded_bin.as_slice().into();
  let inner = v.0;
  std::mem::forget(v);
  inner
}

#[no_mangle]
pub extern "C" fn shards_create_env(namespace: SHStringWithLen) -> *mut EvalEnv {
  if namespace.len == 0 {
    Box::into_raw(Box::new(EvalEnv::new(None, None)))
  } else {
    let namespace: &str = namespace.into();
    Box::into_raw(Box::new(EvalEnv::new(Some(namespace.into()), None)))
  }
}

#[no_mangle]
pub extern "C" fn shards_forbid_shard(env: *mut EvalEnv, name: SHStringWithLen) {
  let env = unsafe { &mut *env };
  let name: &str = name.into();
  env.forbidden_funcs.insert(Identifier {
    name: RcStrWrapper::from(name),
    namespaces: Vec::new(),
  });
}

#[no_mangle]
pub extern "C" fn shards_free_env(env: *mut EvalEnv) {
  unsafe {
    drop(Box::from_raw(env));
  }
}

#[no_mangle]
pub extern "C" fn shards_create_sub_env(
  env: *mut EvalEnv,
  namespace: SHStringWithLen,
) -> *mut EvalEnv {
  let env = unsafe { &mut *env };
  if namespace.len == 0 {
    Box::into_raw(Box::new(EvalEnv::new(None, Some(env))))
  } else {
    let namespace: &str = namespace.into();
    Box::into_raw(Box::new(EvalEnv::new(Some(namespace.into()), Some(env))))
  }
}

#[no_mangle]
pub extern "C" fn shards_eval_env(env: *mut EvalEnv, ast: *mut Sequence) -> *mut SHLError {
  let env = unsafe { &mut *env };
  let ast = unsafe { &*ast };
  for stmt in &ast.statements {
    if let Err(e) = eval::eval_statement(stmt, env, new_cancellation_token()) {
      shlog_error!("{}:{}: {}", e.loc.line, e.loc.column, e.message);
      let error_message = CString::new(e.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
        line: e.loc.line,
        column: e.loc.column,
      };
      return Box::into_raw(Box::new(shards_error));
    }
  }
  core::ptr::null_mut()
}

/// It will consume the env
#[no_mangle]
pub extern "C" fn shards_transform_env(env: *mut EvalEnv, name: SHStringWithLen) -> SHLWire {
  let name = name.into();
  let mut env = unsafe { Box::from_raw(env) };
  let res = eval::transform_env(&mut env, name);
  match res {
    Ok(wire) => SHLWire {
      wire: Box::into_raw(Box::new(wire)),
      error: std::ptr::null_mut(),
    },
    Err(error) => {
      shlog_error!("{}:{}: {}", error.loc.line, error.loc.column, error.message);
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
        line: error.loc.line,
        column: error.loc.column,
      };
      SHLWire {
        wire: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
  }
}

#[no_mangle]
pub extern "C" fn shards_transform_envs(
  env: *mut *mut EvalEnv,
  len: usize,
  name: SHStringWithLen,
) -> SHLWire {
  let name = name.into();
  let envs = unsafe { std::slice::from_raw_parts_mut(env, len) };
  let mut deref_envs = Vec::with_capacity(len);
  for &env in envs.iter() {
    let env = unsafe { Box::from_raw(env) };
    deref_envs.push(env);
  }
  let res = eval::transform_envs(deref_envs.iter_mut().map(|x| x.as_mut()), name);
  match res {
    Ok(wire) => SHLWire {
      wire: Box::into_raw(Box::new(wire)),
      error: std::ptr::null_mut(),
    },
    Err(error) => {
      shlog_error!("{}:{}: {}", error.loc.line, error.loc.column, error.message);
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
        line: error.loc.line,
        column: error.loc.column,
      };
      SHLWire {
        wire: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
  }
}

#[no_mangle]
pub extern "C" fn shards_eval(sequence: *mut Sequence, name: SHStringWithLen) -> SHLWire {
  let name = name.into();
  // we just want a reference to the sequence, not ownership
  let seq = unsafe { &*sequence };
  let result = eval::eval(seq, name, HashMap::new(), new_cancellation_token());
  match result {
    Ok(wire) => SHLWire {
      wire: Box::into_raw(Box::new(wire)),
      error: std::ptr::null_mut(),
    },
    Err(error) => {
      let error_message = CString::new(error.message).unwrap();
      let shards_error = SHLError {
        message: error_message.into_raw(),
        line: error.loc.line,
        column: error.loc.column,
      };
      SHLWire {
        wire: std::ptr::null_mut(),
        error: Box::into_raw(Box::new(shards_error)),
      }
    }
  }
}

// #[no_mangle]
// pub extern "C" fn shards_print_ast(ast: *mut Sequence) -> Var {
//   let seq = unsafe { &*ast };
//   let s = print_ast(seq);
//   let s = Var::ephemeral_string(&s);
//   let mut v = Var::default();
//   cloneVar(&mut v, &s);
//   v
// }

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

#[no_mangle]
pub extern "C" fn shardsRegister_lang_lang(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<read::ReadShard>();
  register_legacy_shard::<eval::EvalShard>();
}

/// Please note it will consume `from` but not `to`
#[no_mangle]
pub extern "C" fn shards_merge_envs(from: *mut EvalEnv, to: *mut EvalEnv) -> *mut SHLError {
  let from = unsafe { Box::from_raw(from) };
  let to = unsafe { &mut *to };
  if let Err(e) = merge_env(*from, to) {
    shlog_error!("{}:{}: {}", e.loc.line, e.loc.column, e.message);
    let error_message = CString::new(e.message).unwrap();
    let shards_error = SHLError {
      message: error_message.into_raw(),
      line: e.loc.line,
      column: e.loc.column,
    };
    Box::into_raw(Box::new(shards_error))
  } else {
    std::ptr::null_mut()
  }
}

#[no_mangle]
pub extern "C" fn setup_panic_hook() {
  // Had to put this in this crate otherwise we would have duplicated symbols
  // Set a custom panic hook to break into the debugger.
  #[cfg(debug_assertions)]
  std::panic::set_hook(Box::new(|info| {
    // Print the panic info to standard error.
    eprintln!("Panic occurred: {:?}", info);
    // Trigger a breakpoint.
    #[cfg(unix)]
    unsafe {
      libc::raise(libc::SIGTRAP);
    }
    #[cfg(windows)]
    unsafe {
      windows::Win32::System::Diagnostics::Debug::DebugBreak();
    }
  }));
}
