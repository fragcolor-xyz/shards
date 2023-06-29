extern crate pest;
#[macro_use]
extern crate pest_derive;

mod ast;
mod eval;
mod print;
mod read;

use crate::ast::*;

use core::fmt;

use print::print_ast;
use shards::core::cloneVar;

use std::ops::Deref;

use shards::types::{Var, Wire};

use std::ffi::CStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::hash::Hash;
use std::rc::Rc;

use std::ffi::CString;
use std::os::raw::c_char;
use std::panic::catch_unwind;

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
  let result = catch_unwind(|| read::read(code));
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
  let result = catch_unwind(|| eval::eval(seq, name));
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
