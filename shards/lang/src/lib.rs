extern crate pest;
#[macro_use]
extern crate pest_derive;

extern crate clap;

pub mod ast;
pub mod ast_visitor;
pub mod cli;
pub mod custom_state;
pub mod directory;
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
use std::path::PathBuf;

use shards::types::{AutoShardRef, ClonedVar};

use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::hash::{Hash, Hasher};
use std::rc::Rc;

use pest_derive::Parser;
use shards::SHType_Float;
use std::collections::{HashMap, BTreeMap};

// String table for optimized serialization
#[derive(Debug, Clone, PartialEq)]
pub struct StringTable {
  pub strings: Vec<String>,
  pub indices: HashMap<String, u32>,
}

impl StringTable {
  pub fn new() -> Self {
    Self {
      strings: Vec::new(),
      indices: HashMap::new(),
    }
  }

  pub fn insert(&mut self, s: &str) -> u32 {
    if let Some(&index) = self.indices.get(s) {
      index
    } else {
      let index = self.strings.len() as u32;
      self.strings.push(s.to_string());
      self.indices.insert(s.to_string(), index);
      index
    }
  }

  pub fn get(&self, index: u32) -> Option<&str> {
    self.strings.get(index as usize).map(|s| s.as_str())
  }

  pub fn len(&self) -> usize {
    self.strings.len()
  }
}

impl Serialize for StringTable {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: Serializer,
  {
    self.strings.serialize(serializer)
  }
}

impl<'de> Deserialize<'de> for StringTable {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    let strings = Vec::<String>::deserialize(deserializer)?;
    let mut indices = HashMap::new();
    for (index, string) in strings.iter().enumerate() {
      indices.insert(string.clone(), index as u32);
    }
    Ok(Self { strings, indices })
  }
}

// Structure for serializing with string table
#[derive(Serialize, Deserialize, Debug)]
pub struct StringTableSerialization<T> {
  pub string_table: StringTable,
  pub data: T,
}

// Context for building string table during serialization
pub struct StringTableContext {
  pub table: StringTable,
}

impl StringTableContext {
  pub fn new() -> Self {
    Self {
      table: StringTable::new(),
    }
  }

  pub fn collect_strings<T: CollectStrings>(&mut self, value: &T) {
    value.collect_strings(self);
  }

  pub fn insert(&mut self, s: &str) -> u32 {
    self.table.insert(s)
  }
}

// Trait for collecting strings from AST nodes
pub trait CollectStrings {
  fn collect_strings(&self, ctx: &mut StringTableContext);
}

// Trait for serializing with string indices
pub trait SerializeWithStringTable {
  fn serialize_with_string_table<S>(&self, ctx: &StringTableContext, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: Serializer;
}

// Trait for deserializing with string table
pub trait DeserializeWithStringTable<'de>: Sized {
  fn deserialize_with_string_table<D>(table: &StringTable, deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>;
}

#[derive(Debug, Clone)]
pub struct RcBytesWrapper(Rc<Cow<'static, [u8]>>);

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
    struct BytesVisitor;

    impl<'de> serde::de::Visitor<'de> for BytesVisitor {
      type Value = RcBytesWrapper;

      fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a byte array")
      }

      fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
      where
        E: serde::de::Error,
      {
        Ok(RcBytesWrapper(Rc::new(Cow::Owned(v.to_vec()))))
      }

      fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Self::Value, E>
      where
        E: serde::de::Error,
      {
        Ok(RcBytesWrapper(Rc::new(Cow::Owned(v))))
      }

      fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
      where
        A: serde::de::SeqAccess<'de>,
      {
        let mut bytes = Vec::new();
        while let Some(byte) = seq.next_element()? {
          bytes.push(byte);
        }
        Ok(RcBytesWrapper(Rc::new(Cow::Owned(bytes))))
      }
    }

    deserializer.deserialize_bytes(BytesVisitor)
  }
}

