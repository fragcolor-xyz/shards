use crate::ast::*;
use core::convert::TryInto;
use pest::iterators::Pair;
#[cfg(test)]
use pest::Parser;

fn process_assignment(pair: Pair<Rule>) -> Result<Assignment, ShardsError> {
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
  )?;
  let identifier = sub_inner
    .next()
    .ok_or(("Expected an identifier in Assignment, but found none.", pos).into())?
    .as_str()
    .to_string();

  match sub_rule {
    Rule::AssignRef => Ok(Assignment::AssignRef(pipeline, identifier)),
    Rule::AssignSet => Ok(Assignment::AssignSet(pipeline, identifier)),
    Rule::AssignUpd => Ok(Assignment::AssignUpd(pipeline, identifier)),
    Rule::AssignPush => Ok(Assignment::AssignPush(pipeline, identifier)),
    _ => Err(("Unexpected sub-rule in Assignment.", pos).into()),
  }
}

fn process_vector(pair: Pair<Rule>) -> Result<Value, ShardsError> {
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

fn process_function(pair: Pair<Rule>) -> Result<Function, ShardsError> {
  let pos = pair.as_span().start_pos();

  let mut inner = pair.into_inner();
  let shard_exp = inner
    .next()
    .ok_or(("Expected a Name or Const in Shard", pos).into())?;

  let pos = shard_exp.as_span().start_pos();

  match shard_exp.as_rule() {
    Rule::UppIden | Rule::LowIden => {
      let shard_name = shard_exp.as_str().to_string();
      let next = inner.next();

      let params = match next {
        Some(pair) => {
          if pair.as_rule() == Rule::Params {
            Some(process_params(pair)?)
          } else {
            return Err(("Expected Params in Shard", pos).into());
          }
        }
        None => None,
      };

      Ok(Function {
        name: shard_name,
        params,
      })
    }
    _ => Err(
      (
        format!("Unexpected rule {:?} in Function.", shard_exp.as_rule()),
        pos,
      )
        .into(),
    ),
  }
}

fn process_take_table(pair: Pair<Rule>) -> (String, Vec<String>) {
  let str_expr = pair.as_str();
  // split by ':'
  let splits: Vec<_> = str_expr.split(':').collect();
  let var_name = splits[0];
  let keys: Vec<String> = splits[1..].iter().map(|s| s.to_string()).collect();
  // wrap the shards into an Expr Sequence
  (var_name.to_owned(), keys)
}

fn process_take_seq(pair: Pair<Rule>) -> Result<(String, Vec<u32>), ShardsError> {
  let pos = pair.as_span().start_pos();
  // do the same as TakeTable but with a sequence of values where index is an integer int32
  let str_expr = pair.as_str();
  // split by ':'
  let splits: Vec<_> = str_expr.split(':').collect();
  let var_name = splits[0];
  let keys: Result<Vec<u32>, _> = splits[1..].iter().map(|s| s.parse::<u32>()).collect();
  let keys = keys.map_err(|_| ("Failed to parse unsigned index integer", pos).into())?;
  // wrap the shards into an Expr Sequence
  Ok((var_name.to_owned(), keys))
}

fn process_pipeline(pair: Pair<Rule>) -> Result<Pipeline, ShardsError> {
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
        let value = process_vector(pair)?;
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
        )?),
        line_info: pos.into(),
      }),
      Rule::Expr => blocks.push(Block {
        content: BlockContent::Expr(process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(("Expected an expression, but found none.", pos).into())?,
        )?),
        line_info: pos.into(),
      }),
      Rule::Shard => blocks.push(Block {
        content: BlockContent::Shard(process_function(pair)?),
        line_info: pos.into(),
      }),
      Rule::Func => blocks.push(Block {
        content: BlockContent::Func(process_function(pair)?),
        line_info: pos.into(),
      }),
      Rule::TakeTable => blocks.push(Block {
        content: {
          let pair = process_take_table(pair);
          BlockContent::TakeTable(pair.0, pair.1)
        },
        line_info: pos.into(),
      }),
      Rule::TakeSeq => blocks.push(Block {
        content: {
          let pair = process_take_seq(pair)?;
          BlockContent::TakeSeq(pair.0, pair.1)
        },
        line_info: pos.into(),
      }),
      Rule::ConstValue => blocks.push(Block {
        // this is an indirection, process_value will handle the case of a ConstValue
        content: BlockContent::Const(process_value(pair)?),
        line_info: pos.into(),
      }),
      Rule::Shards => blocks.push(Block {
        content: BlockContent::Shards(process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(("Expected an expression, but found none.", pos).into())?,
        )?),
        line_info: pos.into(),
      }),
      _ => return Err((format!("Unexpected rule ({:?}) in Pipeline.", rule), pos).into()),
    }
  }
  Ok(Pipeline { blocks })
}

