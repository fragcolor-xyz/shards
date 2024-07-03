#![allow(non_upper_case_globals)]

use core::fmt;
use std::any::Any;
use std::fmt::Debug;

use pest::Position;
use serde::{Deserialize, Serialize};
use shards::{
  types::Var, SHType_Bool, SHType_Bytes, SHType_Float, SHType_Int, SHType_None, SHType_String,
};

use crate::{RcBytesWrapper, RcStrWrapper};

#[derive(Parser)]
#[grammar = "shards.pest"]
pub struct ShardsParser;

/// CustomAny trait allows for flexible, type-erased storage of custom state within AST nodes.
/// This approach was chosen to balance performance, flexibility, and simplicity.
///
/// Benefits:
/// - Direct access to custom state without additional lookups
/// - Simplifies state management by keeping it close to related data
/// - Minimizes memory overhead through use of Box<dyn CustomAny>
///
/// Considerations:
/// - Adds non-structural data to the AST
/// - May require special handling during serialization/deserialization
///
/// Alternative considered: External state storage with UUID references in AST nodes.
/// Current approach preferred due to reduced lookup overhead and simpler implementation.
pub trait CustomAny: Any + Debug + CustomClone + CustomPartialEq {
  fn as_any(&self) -> &dyn Any;
  fn as_any_mut(&mut self) -> &mut dyn Any;
}

pub trait CustomClone {
  fn clone_box(&self) -> Box<dyn CustomAny>;
}

impl Clone for Box<dyn CustomAny> {
  fn clone(&self) -> Box<dyn CustomAny> {
    self.clone_box()
  }
}

pub trait CustomPartialEq {
  fn eq_box(&self, other: &dyn CustomAny) -> bool;
}

impl PartialEq for Box<dyn CustomAny> {
  fn eq(&self, other: &Self) -> bool {
    self.eq_box(other.as_ref())
  }
}

// Macro to implement CustomAny for common types
#[macro_export]
macro_rules! impl_custom_any {
  ($($t:ty),*) => {
      $(
          impl CustomClone for $t {
              fn clone_box(&self) -> Box<dyn CustomAny> {
                  Box::new(self.clone())
              }
          }

          impl CustomPartialEq for $t {
              fn eq_box(&self, other: &dyn CustomAny) -> bool {
                  other
                      .as_any()
                      .downcast_ref::<$t>()
                      .map_or(false, |a| self == a)
              }
          }

          impl CustomAny for $t {
              fn as_any(&self) -> &dyn Any {
                  self
              }

              fn as_any_mut(&mut self) -> &mut dyn Any {
                self
              }
          }
      )*
  };
}

macro_rules! impl_custom_state {
  ($type:ident) => {
    impl $type {
      /// Sets custom state for this item.
      /// This method allows attaching arbitrary data to AST nodes,
      /// enabling flexible extensions without modifying the core AST structure.
      pub fn set_custom_state<T: 'static + CustomAny>(&mut self, state: T) {
        self.custom_state = Some(Box::new(state));
      }

      /// Gets a mutable reference to the custom state of a specific type, if it exists.
      pub fn get_custom_state<T: 'static + CustomAny>(&mut self) -> Option<&mut T> {
        self
          .custom_state
          .as_mut()
          .and_then(|state| state.as_any_mut().downcast_mut::<T>())
      }

      /// Uses a custom state of a specific type, if it exists within a closure.
      /// This method simplifies working with custom state by providing a safe, ergonomic API.
      pub fn with_custom_state<T: 'static, R, F>(&mut self, f: F) -> Option<R>
      where
        T: CustomAny,
        F: FnOnce(&mut T) -> R,
      {
        self.get_custom_state::<T>().map(|state| f(state))
      }

      /// Gets or inserts custom state of a specific type.
      /// If the state doesn't exist, it's created using the provided default function.
      pub fn get_or_insert_custom_state<T: 'static + CustomAny, F>(&mut self, default: F) -> &mut T
      where
        F: FnOnce() -> T,
      {
        if self.custom_state.is_none() {
          self.custom_state = Some(Box::new(default()));
        }

        self.get_custom_state().unwrap()
      }

      /// Clears all custom state from this item.
      /// Useful when a "clean" AST representation is needed, free of runtime-specific data.
      pub fn clear_custom_state(&mut self) {
        self.custom_state = None;
      }
    }
  };
}

