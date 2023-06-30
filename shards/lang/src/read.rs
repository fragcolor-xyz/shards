use crate::{ast::*, RcStrWrapper};
use core::convert::TryInto;
use pest::iterators::Pair;
use pest::Parser;
use std::collections::HashSet;
use std::path::Path;

pub(crate) struct ReadEnv {
  directory: String,
  included: HashSet<RcStrWrapper>,
  parent: Option<*const ReadEnv>,
}

impl ReadEnv {
  pub(crate) fn new(directory: &str) -> Self {
    Self {
      directory: directory.to_string(),
      included: HashSet::new(),
      parent: None,
    }
  }
}

fn check_included<'a>(name: &'a RcStrWrapper, env: &'a ReadEnv) -> bool {
  if env.included.contains(name) {
    true
  } else if let Some(parent) = env.parent {
    check_included(name, unsafe { &*parent })
  } else {
    false
  }
}

fn process_assignment(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Assignment, ShardsError> {
  let pos = pair.as_span().start_pos();
  if pair.as_rule() != Rule::Assignment {
    return Err(
      (
        "Expected an Assignment rule, but found a different rule.",
        pos,
      )
        .into(),
    );
  }

  let mut inner = pair.into_inner();
  let sub_pair = inner
    .next()
    .ok_or(("Expected a sub-pair in Assignment, but found none.", pos).into())?;
  let sub_rule = sub_pair.as_rule();

  let mut sub_inner = sub_pair.into_inner();
  let pipeline = process_pipeline(
    sub_inner
      .next()
      .ok_or(("Expected a Pipeline in Assignment, but found none.", pos).into())?,
    env,
  )?;

  let identifier = sub_inner
    .next()
    .ok_or(("Expected an identifier in Assignment, but found none.", pos).into())?
    .as_str();
  let identifier = RcStrWrapper::new(identifier);

  match sub_rule {
    Rule::AssignRef => Ok(Assignment::AssignRef(pipeline, identifier)),
    Rule::AssignSet => Ok(Assignment::AssignSet(pipeline, identifier)),
    Rule::AssignUpd => Ok(Assignment::AssignUpd(pipeline, identifier)),
    Rule::AssignPush => Ok(Assignment::AssignPush(pipeline, identifier)),
    _ => Err(("Unexpected sub-rule in Assignment.", pos).into()),
  }
}

fn process_vector(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Value, ShardsError> {
  assert_eq!(pair.as_rule(), Rule::Vector);
  let pos = pair.as_span().start_pos();
  let inner = pair.into_inner();
  let mut values = Vec::new();
  for pair in inner {
    let pos = pair.as_span().start_pos();
    match pair.as_rule() {
      Rule::Number => values.push(process_number(
        pair
          .into_inner()
          .next()
          .ok_or(("Expected a number", pos).into())?,
        env,
      )?),
      _ => return Err(("Unexpected rule in Vector.", pos).into()),
    }
  }
  // now check that all the values are the same type
  // and if so, return a Const with a vector value
  // now, vectors are always 2+ values, so we can safely check first
  let first = values.first().unwrap(); // qed
  let is_int = match first {
    Number::Float(_) => false,
    _ => true,
  };
  if is_int {
    match values.len() {
      2 => {
        let mut vec = Vec::new();
        for value in values {
          match value {
            Number::Integer(i) => vec.push(i),
            _ => unreachable!(),
          }
        }
        Ok(Value::Int2(vec.try_into().unwrap())) // qed
      }
      3 => {
        let mut vec: Vec<i32> = Vec::new();
        for value in values {
          match value {
            Number::Integer(i) => vec.push(i.try_into().map_err(|_| {
              (
                "Expected a signed integer that fits in 32 bits, but found one that doesn't.",
                pos,
              )
                .into()
            })?),
            _ => unreachable!(),
          }
        }
        Ok(Value::Int3(vec.try_into().unwrap())) // qed
      }
      4 => {
        let mut vec: Vec<i32> = Vec::new();
        for value in values {
          match value {
            Number::Integer(i) => vec.push(i.try_into().map_err(|_| {
              (
                "Expected a signed integer that fits in 32 bits, but found one that doesn't.",
                pos,
              )
                .into()
            })?),
            _ => unreachable!(),
          }
        }
        Ok(Value::Int4(vec.try_into().unwrap())) // qed
      }
      8 => {
        let mut vec: Vec<i16> = Vec::new();
        for value in values {
          match value {
            Number::Integer(i) => vec.push(i.try_into().map_err(|_| {
              (
                "Expected a signed integer that fits in 16 bits, but found one that doesn't.",
                pos,
              )
                .into()
            })?),
            _ => unreachable!(),
          }
        }
        Ok(Value::Int8(vec.try_into().unwrap())) // qed
      }
      16 => {
        let mut vec: Vec<i8> = Vec::new();
        for value in values {
          match value {
            Number::Integer(i) => vec.push(i.try_into().map_err(|_| {
              (
                "Expected a signed integer that fits in 8 bits, but found one that doesn't.",
                pos,
              )
                .into()
            })?),
            _ => unreachable!(),
          }
        }
        Ok(Value::Int16(vec.try_into().unwrap())) // qed
      }
      _ => Err(("Expected an int vector of 2, 3, 4, 8, or 16 numbers.", pos).into()),
    }
  } else {
    match values.len() {
      2 => {
        let mut vec = Vec::new();
        for value in values {
          match value {
            Number::Float(f) => vec.push(f),
            _ => unreachable!(),
          }
        }
        Ok(Value::Float2(vec.try_into().unwrap())) // qed
      }
      3 => {
        let mut vec: Vec<f32> = Vec::new();
        for value in values {
          match value {
            Number::Float(f) => vec.push(f as f32),
            _ => unreachable!(),
          }
        }
        Ok(Value::Float3(vec.try_into().unwrap())) // qed
      }
      4 => {
        let mut vec: Vec<f32> = Vec::new();
        for value in values {
          match value {
            Number::Float(f) => vec.push(f as f32),
            _ => unreachable!(),
          }
        }
        Ok(Value::Float4(vec.try_into().unwrap())) // qed
      }
      _ => Err(("Expected a float vector of 2, 3, 4 numbers.", pos).into()),
    }
  }
}

enum FunctionValue {
  Const(Value),
  Function(Function),
  Sequence(Sequence),
}

fn process_function(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<FunctionValue, ShardsError> {
  let pos = pair.as_span().start_pos();

  let mut inner = pair.into_inner();
  let exp = inner
    .next()
    .ok_or(("Expected a Name or Const in Shard", pos).into())?;

  let pos = exp.as_span().start_pos();

  match exp.as_rule() {
    Rule::UppIden | Rule::LowIden => {
      let name = exp.as_str();
      let next = inner.next();

      let params = match next {
        Some(pair) => {
          if pair.as_rule() == Rule::Params {
            Some(process_params(pair, env)?)
          } else {
            return Err(("Expected Params in Shard", pos).into());
          }
        }
        None => None,
      };

      match name {
        "include" => {
          let params = params.ok_or(("Expected 2 parameters", pos).into())?;
          let n_params = params.len();

          let file_name = if n_params > 0 && params[0].name.is_none() {
            Some(&params[0])
          } else {
            params
              .iter()
              .find(|param| param.name.as_deref() == Some("File"))
          };
          let file_name = file_name.ok_or(("Expected a file name (File:)", pos).into())?;
          let file_name = match &file_name.value {
            Value::String(s) => Ok(s),
            _ => Err(("Expected a string value for File", pos).into()),
          }?;

          let once = if n_params > 1 && params[0].name.is_none() && params[1].name.is_none() {
            Some(&params[1])
          } else {
            params
              .iter()
              .find(|param| param.name.as_deref() == Some("Once"))
          };

          let once = once
            .map(|param| match &param.value {
              Value::Boolean(b) => Ok(*b),
              _ => Err(("Expected a boolean value for Once", pos).into()),
            })
            .unwrap_or(Ok(false))?;

          if once && check_included(file_name, env) {
            return Err((format!("File {:?} already included", file_name), pos).into());
          }

          let parent = Path::new(&env.directory);
          let file_path = parent.join(&file_name.as_str());
          let file_path = std::fs::canonicalize(&file_path).map_err(|e| {
            (
              format!("Failed to canonicalize file {:?}: {}", file_name, e),
              pos,
            )
              .into()
          })?;

          println!("Including file {:?}", file_path);

          let rc_path = file_path
            .to_str()
            .ok_or(
              (
                format!("Failed to convert file path {:?} to string", file_path),
                pos,
              )
                .into(),
            )?
            .into();

          if once && check_included(&rc_path, env) {
            return Err((format!("File {:?} already included", file_name), pos).into());
          }

          env.included.insert(rc_path);

          // read string from file
          let code = std::fs::read_to_string(&file_path)
            .map_err(|e| (format!("Failed to read file {:?}: {}", file_path, e), pos).into())?;

          let parent = file_path.parent().unwrap_or(Path::new("."));
          let successful_parse = ShardsParser::parse(Rule::Program, &code)
            .map_err(|e| (format!("Failed to parse file {:?}: {}", file_path, e), pos).into())?;
          let mut sub_env: ReadEnv = ReadEnv::new(
            parent
              .to_str()
              .ok_or(
                (
                  format!("Failed to convert file path {:?} to string", parent),
                  pos,
                )
                  .into(),
              )?
              .into(),
          );
          sub_env.parent = Some(env);
          let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut sub_env)?;

          Ok(FunctionValue::Sequence(seq))
        }
        "read" => {
          let params = params.ok_or(("Expected 2 parameters", pos).into())?;
          let n_params = params.len();

          let file_name = if n_params > 0 && params[0].name.is_none() {
            Some(&params[0])
          } else {
            params
              .iter()
              .find(|param| param.name.as_deref() == Some("File"))
          };
          let file_name = file_name.ok_or(("Expected a file name (File:)", pos).into())?;
          let file_name = match &file_name.value {
            Value::String(s) => Ok(s),
            _ => Err(("Expected a string value for File", pos).into()),
          }?;

          let as_bytes = if n_params > 1 && params[0].name.is_none() && params[1].name.is_none() {
            Some(&params[1])
          } else {
            params
              .iter()
              .find(|param| param.name.as_deref() == Some("AsBytes"))
          };

          let as_bytes = as_bytes
            .map(|param| match &param.value {
              Value::Boolean(b) => Ok(*b),
              _ => Err(("Expected a boolean value for AsBytes", pos).into()),
            })
            .unwrap_or(Ok(false))?;

          let parent = Path::new(&env.directory);
          let file_path = parent.join(&file_name.as_str());
          if as_bytes {
            // read bytes from file
            let bytes = std::fs::read(&file_path)
              .map_err(|e| (format!("Failed to read file {:?}: {}", file_path, e), pos).into())?;
            Ok(FunctionValue::Const(Value::Bytes(bytes.into())))
          } else {
            // read string from file
            let string = std::fs::read_to_string(&file_path)
              .map_err(|e| (format!("Failed to read file {:?}: {}", file_path, e), pos).into())?;
            Ok(FunctionValue::Const(Value::String(string.into())))
          }
        }
        _ => Ok(FunctionValue::Function(Function {
          name: RcStrWrapper::new(name),
          params,
        })),
      }
    }
    _ => Err(
      (
        format!("Unexpected rule {:?} in Function.", exp.as_rule()),
        pos,
      )
        .into(),
    ),
  }
}

fn process_take_table(pair: Pair<Rule>, _env: &mut ReadEnv) -> (RcStrWrapper, Vec<RcStrWrapper>) {
  let str_expr = pair.as_str();
  // split by ':'
  let splits: Vec<_> = str_expr.split(':').collect();
  let var_name = splits[0];
  let keys: Vec<RcStrWrapper> = splits[1..].iter().map(|s| (*s).into()).collect();
  // wrap the shards into an Expr Sequence
  (var_name.into(), keys)
}

fn process_take_seq(
  pair: Pair<Rule>,
  _env: &mut ReadEnv,
) -> Result<(RcStrWrapper, Vec<u32>), ShardsError> {
  let pos = pair.as_span().start_pos();
  // do the same as TakeTable but with a sequence of values where index is an integer int32
  let str_expr = pair.as_str();
  // split by ':'
  let splits: Vec<_> = str_expr.split(':').collect();
  let var_name = splits[0];
  let keys: Result<Vec<u32>, _> = splits[1..].iter().map(|s| s.parse::<u32>()).collect();
  let keys = keys.map_err(|_| ("Failed to parse unsigned index integer", pos).into())?;
  // wrap the shards into an Expr Sequence
  Ok((var_name.into(), keys))
}

fn process_pipeline(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Pipeline, ShardsError> {
  let pos = pair.as_span().start_pos();
  if pair.as_rule() != Rule::Pipeline {
    return Err(("Expected a Pipeline rule, but found a different rule.", pos).into());
  }

  let mut blocks = Vec::new();

  for pair in pair.into_inner() {
    let pos = pair.as_span().start_pos();
    let rule = pair.as_rule();
    match rule {
      Rule::Vector => {
        // in this case we want a Const with a vector value
        let value = process_vector(pair, env)?;
        blocks.push(Block {
          content: BlockContent::Const(value),
          line_info: pos.into(),
        });
      }
      Rule::EvalExpr => blocks.push(Block {
        content: BlockContent::EvalExpr(process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(("Expected an eval time expression, but found none.", pos).into())?,
          env,
        )?),
        line_info: pos.into(),
      }),
      Rule::Expr => blocks.push(Block {
        content: BlockContent::Expr(process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(("Expected an expression, but found none.", pos).into())?,
          env,
        )?),
        line_info: pos.into(),
      }),
      Rule::Shard => match process_function(pair, env)? {
        FunctionValue::Const(value) => blocks.push(Block {
          content: BlockContent::Const(value),
          line_info: pos.into(),
        }),
        FunctionValue::Function(func) => blocks.push(Block {
          content: BlockContent::Shard(func),
          line_info: pos.into(),
        }),
        FunctionValue::Sequence(seq) => blocks.push(Block {
          content: BlockContent::Expr(seq),
          line_info: pos.into(),
        }),
      },
      Rule::Func => match process_function(pair, env)? {
        FunctionValue::Const(value) => blocks.push(Block {
          content: BlockContent::Const(value),
          line_info: pos.into(),
        }),
        FunctionValue::Function(func) => blocks.push(Block {
          content: BlockContent::Func(func),
          line_info: pos.into(),
        }),
        FunctionValue::Sequence(seq) => blocks.push(Block {
          content: BlockContent::Embed(seq),
          line_info: pos.into(),
        }),
      },
      Rule::TakeTable => blocks.push(Block {
        content: {
          let pair = process_take_table(pair, env);
          BlockContent::TakeTable(pair.0, pair.1)
        },
        line_info: pos.into(),
      }),
      Rule::TakeSeq => blocks.push(Block {
        content: {
          let pair = process_take_seq(pair, env)?;
          BlockContent::TakeSeq(pair.0, pair.1)
        },
        line_info: pos.into(),
      }),
      Rule::ConstValue => blocks.push(Block {
        // this is an indirection, process_value will handle the case of a ConstValue
        content: BlockContent::Const(process_value(pair, env)?),
        line_info: pos.into(),
      }),
      Rule::Shards => blocks.push(Block {
        content: BlockContent::Shards(process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(("Expected an expression, but found none.", pos).into())?,
          env,
        )?),
        line_info: pos.into(),
      }),
      _ => return Err((format!("Unexpected rule ({:?}) in Pipeline.", rule), pos).into()),
    }
  }
  Ok(Pipeline { blocks })
}