fn process_statement(pair: Pair<Rule>) -> Result<Statement, ShardsError> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::Assignment => process_assignment(pair).map(Statement::Assignment),
    Rule::Pipeline => process_pipeline(pair).map(Statement::Pipeline),
    _ => Err(("Expected an Assignment or a Pipeline", pos).into()),
  }
}

pub(crate) fn process_sequence(pair: Pair<Rule>) -> Result<Sequence, ShardsError> {
  let statements = pair
    .into_inner()
    .map(process_statement)
    .collect::<Result<Vec<_>, _>>()?;
  Ok(Sequence { statements })
}

pub(crate) fn process_program(pair: Pair<Rule>) -> Result<Sequence, ShardsError> {
  let pos = pair.as_span().start_pos();
  if pair.as_rule() != Rule::Program {
    return Err(("Expected a Program rule, but found a different rule.", pos).into());
  }
  let pair = pair.into_inner().next().unwrap();
  process_sequence(pair)
}

fn process_value(pair: Pair<Rule>) -> Result<Value, ShardsError> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::ConstValue => {
      // unwrap the inner rule
      let pair = pair.into_inner().next().unwrap();
      process_value(pair)
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
    Rule::LowIden => Ok(Value::Identifier(pair.as_str().to_string())),
    Rule::Enum => {
      let text = pair.as_str();
      let splits: Vec<_> = text.split('.').collect();
      if splits.len() != 2 {
        return Err(("Expected an enum value", pos).into());
      }
      let enum_name = splits[0];
      let variant_name = splits[1];
      Ok(Value::Enum(enum_name.to_owned(), variant_name.to_owned()))
    }
    Rule::Vector => process_vector(pair),
    Rule::Number => process_number(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Number value", pos).into())?,
    )
    .map(Value::Number),
    Rule::String => {
      let inner = pair.into_inner().next().unwrap();
      match inner.as_rule() {
        Rule::SimpleString => Ok(Value::String({
          let full_str = inner.as_str().to_string();
          // remove quotes
          full_str[1..full_str.len() - 1].to_string()
        })),
        Rule::ComplexString => Ok(Value::String({
          let full_str = inner.as_str().to_string();
          // remove triple quotes
          full_str[3..full_str.len() - 3].to_string()
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
            Rule::Iden => Value::String(key.as_str().to_string()),
            Rule::Value => process_value(key.into_inner().next().unwrap())?,
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
    )
    .map(Value::Shards),
    Rule::EvalExpr => process_sequence(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Sequence in Value", pos).into())?,
    )
    .map(Value::EvalExpr),
    Rule::Expr => process_sequence(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Sequence in Value", pos).into())?,
    )
    .map(Value::Expr),
    Rule::TakeTable => {
      let pair = process_take_table(pair);
      Ok(Value::TakeTable(pair.0, pair.1))
    }
    Rule::TakeSeq => {
      let pair = process_take_seq(pair)?;
      Ok(Value::TakeSeq(pair.0, pair.1))
    }
    Rule::Func => {
      let pair = process_function(pair)?;
      Ok(Value::Func(pair))
    }
    _ => Err(
      (
        format!("Unexpected rule ({:?}) in Value", pair.as_rule()),
        pos,
      )
        .into(),
    ),
  }
}

fn process_number(pair: Pair<Rule>) -> Result<Number, ShardsError> {
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
    Rule::Hexadecimal => Ok(Number::Hexadecimal(
      pair
        .as_str()
        .parse()
        .map_err(|_| ("Failed to parse Hexadecimal", pos).into())?,
    )),
    _ => Err(("Unexpected rule in Number", pos).into()),
  }
}

fn process_param(pair: Pair<Rule>) -> Result<Param, ShardsError> {
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
    let name = name[0..name.len() - 1].to_string();
    let value = process_value(
      inner
        .next()
        .ok_or(("Expected a Value in Param", pos).into())?
        .into_inner()
        .next()
        .ok_or(("Expected a Value in Param", pos).into())?,
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
      )?,
    )
  };

  Ok(Param {
    name: param_name,
    value: param_value,
  })
}

fn process_params(pair: Pair<Rule>) -> Result<Vec<Param>, ShardsError> {
  pair.into_inner().map(process_param).collect()
}

#[test]
fn test_parsing1() {
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let seq = process_program(successful_parse.into_iter().next().unwrap()).unwrap();

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
  let seq = process_program(successful_parse.into_iter().next().unwrap()).unwrap();

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