#[derive(Debug, Copy, Clone, Default, PartialEq)]
pub struct LineInfo {
  pub line: u32,
  pub column: u32,
}

#[derive(Debug)]
pub struct ShardsError {
  pub message: String,
  pub loc: LineInfo,
}

impl<'a> Into<ShardsError> for (&str, Position<'a>) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    let line = pos.line_col().0;
    let column = pos.line_col().1;
    ShardsError {
      message: message.to_string(),
      loc: LineInfo {
        line: line as u32,
        column: column as u32,
      },
    }
  }
}

impl<'a> Into<ShardsError> for (String, Position<'a>) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    let line = pos.line_col().0;
    let column = pos.line_col().1;
    ShardsError {
      message,
      loc: LineInfo {
        line: line as u32,
        column: column as u32,
      },
    }
  }
}

impl<'a> Into<ShardsError> for (&str, LineInfo) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    ShardsError {
      message: message.to_string(),
      loc: pos,
    }
  }
}

impl<'a> Into<ShardsError> for (String, LineInfo) {
  fn into(self) -> ShardsError {
    let (message, pos) = self;
    ShardsError { message, loc: pos }
  }
}

impl<'a> Into<LineInfo> for Position<'a> {
  fn into(self) -> LineInfo {
    let line = self.line_col().0;
    let column = self.line_col().1;
    LineInfo {
      line: line as u32,
      column: column as u32,
    }
  }
}

impl Into<(u32, u32)> for LineInfo {
  fn into(self) -> (u32, u32) {
    (self.line, self.column)
  }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
#[serde(rename = "Num")]
pub enum Number {
  #[serde(rename = "int")]
  Integer(i64),
  #[serde(rename = "float")]
  Float(f64),
  #[serde(rename = "hex")]
  Hexadecimal(RcStrWrapper),
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Hash, Eq)]
#[serde(rename = "Iden")]
pub struct Identifier {
  pub name: RcStrWrapper,
  #[serde(default)]
  #[serde(rename = "ns")]
  #[serde(skip_serializing_if = "Vec::is_empty")]
  pub namespaces: Vec<RcStrWrapper>,
}

impl Identifier {
  pub fn resolve(&self) -> RcStrWrapper {
    if self.namespaces.is_empty() {
      return self.name.clone();
    } else {
      // go thru all namespaces and concatenate them with "/" finally add name
      let mut result = String::new();
      for namespace in &self.namespaces {
        result.push_str(&namespace);
        result.push('/');
      }
      result.push_str(&self.name);
      result.into()
    }
  }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Value {
  #[serde(rename = "none")]
  None,
  #[serde(rename = "id")]
  Identifier(Identifier),
  #[serde(rename = "bool")]
  Boolean(bool),
  #[serde(rename = "enum")]
  Enum(RcStrWrapper, RcStrWrapper),
  #[serde(rename = "num")]
  Number(Number),
  #[serde(rename = "str")]
  String(RcStrWrapper),
  #[serde(rename = "bytes")]
  Bytes(RcBytesWrapper),
  #[serde(rename = "i2")]
  Int2([i64; 2]),
  #[serde(rename = "i3")]
  Int3([i32; 3]),
  #[serde(rename = "i4")]
  Int4([i32; 4]),
  #[serde(rename = "i8")]
  Int8([i16; 8]),
  #[serde(rename = "i16")]
  Int16([i8; 16]),
  #[serde(rename = "f2")]
  Float2([f64; 2]),
  #[serde(rename = "f3")]
  Float3([f32; 3]),
  #[serde(rename = "f4")]
  Float4([f32; 4]),
  #[serde(rename = "seq")]
  Seq(Vec<Value>),
  #[serde(rename = "table")]
  Table(Vec<(Value, Value)>),
  #[serde(rename = "sh")]
  Shard(Function),
  #[serde(rename = "shs")]
  Shards(Sequence),
  #[serde(rename = "eExpr")]
  EvalExpr(Sequence),
  #[serde(rename = "expr")]
  Expr(Sequence),
  #[serde(rename = "tt")]
  TakeTable(Identifier, Vec<RcStrWrapper>),
  #[serde(rename = "ts")]
  TakeSeq(Identifier, Vec<u32>),
  #[serde(rename = "func")]
  Func(Function),
}

impl TryFrom<Var> for Value {
  type Error = &'static str;