fn process_statement(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Statement, ShardsError> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::Assignment => process_assignment(pair, env).map(Statement::Assignment),
    Rule::Pipeline => process_pipeline(pair, env).map(Statement::Pipeline),
    _ => Err(("Expected an Assignment or a Pipeline", pos).into()),
  }
}

pub(crate) fn process_sequence(
  pair: Pair<Rule>,
  env: &mut ReadEnv,
) -> Result<Sequence, ShardsError> {
  let statements = pair
    .into_inner()
    .map(|x| process_statement(x, env))
    .collect::<Result<Vec<_>, _>>()?;
  Ok(Sequence { statements })
}

pub(crate) fn process_program(
  pair: Pair<Rule>,
  env: &mut ReadEnv,
) -> Result<Sequence, ShardsError> {
  let pos = pair.as_span().start_pos();
  if pair.as_rule() != Rule::Program {
    return Err(("Expected a Program rule, but found a different rule.", pos).into());
  }
  let pair = pair.into_inner().next().unwrap();
  process_sequence(pair, env)
}

fn process_value(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Value, ShardsError> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::ConstValue => {
      // unwrap the inner rule
      let pair = pair.into_inner().next().unwrap();
      process_value(pair, env)
    }
    Rule::None => Ok(Value::None),
    Rule::Boolean => {
      // check if string content is true or false
      let bool_str = pair.as_str();
      if bool_str == "true" {
        Ok(Value::Boolean(true))
      } else if bool_str == "false" {
        Ok(Value::Boolean(false))
      } else {
        Err(("Expected a boolean value", pos).into())
      }
    }
    Rule::LowIden => Ok(Value::Identifier(pair.as_str().into())),
    Rule::Enum => {
      let text = pair.as_str();
      let splits: Vec<_> = text.split('.').collect();
      if splits.len() != 2 {
        return Err(("Expected an enum value", pos).into());
      }
      let enum_name = splits[0];
      let variant_name = splits[1];
      Ok(Value::Enum(enum_name.into(), variant_name.into()))
    }
    Rule::Vector => process_vector(pair, env),
    Rule::Number => process_number(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Number value", pos).into())?,
      env,
    )
    .map(Value::Number),
    Rule::String => {
      let inner = pair.into_inner().next().unwrap();
      match inner.as_rule() {
        Rule::SimpleString => Ok(Value::String({
          let full_str = inner.as_str();
          // remove quotes
          full_str[1..full_str.len() - 1].into()
        })),
        Rule::ComplexString => Ok(Value::String({
          let full_str = inner.as_str();
          // remove triple quotes
          full_str[3..full_str.len() - 3].into()
        })),
        _ => unreachable!(),
      }
    }
    Rule::Seq => {
      let values = pair
        .into_inner()
        .map(|value| {
          let pos = value.as_span().start_pos();
          process_value(
            value
              .into_inner()
              .next()
              .ok_or(("Expected a Value in the sequence", pos).into())?,
            env,
          )
        })
        .collect::<Result<Vec<_>, _>>()?;
      Ok(Value::Seq(values))
    }
    Rule::Table => {
      let pairs = pair
        .into_inner()
        .map(|pair| {
          assert_eq!(pair.as_rule(), Rule::TableEntry);

          let mut inner = pair.into_inner();

          let key = inner.next().unwrap(); // should not fail
          assert_eq!(key.as_rule(), Rule::TableKey);
          let pos = key.as_span().start_pos();
          let key = key
            .into_inner()
            .next()
            .ok_or(("Expected a Table key", pos).into())?;
          let key = match key.as_rule() {
            Rule::Iden => Value::String(key.as_str().into()),
            Rule::Value => process_value(key.into_inner().next().unwrap(), env)?,
            _ => unreachable!(),
          };

          let value = inner
            .next()
            .ok_or(("Expected a value in TableEntry", pos).into())?;
          let pos = value.as_span().start_pos();
          let value = process_value(
            value
              .into_inner()
              .next()
              .ok_or(("Expected a value in TableEntry", pos).into())?,
            env,
          )?;
          Ok((key, value))
        })
        .collect::<Result<Vec<_>, _>>()?;
      Ok(Value::Table(pairs))
    }
    Rule::Shards => process_sequence(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Sequence in Value", pos).into())?,
      env,
    )
    .map(Value::Shards),
    Rule::EvalExpr => process_sequence(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Sequence in Value", pos).into())?,
      env,
    )
    .map(Value::EvalExpr),
    Rule::Expr => process_sequence(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Sequence in Value", pos).into())?,
      env,
    )
    .map(Value::Expr),
    Rule::TakeTable => {
      let pair = process_take_table(pair, env);
      Ok(Value::TakeTable(pair.0, pair.1))
    }
    Rule::TakeSeq => {
      let pair = process_take_seq(pair, env)?;
      Ok(Value::TakeSeq(pair.0, pair.1))
    }
    Rule::Func => match process_function(pair, env)? {
      FunctionValue::Const(val) => return Ok(val),
      FunctionValue::Function(func) => Ok(Value::Func(func)),
      _ => Err(("Function cannot be used as value", pos).into()),
    },
    _ => Err(
      (
        format!("Unexpected rule ({:?}) in Value", pair.as_rule()),
        pos,
      )
        .into(),
    ),
  }
}

