extern crate pest;
#[macro_use]
extern crate pest_derive;

extern crate clap;

pub mod ast;
pub mod ast_visitor;
pub mod cli;
mod error;
pub mod eval;
mod formatter;
pub mod print;
pub mod read;
pub mod rule_visitor;

use crate::ast::*;

use core::fmt;

use std::borrow::Cow;
use std::ops::Deref;

use shards::types::{AutoShardRef, ClonedVar};

use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::hash::{Hash, Hasher};
use std::rc::Rc;

#[derive(Debug, Clone, PartialEq)]
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
pub struct RcStrWrapper(Rc<Cow<'static, str>>);

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
    Ok(RcStrWrapper(Rc::new(Cow::Owned(s))))
  }
}

impl RcStrWrapper {
  pub fn new<S: Into<Cow<'static, str>>>(s: S) -> Self {
    RcStrWrapper(Rc::new(s.into()))
  }

  pub fn to_string(&self) -> String {
    self.0.to_string()
  }

  pub fn as_str(&self) -> &str {
    &self.0
  }

  pub fn to_mut(&mut self) -> &mut String {
    Rc::get_mut(&mut self.0)
      .expect("Cannot get mutable reference")
      .to_mut()
  }
}

impl From<&'static str> for RcStrWrapper {
  fn from(s: &'static str) -> Self {
    RcStrWrapper::new(Cow::Borrowed(s))
  }
}

impl From<String> for RcStrWrapper {
  fn from(s: String) -> Self {
    RcStrWrapper::new(Cow::Owned(s))
  }
}

impl Eq for RcStrWrapper {}

impl PartialEq<RcStrWrapper> for RcStrWrapper {
  fn eq(&self, other: &RcStrWrapper) -> bool {
    self.0 == other.0
  }
}

impl PartialEq<str> for RcStrWrapper {
  fn eq(&self, other: &str) -> bool {
    *self.0 == other
  }
}

impl Hash for RcStrWrapper {
  fn hash<H: Hasher>(&self, state: &mut H) {
    self.0.hash(state)
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