  fn try_from(value: Var) -> Result<Self, Self::Error> {
    match value.valueType {
      SHType_None => Ok(Value::None),
      SHType_Bool => Ok(Value::Boolean(value.as_ref().try_into().unwrap())),
      SHType_Int => Ok(Value::Number(Number::Integer(
        value.as_ref().try_into().unwrap(),
      ))),
      SHType_Float => Ok(Value::Number(Number::Float(
        value.as_ref().try_into().unwrap(),
      ))),
      SHType_String => {
        let s: &str = value.as_ref().try_into().unwrap();
        Ok(Value::String(s.into()))
      }
      SHType_Bytes => {
        let b: &[u8] = value.as_ref().try_into().unwrap();
        Ok(Value::Bytes(b.into()))
      }
      _ => Err("Unsupported type"),
    }
  }
}

impl Value {
  pub fn get_identifier(&self) -> Option<&Identifier> {
    match self {
      Value::Identifier(identifier) => Some(identifier),
      _ => None,
    }
  }
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Param {
  #[serde(skip_serializing_if = "Option::is_none")]
  pub name: Option<RcStrWrapper>,
  #[serde(flatten)]
  pub value: Value,

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  #[serde(skip)]
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Param);

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Function {
  #[serde(flatten)]
  pub name: Identifier,
  #[serde(skip_serializing_if = "Option::is_none")]
  pub params: Option<Vec<Param>>,

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  #[serde(skip)]
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Function);

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum BlockContent {
  #[serde(rename = "none")]
  Empty,
  #[serde(rename = "sh")]
  Shard(Function), // Rule: Shard
  #[serde(rename = "shs")]
  Shards(Sequence), // Rule: Shards
  #[serde(rename = "const")]
  Const(Value), // Rules: ConstValue, Vector
  #[serde(rename = "tt")]
  TakeTable(Identifier, Vec<RcStrWrapper>), // Rule: TakeTable
  #[serde(rename = "ts")]
  TakeSeq(Identifier, Vec<u32>), // Rule: TakeSeq
  #[serde(rename = "eExpr")]
  EvalExpr(Sequence), // Rule: EvalExpr
  #[serde(rename = "expr")]
  Expr(Sequence), // Rule: Expr
  #[serde(rename = "func")]
  Func(Function), // Rule: BuiltIn
  #[serde(rename = "prog")]
  Program(Program), // @include files, this is a sequence that will include itself when evaluated
}

#[derive(Debug, Clone, PartialEq)]
pub struct Block {
  pub content: BlockContent,
  pub line_info: Option<LineInfo>,

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Block);

#[derive(Debug, Clone, PartialEq)]
pub struct Pipeline {
  pub blocks: Vec<Block>,
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub enum Assignment {
  #[serde(rename = "ref")]
  AssignRef(Pipeline, Identifier),
  #[serde(rename = "set")]
  AssignSet(Pipeline, Identifier),
  #[serde(rename = "upd")]
  AssignUpd(Pipeline, Identifier),
  #[serde(rename = "push")]
  AssignPush(Pipeline, Identifier),
}

#[derive(Debug, Clone, PartialEq)]
pub enum Statement {
  Assignment(Assignment),
  Pipeline(Pipeline),
}

#[derive(Serialize, Deserialize, Debug, Clone, PartialEq)]
pub struct Metadata {
  pub name: RcStrWrapper,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Sequence {
  pub statements: Vec<Statement>,

  /// Custom state for UI or other runtime-specific data.
  /// Stored directly in the AST node for efficient access and simpler management.
  /// Uses Box<dyn CustomAny> to minimize memory overhead when unused.
  pub custom_state: Option<Box<dyn CustomAny>>,
}

impl_custom_state!(Sequence);

#[derive(Debug, Clone, PartialEq)]
pub struct Program {
  pub sequence: Sequence,
  pub metadata: Metadata,
}

pub trait RewriteFunction {
  fn rewrite_function(&self, function: &Function) -> Option<Function>;
}

impl Serialize for Sequence {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: serde::Serializer,
  {
    self.statements.serialize(serializer)
  }
}

impl<'de> Deserialize<'de> for Sequence {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: serde::Deserializer<'de>,
  {
    let statements = Vec::deserialize(deserializer)?;
    Ok(Sequence {
      statements,
      custom_state: None,
    })
  }
}

impl Serialize for Program {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: serde::Serializer,
  {
    use serde::ser::SerializeStruct;
    let mut state = serializer.serialize_struct("Program", 2)?;
    state.serialize_field("metadata", &self.metadata)?;
    state.serialize_field("sequence", &self.sequence.statements)?;
    state.end()
  }
}

impl<'de> Deserialize<'de> for Program {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: serde::Deserializer<'de>,
  {
    #[derive(Deserialize)]
    struct ProgramHelper {
      metadata: Metadata,
      sequence: Vec<Statement>,
    }