impl RcBytesWrapper {
  pub fn new<S: Into<Cow<'static, [u8]>>>(s: S) -> Self {
    RcBytesWrapper(Rc::new(s.into()))
  }

  pub fn to_vec(&self) -> Vec<u8> {
    self.0.to_vec()
  }

  pub fn as_slice(&self) -> &[u8] {
    &self.0
  }

  pub fn to_mut(&mut self) -> &mut Vec<u8> {
    let cow = Rc::make_mut(&mut self.0);
    cow.to_mut()
  }
}

impl From<Cow<'static, [u8]>> for RcBytesWrapper {
  fn from(s: Cow<'static, [u8]>) -> Self {
    RcBytesWrapper::new(s)
  }
}

impl From<Vec<u8>> for RcBytesWrapper {
  fn from(s: Vec<u8>) -> Self {
    RcBytesWrapper::new(Cow::Owned(s))
  }
}

impl PartialEq for RcBytesWrapper {
  fn eq(&self, other: &RcBytesWrapper) -> bool {
    self.0 == other.0
  }
}

impl Eq for RcBytesWrapper {}

impl PartialEq<[u8]> for RcBytesWrapper {
  fn eq(&self, other: &[u8]) -> bool {
    *self.0 == other
  }
}

impl Hash for RcBytesWrapper {
  fn hash<H: Hasher>(&self, state: &mut H) {
    self.0.hash(state)
  }
}

impl fmt::Display for RcBytesWrapper {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    write!(f, "{:?}", self.0)
  }
}

impl Deref for RcBytesWrapper {
  type Target = [u8];
  fn deref(&self) -> &Self::Target {
    &self.0
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

impl CollectStrings for RcStrWrapper {
  fn collect_strings(&self, ctx: &mut StringTableContext) {
    ctx.insert(&self.0);
  }
}

impl SerializeWithStringTable for RcStrWrapper {
  fn serialize_with_string_table<S>(&self, ctx: &StringTableContext, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: Serializer,
  {
    // Find the index of this string in the table
    let index = ctx.table.indices.get(&**self.0).unwrap();
    serializer.serialize_u32(*index)
  }
}

impl<'de> DeserializeWithStringTable<'de> for RcStrWrapper {
  fn deserialize_with_string_table<D>(table: &StringTable, deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    let index = u32::deserialize(deserializer)?;
    match table.get(index) {
      Some(s) => Ok(RcStrWrapper(Rc::new(Cow::Owned(s.to_string())))),
      None => Err(serde::de::Error::custom(format!("Invalid string table index: {}", index))),
    }
  }
}

impl RcStrWrapper {
  pub fn new<S: Into<Cow<'static, str>>>(s: S) -> Self {
    RcStrWrapper(Rc::new(s.into()))
  }

  pub fn from_const(s: &'static str) -> Self {
    RcStrWrapper(Rc::new(Cow::Borrowed(s)))
  }

  pub fn to_string(&self) -> String {
    self.0.to_string()
  }

  pub fn as_str(&self) -> &str {
    &self.0
  }

  pub fn to_mut(&mut self) -> &mut String {
    let cow = Rc::make_mut(&mut self.0);
    cow.to_mut()
  }
}

impl<'a> RcStrWrapper {
  pub fn new_clone(s: &'a str) -> Self {
    RcStrWrapper::new(Cow::Owned(s.to_string()))
  }
}

impl From<String> for RcStrWrapper {
  fn from(s: String) -> Self {
    RcStrWrapper::new(Cow::Owned(s))
  }
}

impl From<Cow<'static, str>> for RcStrWrapper {
  fn from(s: Cow<'static, str>) -> Self {
    RcStrWrapper::new(s)
  }
}

impl From<&'static str> for RcStrWrapper {
  fn from(s: &'static str) -> Self {
    RcStrWrapper::from_const(s)
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
    let named_param_encountered = self.params.iter().take(index + 1).any(|p| p.name.is_some());

    if named_param_encountered {
      // If we've encountered a named parameter up to this index, we only look for the parameter by name
      self
        .params
        .iter()
        .find(|param| param.name.as_deref() == Some(param_name))
    } else if index < self.params.len() {
      // If no named parameters encountered and index is valid, return the parameter at that index
      Some(&self.params[index])
    } else {
      // If index is out of bounds, we look for a parameter with the given name
      self
        .params
        .iter()
        .find(|param| param.name.as_deref() == Some(param_name))
    }
  }
}