fn process_number(pair: Pair<Rule>, _env: &mut ReadEnv) -> Result<Number, ShardsError> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::Integer => Ok(Number::Integer(
      pair
        .as_str()
        .parse()
        .map_err(|_| ("Failed to parse Integer", pos).into())?,
    )),
    Rule::Float => Ok(Number::Float(
      pair
        .as_str()
        .parse()
        .map_err(|_| ("Failed to parse Float", pos).into())?,
    )),
    Rule::Hexadecimal => Ok(Number::Hexadecimal(pair.as_str().into())),
    _ => Err(("Unexpected rule in Number", pos).into()),
  }
}

fn process_param(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Param, ShardsError> {
  let pos = pair.as_span().start_pos();
  if pair.as_rule() != Rule::Param {
    return Err(("Expected a Param rule", pos).into());
  }

  let mut inner = pair.into_inner();
  let first = inner
    .next()
    .ok_or(("Expected a ParamName or Value in Param", pos).into())?;
  let pos = first.as_span().start_pos();
  let (param_name, param_value) = if first.as_rule() == Rule::ParamName {
    let name = first.as_str();
    let name = name[0..name.len() - 1].into();
    let value = process_value(
      inner
        .next()
        .ok_or(("Expected a Value in Param", pos).into())?
        .into_inner()
        .next()
        .ok_or(("Expected a Value in Param", pos).into())?,
      env,
    )?;
    (Some(name), value)
  } else {
    (
      None,
      process_value(
        first
          .into_inner()
          .next()
          .ok_or(("Expected a Value in Param", pos).into())?,
        env,
      )?,
    )
  };

  Ok(Param {
    name: param_name,
    value: param_value,
  })
}

fn process_params(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Vec<Param>, ShardsError> {
  pair.into_inner().map(|x| process_param(x, env)).collect()
}

pub fn read(code: &str, path: &str) -> Result<Sequence, ShardsError> {
  profiling::scope!("read", path);

  let successful_parse = ShardsParser::parse(Rule::Program, code).expect("Code parsing failed");
  let mut env = ReadEnv::new(path);
  process_program(successful_parse.into_iter().next().unwrap(), &mut env)
}

#[test]
fn test_parsing1() {
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new(".");
  let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();

  // Serialize using bincode
  let encoded_bin: Vec<u8> = bincode::serialize(&seq).unwrap();

  // Deserialize using bincode
  let decoded_bin: Sequence = bincode::deserialize(&encoded_bin[..]).unwrap();

  // Serialize using json
  let encoded_json = serde_json::to_string(&seq).unwrap();
  println!("Json Serialized = {}", encoded_json);

  let encoded_json2 = serde_json::to_string(&decoded_bin).unwrap();
  assert_eq!(encoded_json, encoded_json2);

  // Deserialize using json
  let decoded_json: Sequence = serde_json::from_str(&encoded_json).unwrap();

  let encoded_bin2: Vec<u8> = bincode::serialize(&decoded_json).unwrap();
  assert_eq!(encoded_bin, encoded_bin2);
}

#[test]
fn test_parsing2() {
  let code = include_str!("explained.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new(".");
  let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();

  // Serialize using bincode
  let encoded_bin: Vec<u8> = bincode::serialize(&seq).unwrap();

  // Deserialize using bincode
  let decoded_bin: Sequence = bincode::deserialize(&encoded_bin[..]).unwrap();

  // Serialize using json
  let encoded_json = serde_json::to_string(&seq).unwrap();
  println!("Json Serialized = {}", encoded_json);

  let encoded_json2 = serde_json::to_string(&decoded_bin).unwrap();
  assert_eq!(encoded_json, encoded_json2);

  // Deserialize using json
  let decoded_json: Sequence = serde_json::from_str(&encoded_json).unwrap();

  let encoded_bin2: Vec<u8> = bincode::serialize(&decoded_json).unwrap();
  assert_eq!(encoded_bin, encoded_bin2);
}
