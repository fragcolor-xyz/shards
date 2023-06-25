use crate::types::*;
use core::convert::TryInto;
use pest::{iterators::Pair, Parser};

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
      Rule::Shard => blocks.push(Block::BlockContent(
        process_block_content(pair)?,
        pos.into(),
      )),
      Rule::EvalExpr => blocks.push(Block::EvalExpr(process_sequence(
        pair
          .into_inner()
          .next()
          .ok_or(("Expected an eval time expression, but found none.", pos).into())?,
      )?)),
      Rule::Expr => blocks.push(Block::Expr(process_sequence(
        pair
          .into_inner()
          .next()
          .ok_or(("Expected an expression, but found none.", pos).into())?,
      )?)),
      Rule::Vector => {
        // in this case we want a Const with a vector value
        let value = process_vector(pair)?;
        blocks.push(Block::BlockContent(BlockContent::Const(value), pos.into()));
      }
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

fn process_value(pair: Pair<Rule>) -> Result<Value, ShardsError> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
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
    Rule::Vector => process_vector(pair),
    Rule::Number => process_number(
      pair
        .into_inner()
        .next()
        .ok_or(("Expected a Number value", pos).into())?,
    )
    .map(Value::Number),
    Rule::String => Ok(Value::String(pair.as_str().to_string())),
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
          let pos = pair.as_span().start_pos();
          let mut inner = pair.into_inner();

          let key = inner
            .next()
            .ok_or(("Expected a value key in TableEntry", pos).into())?;
          let pos = key.as_span().start_pos();
          let key = process_value(
            key
              .into_inner()
              .next()
              .ok_or(("Expected a value key in TableEntry", pos).into())?
              .into_inner()
              .next()
              .ok_or(("Expected a value key in TableEntry", pos).into())?,
          )?;

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
      let str_expr = pair.as_str();
      // split by ':'
      let splits: Vec<_> = str_expr.split(':').collect();
      let var_name = splits[0];
      let keys: Vec<String> = splits[1..].iter().map(|s| s.to_string()).collect();
      // wrap the shards into an Expr Sequence
      Ok(Value::TakeTable(var_name.to_owned(), keys))
    }
    Rule::TakeSeq => {
      // do the same as TakeTable but with a sequence of values where index is an integer int32
      let str_expr = pair.as_str();
      // split by ':'
      let splits: Vec<_> = str_expr.split(':').collect();
      let var_name = splits[0];
      let keys: Result<Vec<u32>, _> = splits[1..].iter().map(|s| s.parse::<u32>()).collect();
      let keys = keys.map_err(|_| ("Failed to parse unsigned index integer", pos).into())?;
      // wrap the shards into an Expr Sequence
      Ok(Value::TakeSeq(var_name.to_owned(), keys))
    }
    _ => Err(("Unexpected rule in Value", pos).into()),
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

fn process_block_content(pair: Pair<Rule>) -> Result<BlockContent, ShardsError> {
  let pos = pair.as_span().start_pos();

  let mut inner = pair.into_inner();
  let shard_exp = inner
    .next()
    .ok_or(("Expected a Name or Const in Shard", pos).into())?;

  let pos = shard_exp.as_span().start_pos();

  match shard_exp.as_rule() {
    Rule::None => Ok(BlockContent::Const(Value::None)),
    Rule::Boolean => {
      // check if string content is true or false
      let bool_str = shard_exp.as_str();
      if bool_str == "true" {
        Ok(BlockContent::Const(Value::Boolean(true)))
      } else if bool_str == "false" {
        Ok(BlockContent::Const(Value::Boolean(false)))
      } else {
        Err(("Expected a boolean value", pos).into())
      }
    }
    Rule::UppIden => {
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

      Ok(BlockContent::Shard(Shard {
        name: shard_name,
        params,
      }))
    }
    Rule::Value => {
      let value = process_value(
        shard_exp
          .into_inner()
          .next()
          .ok_or(("Expected a Value in Shard", pos).into())?,
      )?;
      Ok(BlockContent::Const(value))
    }
    Rule::TakeTable => {
      let str_expr = shard_exp.as_str();
      // split by ':'
      let splits: Vec<_> = str_expr.split(':').collect();
      let var_name = splits[0];
      let keys: Vec<String> = splits[1..].iter().map(|s| s.to_string()).collect();
      // wrap the shards into an Expr Sequence
      Ok(BlockContent::TakeTable(var_name.to_owned(), keys))
    }
    Rule::TakeSeq => {
      // do the same as TakeTable but with a sequence of values where index is an integer int32
      let str_expr = shard_exp.as_str();
      // split by ':'
      let splits: Vec<_> = str_expr.split(':').collect();
      let var_name = splits[0];
      let keys: Result<Vec<u32>, _> = splits[1..].iter().map(|s| s.parse::<u32>()).collect();
      let keys = keys.map_err(|_| ("Failed to parse integer", pos).into())?;
      // wrap the shards into an Expr Sequence
      Ok(BlockContent::TakeSeq(var_name.to_owned(), keys))
    }
    _ => Err(("Unexpected rule in Shard", pos).into()),
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
fn test_parsing() {
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Sequence, code).unwrap();
  let seq = process_sequence(successful_parse.into_iter().next().unwrap()).unwrap();

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