pub struct ParamHelperMut<'a> {
  params: &'a mut [Param],
}

impl<'a> ParamHelperMut<'a> {
  pub fn new(params: &'a mut [Param]) -> Self {
    Self { params }
  }

  pub fn get_param_by_name_or_index_mut(
    &mut self,
    param_name: &str,
    index: usize,
  ) -> Option<&mut Param> {
    let named_param_encountered = self.params.iter().take(index + 1).any(|p| p.name.is_some());

    if named_param_encountered {
      // If we've encountered a named parameter up to this index, we only look for the parameter by name
      self
        .params
        .iter_mut()
        .find(|param| param.name.as_deref() == Some(param_name))
    } else if index < self.params.len() {
      // If no named parameters encountered and index is valid, return the parameter at that index
      Some(&mut self.params[index])
    } else {
      // If index is out of bounds, we look for a parameter with the given name
      self
        .params
        .iter_mut()
        .find(|param| param.name.as_deref() == Some(param_name))
    }
  }
}

pub trait ShardsExtension {
  fn name(&self) -> &str;
  fn process_to_var(
    &self,
    func: &Function,
    line_info: LineInfo,
  ) -> Result<ClonedVar, ShardsError>;
  fn process_to_shard(
    &self,
    func: &Function,
    line_info: LineInfo,
  ) -> Result<AutoShardRef, ShardsError>;
}

// Helper functions for string table serialization
pub fn serialize_with_string_table<T>(data: &T) -> Result<Vec<u8>, serde_json::Error>
where
  T: CollectStrings + Serialize,
{
  // First pass: collect all strings to build the table
  let mut ctx = StringTableContext::new();
  ctx.collect_strings(data);
  
  // For now, we use a simple approach: serialize the data normally but include the string table
  // This still provides compression benefits by deduplicating the string table itself
  let wrapper = StringTableSerialization {
    string_table: ctx.table,
    data: serde_json::to_vec(data)?,
  };
  
  serde_json::to_vec(&wrapper)
}

pub fn deserialize_with_string_table<T>(data: &[u8]) -> Result<T, serde_json::Error>
where
  T: for<'de> Deserialize<'de>,
{
  let wrapper: StringTableSerialization<Vec<u8>> = serde_json::from_slice(data)?;
  serde_json::from_slice(&wrapper.data)
}

// Convenience methods for common use cases
impl Program {
  /// Serialize this program with string table optimization
  pub fn serialize_optimized(&self) -> Result<Vec<u8>, serde_json::Error> {
    // Simple serialization with string table analysis
    let mut ctx = StringTableContext::new();
    ctx.collect_strings(self);
    
    let wrapper = StringTableSerialization {
      string_table: ctx.table,
      data: serde_json::to_vec(self)?,
    };
    
    serde_json::to_vec(&wrapper)
  }
  
  /// Deserialize a program from string table optimized format
  pub fn deserialize_optimized(data: &[u8]) -> Result<Self, serde_json::Error> {
    deserialize_with_string_table(data)
  }
  
  /// Get statistics about string usage in this program
  pub fn string_statistics(&self) -> StringTableStats {
    let mut ctx = StringTableContext::new();
    ctx.collect_strings(self);
    
    let total_chars: usize = ctx.table.strings.iter().map(|s| s.len()).sum();
    let unique_strings = ctx.table.strings.len();
    
    StringTableStats {
      unique_strings,
      total_characters: total_chars,
      average_string_length: if unique_strings > 0 { total_chars as f64 / unique_strings as f64 } else { 0.0 },
      string_table: ctx.table,
    }
  }
}

#[derive(Debug, Clone)]
pub struct StringTableStats {
  pub unique_strings: usize,
  pub total_characters: usize,
  pub average_string_length: f64,
  pub string_table: StringTable,
}