    let helper = ProgramHelper::deserialize(deserializer)?;
    Ok(Program {
      metadata: helper.metadata,
      sequence: Sequence {
        statements: helper.sequence,
        custom_state: None,
      },
    })
  }
}

// Custom serialization for Pipeline
impl Serialize for Pipeline {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: serde::Serializer,
  {
    // Directly serialize the blocks vector
    self.blocks.serialize(serializer)
  }
}

// Custom deserialization for Pipeline
impl<'de> Deserialize<'de> for Pipeline {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: serde::Deserializer<'de>,
  {
    // Directly deserialize into a Vec<Block>
    let blocks = Vec::<Block>::deserialize(deserializer)?;
    Ok(Pipeline { blocks })
  }
}

// Custom serialization for Statement
impl Serialize for Statement {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: serde::Serializer,
  {
    match self {
      Statement::Assignment(assignment) => assignment.serialize(serializer),
      Statement::Pipeline(pipeline) => pipeline.serialize(serializer),
    }
  }
}

use serde::de::{self, Deserializer, MapAccess, SeqAccess, Visitor};

impl<'de> Deserialize<'de> for Statement {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    struct StatementVisitor;

    impl<'de> Visitor<'de> for StatementVisitor {
      type Value = Statement;

      fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("a map (for Assignment) or a sequence (for Pipeline)")
      }

      fn visit_seq<V>(self, seq: V) -> Result<Self::Value, V::Error>
      where
        V: SeqAccess<'de>,
      {
        let pipeline = Pipeline::deserialize(de::value::SeqAccessDeserializer::new(seq))?;
        Ok(Statement::Pipeline(pipeline))
      }

      fn visit_map<M>(self, map: M) -> Result<Self::Value, M::Error>
      where
        M: MapAccess<'de>,
      {
        let assignment = Assignment::deserialize(de::value::MapAccessDeserializer::new(map))?;
        Ok(Statement::Assignment(assignment))
      }
    }

    deserializer.deserialize_any(StatementVisitor)
  }
}

impl Serialize for Block {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: serde::Serializer,
  {
    use serde::ser::SerializeStruct;
    let mut state = serializer.serialize_struct("Block", 2)?;

    // Flatten the content
    match &self.content {
      BlockContent::Empty => state.serialize_field("none", &()),
      BlockContent::Shard(func) => state.serialize_field("sh", func),
      BlockContent::Shards(seq) => state.serialize_field("shs", seq),
      BlockContent::Const(val) => state.serialize_field("const", val),
      BlockContent::TakeTable(id, vec) => state.serialize_field("tt", &(id, vec)),
      BlockContent::TakeSeq(id, vec) => state.serialize_field("ts", &(id, vec)),
      BlockContent::EvalExpr(seq) => state.serialize_field("eExpr", seq),
      BlockContent::Expr(seq) => state.serialize_field("expr", seq),
      BlockContent::Func(func) => state.serialize_field("func", func),
      BlockContent::Program(prog) => state.serialize_field("prog", prog),
    }?;