impl fmt::Display for StringTableStats {
  fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    writeln!(f, "String Table Statistics:")?;
    writeln!(f, "  Unique strings: {}", self.unique_strings)?;
    writeln!(f, "  Total characters: {}", self.total_characters)?;
    writeln!(f, "  Average string length: {:.2}", self.average_string_length)?;
    
    if self.unique_strings <= 20 {
      writeln!(f, "  Strings:")?;
      for (i, s) in self.string_table.strings.iter().enumerate() {
        writeln!(f, "    {}: {:?}", i, s)?;
      }
    } else {
      writeln!(f, "  Top 10 strings:")?;
      for (i, s) in self.string_table.strings.iter().take(10).enumerate() {
        writeln!(f, "    {}: {:?}", i, s)?;
      }
      writeln!(f, "    ... and {} more", self.unique_strings - 10)?;
    }
    
    Ok(())
  }
}

#[cfg(test)]
mod string_table_tests {
  use super::*;
  use crate::ast::*;

  #[test]
  fn test_string_table_creation() {
    let mut table = StringTable::new();
    
    let idx1 = table.insert("hello");
    let idx2 = table.insert("world");
    let idx3 = table.insert("hello"); // Duplicate
    
    assert_eq!(idx1, 0);
    assert_eq!(idx2, 1);
    assert_eq!(idx3, 0); // Should return the same index as the first "hello"
    
    assert_eq!(table.get(0), Some("hello"));
    assert_eq!(table.get(1), Some("world"));
    assert_eq!(table.len(), 2);
  }

  #[test]
  fn test_string_collection_from_ast() {
    // Create a simple AST with some duplicate strings
    let identifier = Identifier {
      name: RcStrWrapper::from("test_function"),
      namespaces: vec![
        RcStrWrapper::from("namespace1"),
        RcStrWrapper::from("namespace2"),
        RcStrWrapper::from("namespace1"), // Duplicate
      ],
      custom_state: CustomStateContainer::new(),
    };

    let mut ctx = StringTableContext::new();
    identifier.collect_strings(&mut ctx);

    // Should have 3 unique strings despite 4 total strings
    assert_eq!(ctx.table.len(), 3);
    assert!(ctx.table.indices.contains_key("test_function"));
    assert!(ctx.table.indices.contains_key("namespace1"));
    assert!(ctx.table.indices.contains_key("namespace2"));
  }

  #[test]
  fn test_program_string_statistics() {
    // Create a program with duplicate identifiers
    let function1 = Function {
      name: Identifier {
        name: RcStrWrapper::from("Get"),
        namespaces: vec![],
        custom_state: CustomStateContainer::new(),
      },
      params: None,
      custom_state: CustomStateContainer::new(),
    };

    let function2 = Function {
      name: Identifier {
        name: RcStrWrapper::from("Get"), // Duplicate name
        namespaces: vec![],
        custom_state: CustomStateContainer::new(),
      },
      params: None,
      custom_state: CustomStateContainer::new(),
    };

    let block1 = Block {
      content: BlockContent::Shard(function1),
      line_info: None,
      custom_state: CustomStateContainer::new(),
    };

    let block2 = Block {
      content: BlockContent::Shard(function2),
      line_info: None,
      custom_state: CustomStateContainer::new(),
    };

    let pipeline = Pipeline {
      blocks: vec![block1, block2],
    };

    let statement = Statement::Pipeline(pipeline);

    let sequence = Sequence {
      statements: vec![statement],
      custom_state: CustomStateContainer::new(),
    };

    let metadata = Metadata {
      name: RcStrWrapper::from("test_program"),
      debug_info: RefCell::new(DebugInfo::default()),
    };

    let program = Program {
      sequence,
      metadata,
    };

    let stats = program.string_statistics();
    
    // Should have only 2 unique strings: "Get" and "test_program"
    assert_eq!(stats.unique_strings, 2);
    assert!(stats.string_table.indices.contains_key("Get"));
    assert!(stats.string_table.indices.contains_key("test_program"));
    
    println!("{}", stats);
  }
}