    state.end()
  }
}

impl<'de> Deserialize<'de> for Block {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    enum Field {
      None,
      Sh,
      Shs,
      Const,
      Tt,
      Ts,
      EExpr,
      Expr,
      Func,
      Prog,
      LineInfo,
    }

    impl<'de> Deserialize<'de> for Field {
      fn deserialize<D>(deserializer: D) -> Result<Field, D::Error>
      where
        D: Deserializer<'de>,
      {
        struct FieldVisitor;

        impl<'de> Visitor<'de> for FieldVisitor {
          type Value = Field;

          fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            formatter.write_str("`none`, `sh`, `shs`, `const`, `tt`, `ts`, `eExpr`, `expr`, `func`, `prog`, or `line_info`")
          }

          fn visit_str<E>(self, value: &str) -> Result<Field, E>
          where
            E: de::Error,
          {
            match value {
              "none" => Ok(Field::None),
              "sh" => Ok(Field::Sh),
              "shs" => Ok(Field::Shs),
              "const" => Ok(Field::Const),
              "tt" => Ok(Field::Tt),
              "ts" => Ok(Field::Ts),
              "eExpr" => Ok(Field::EExpr),
              "expr" => Ok(Field::Expr),
              "func" => Ok(Field::Func),
              "prog" => Ok(Field::Prog),
              "line_info" => Ok(Field::LineInfo),
              _ => Err(de::Error::unknown_field(value, FIELDS)),
            }
          }
        }

        deserializer.deserialize_identifier(FieldVisitor)
      }
    }

    struct BlockVisitor;

    impl<'de> Visitor<'de> for BlockVisitor {
      type Value = Block;

      fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("struct Block")
      }

      fn visit_map<V>(self, mut map: V) -> Result<Block, V::Error>
      where
        V: MapAccess<'de>,
      {
        let mut content = None;
        let line_info = None;

        while let Some(key) = map.next_key()? {
          match key {
            Field::None => {
              map.next_value::<()>()?;
              content = Some(BlockContent::Empty);
            }
            Field::Sh => {
              let value = map.next_value()?;
              content = Some(BlockContent::Shard(value));
            }
            Field::Shs => {
              let value = map.next_value()?;
              content = Some(BlockContent::Shards(value));
            }
            Field::Const => {
              let value = map.next_value()?;
              content = Some(BlockContent::Const(value));
            }
            Field::Tt => {
              let (id, vec): (Identifier, Vec<RcStrWrapper>) = map.next_value()?;
              content = Some(BlockContent::TakeTable(id, vec));
            }
            Field::Ts => {
              let (id, vec): (Identifier, Vec<u32>) = map.next_value()?;
              content = Some(BlockContent::TakeSeq(id, vec));
            }
            Field::EExpr => {
              let value = map.next_value()?;
              content = Some(BlockContent::EvalExpr(value));
            }
            Field::Expr => {
              let value = map.next_value()?;
              content = Some(BlockContent::Expr(value));
            }
            Field::Func => {
              let value = map.next_value()?;
              content = Some(BlockContent::Func(value));
            }
            Field::Prog => {
              let value = map.next_value()?;
              content = Some(BlockContent::Program(value));
            }
            Field::LineInfo => {}
          }
        }

        let content = content.ok_or_else(|| de::Error::missing_field("content"))?;

        Ok(Block {
          content,
          line_info,
          custom_state: None,
        })
      }
    }

    const FIELDS: &[&str] = &[
      "none", "sh", "shs", "const", "tt", "ts", "eExpr", "expr", "func", "prog",
    ];
    deserializer.deserialize_struct("Block", FIELDS, BlockVisitor)
  }
}
