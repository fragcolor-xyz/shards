use std::{fmt::Display, fs};

use pest::{iterators::Pair, RuleType};
use shards_lang_core::{
  ast::{Identifier, Number, Rule, Value},
  RcStrWrapper,
};

type Error = Box<dyn std::error::Error>;

fn fmt_err<M: Display>(msg: M, r: &pest::Span) -> Error {
  format!("{}: {}", r.as_str(), msg).to_string().into()
}

fn fmt_errp<'i, M: Display, R: RuleType>(msg: M, pair: &pest::iterators::Pair<'i, R>) -> Error {
  fmt_err(msg, &pair.as_span())
}

struct Span {
  start: usize,
  end: usize,
}

#[derive(Default)]
struct Env {
  matches: Vec<Span>,
}

fn extract_identifier(pair: Pair<Rule>) -> Result<Identifier, Error> {
  // so this can be either a simple identifier or a complex identifier
  // complex identifiers are separated by '/'
  // we want to return a vector of identifiers
  let mut identifiers = Vec::new();
  for pair in pair.into_inner() {
    let rule = pair.as_rule();
    match rule {
      Rule::LowIden => identifiers.push(pair.as_str().into()),
      _ => return Err(fmt_errp("Unexpected rule in Identifier.", &pair)),
    }
  }
  Ok(Identifier {
    name: identifiers.pop().unwrap(), // qed
    namespaces: identifiers,
  })
}

fn process_take_seq(pair: Pair<Rule>) -> Result<(Identifier, Vec<u32>), Error> {
  // first is the identifier which has to be VarName
  // followed by N Integer which are the indices
  let span = pair.as_span();
  let mut inner = pair.into_inner();
  let identity = inner
    .next()
    .ok_or(fmt_err("Expected an identifier in TakeSeq", &span))?;

  let identifier = extract_identifier(identity)?;

  let mut indices = Vec::new();
  for pair in inner {
    match pair.as_rule() {
      Rule::Integer => {
        let value = pair
          .as_str()
          .parse()
          .map_err(|_| fmt_errp("Failed to parse Integer", &pair))?;
        indices.push(value);
      }
      _ => return Err(fmt_errp("Expected an integer in TakeSeq", &pair)),
    }
  }

  Ok((identifier, indices))
}

fn process_param(pair: Pair<Rule>, e: &mut Env) -> Result<(), Error> {
  let span = pair.as_span();
  if pair.as_rule() != Rule::Param {
    return Err(fmt_errp("Expected a Param rule", &pair));
  }

  let mut inner = pair.into_inner();
  let first = inner
    .next()
    .ok_or(fmt_err("Expected a ParamName or Value in Param", &span))?;
  let pos = first.as_span().start_pos();
  let (param_name, param_value) = if first.as_rule() == Rule::ParamName {
    let name = first.as_str();
    let name: String = name[0..name.len() - 1].into();
    let value = process_value(
      inner
        .next()
        .ok_or(fmt_err("Expected a Value in Param", &span))?
        .into_inner()
        .next()
        .ok_or(fmt_err("Expected a Value in Param", &span))?,
      e,
    )?;
    (Some(name), value)
  } else {
    (
      None,
      process_value(
        first
          .into_inner()
          .next()
          .ok_or(fmt_err("Expected a Value in Param", &span))?,
        e,
      )?,
    )
  };

  Ok(())
}

fn process_params(pair: Pair<Rule>, e: &mut Env) -> Result<Vec<()>, Error> {
  pair.into_inner().map(|x| process_param(x, e)).collect()
}

fn process_function(pair: Pair<Rule>, e: &mut Env) -> Result<(), Error> {
  let span = pair.as_span();

  let mut inner = pair.into_inner();
  let exp = inner
    .next()
    .ok_or(fmt_err("Expected a Name or Const in Shard", &span))?;

  match exp.as_rule() {
    Rule::UppIden => {
      // Definitely a Shard!
      let identifier = Identifier {
        name: exp.as_str().into(),
        namespaces: Vec::new(),
      };
      let next = inner.next();

      let params = match next {
        Some(pair) => {
          if pair.as_rule() == Rule::Params {
            Some(process_params(pair, e)?)
          } else {
            return Err(fmt_errp("Expected Params in Shard", &pair));
          }
        }
        None => None,
      };

      // identifier.

      Ok(())
    }
    Rule::VarName => {
      // Many other things...!
      let identifier = extract_identifier(exp)?;
      let next = inner.next();

      let params = match next {
        Some(pair) => {
          if pair.as_rule() == Rule::Params {
            Some(process_params(pair, e)?)
          } else {
            return Err(fmt_errp("Expected Params in Shard", &pair));
          }
        }
        None => None,
      };

      if identifier.namespaces.is_empty() {
        let name = identifier.name.as_str();
        match name {
          _ => {}
        }
      }
      Ok(())
    }
    _ => Err(fmt_err(
      format!("Unexpected rule {:?} in Function.", exp.as_rule()),
      &span,
    )),
  }
}

fn process_number(pair: Pair<Rule>) -> Result<Number, Error> {
  let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::Integer => Ok(Number::Integer(
      pair
        .as_str()
        .parse()
        .map_err(|_| fmt_errp("Failed to parse Integer", &pair))?,
    )),
    Rule::Float => Ok(Number::Float(
      pair
        .as_str()
        .parse()
        .map_err(|_| fmt_errp("Failed to parse Float", &pair))?,
    )),
    Rule::Hexadecimal => Ok(Number::Hexadecimal(pair.as_str().into())),
    _ => Err(fmt_errp("Unexpected rule in Number", &pair)),
  }
}

fn process_sequence(pair: Pair<Rule>, e: &mut Env) -> Result<(), Error> {
  for stmt in pair.into_inner() {
    println!("Seq> {}", stmt.as_span().as_str());
    process_statement(stmt, e)?;
  }
  // let statements = pair
  //   .into_inner()
  //   .map(|x| )
  //   .collect::<Result<Vec<_>, _>>()?;
  Ok(())
}

fn process_value(pair: Pair<Rule>, e: &mut Env) -> Result<Value, Error> {
  let span = pair.as_span();
  match pair.as_rule() {
    Rule::ConstValue => {
      // unwrap the inner rule
      let pair = pair.into_inner().next().unwrap(); // parsed qed
      process_value(pair, e)
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
        Err(fmt_err("Expected a boolean value", &span))
      }
    }
    Rule::VarName => Ok(Value::Identifier(extract_identifier(pair)?)),
    Rule::Enum => {
      let text = pair.as_str();
      let splits: Vec<_> = text.split("::").collect();
      if splits.len() != 2 {
        return Err(fmt_err("Expected an enum value", &span));
      }
      let enum_name = splits[0];
      let variant_name = splits[1];
      Ok(Value::Enum(enum_name.into(), variant_name.into()))
    }
    Rule::Number => process_number(
      pair
        .into_inner()
        .next()
        .ok_or(fmt_err("Expected a Number value", &span))?,
    )
    .map(Value::Number),
    Rule::String => {
      let inner = pair.into_inner().next().unwrap(); // parsed qed
      match inner.as_rule() {
        Rule::SimpleString => Ok(Value::String({
          let full_str = inner.as_str();
          // remove quotes AND
          // with this case we need to transform escaped characters
          // so we need to iterate over the string
          let mut chars = full_str[1..full_str.len() - 1].chars();
          let mut new_str = String::new();
          while let Some(c) = chars.next() {
            if c == '\\' {
              // we need to check the next character
              let c = chars
                .next()
                .ok_or(fmt_err("Unexpected end of string", &span))?;
              match c {
                'n' => new_str.push('\n'),
                'r' => new_str.push('\r'),
                't' => new_str.push('\t'),
                '\\' => new_str.push('\\'),
                '"' => new_str.push('"'),
                '\'' => new_str.push('\''),
                _ => {
                  return Err(fmt_err(
                    format!("Unexpected escaped character {:?}", c),
                    &span,
                  ))
                }
              }
            } else {
              new_str.push(c);
            }
          }
          new_str.into()
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
              .ok_or(fmt_err("Expected a Value in the sequence", &span))?,
            e,
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
            .ok_or(fmt_err("Expected a Table key", &span))?;
          let key = match key.as_rule() {
            Rule::None => Value::None,
            Rule::Iden => Value::String(key.as_str().into()),
            Rule::Value => process_value(
              key.into_inner().next().unwrap(), // parsed qed
              e,
            )?,
            _ => unreachable!(),
          };

          let value = inner
            .next()
            .ok_or(fmt_err("Expected a value in TableEntry", &span))?;
          let pos = value.as_span().start_pos();
          let value = process_value(
            value
              .into_inner()
              .next()
              .ok_or(fmt_err("Expected a value in TableEntry", &span))?,
            e,
          )?;
          Ok((key, value))
        })
        .collect::<Result<Vec<_>, Error>>()?;
      Ok(Value::Table(pairs))
      // Ok(Value::None{})
    }
    Rule::Shards => {
      process_sequence(
        pair
          .into_inner()
          .next()
          .ok_or(fmt_err("Expected a Sequence in Value", &span))?,
        e,
      )?;
      Ok(Value::None {})
    }
    Rule::Shard => {
      let _f = process_function(pair, e)?;
      //   match process_function(pair)? {
      //   FunctionValue::Function(func) => Ok(Value::Shard(func)),
      //   _ => Err(fmt_err("Invalid Shard in value", &pair)),
      // }
      Ok(Value::None {})
    }
    Rule::EvalExpr => {
      process_sequence(
        pair
          .into_inner()
          .next()
          .ok_or(fmt_err("Expected a Sequence in Value", &span))?,
        e,
      )?;
      // .map(Value::EvalExpr)

      Ok(Value::None {})
    }
    Rule::Expr => {
      process_sequence(
        pair
          .into_inner()
          .next()
          .ok_or(fmt_err("Expected a Sequence in Value", &span))?,
        e,
      )?;
      Ok(Value::None {})
      // .map(Value::Expr)
    }
    Rule::TakeTable => {
      let pair = process_take_table(pair)?;
      Ok(Value::TakeTable(pair.0, pair.1))
    }
    Rule::TakeSeq => {
      let pair = process_take_seq(pair)?;
      Ok(Value::TakeSeq(pair.0, pair.1))
    }
    Rule::Func => {
      let _f = process_function(pair, e)?;
      // match process_function(pair)? {
      //   FunctionValue::Const(val) => return Ok(val),
      //   FunctionValue::Function(func) => Ok(Value::Func(func)),
      //   _ => Err(fmt_errp("Function cannot be used as value", &pair)),
      // }
      Ok(Value::None {})
    }
    _ => Err(fmt_err(
      format!("Unexpected rule ({:?}) in Value", pair.as_rule()),
      &span,
    )),
  }
}

fn process_take_table(pair: Pair<Rule>) -> Result<(Identifier, Vec<RcStrWrapper>), Error> {
  // first is the identifier which has to be VarName
  // followed by N Iden which are the keys
  let span = pair.as_span();
  let mut inner = pair.into_inner();
  let identity = inner
    .next()
    .ok_or(fmt_err("Expected an identifier in TakeTable", &span))?;

  let identifier = extract_identifier(identity)?;

  let mut keys = Vec::new();
  for pair in inner {
    let pos = pair.as_span().start_pos();
    match pair.as_rule() {
      Rule::Iden => keys.push(pair.as_str().into()),
      _ => return Err(fmt_err("Expected an identifier in TakeTable", &span)),
    }
  }

  // wrap the shards into an Expr Sequence
  Ok((identifier, keys))
}

fn process_pipeline(pair: Pair<Rule>, e: &mut Env) -> Result<(), Error> {
  if pair.as_rule() != Rule::Pipeline {
    return Err(fmt_errp(
      "Expected a Pipeline rule, but found a different rule.",
      &pair,
    ));
  }

  // let mut blocks = Vec::new();

  let span = pair.as_span();
  for pair in pair.into_inner() {
    let rule = pair.as_rule();
    match rule {
      Rule::EvalExpr => {
        process_sequence(
          pair.into_inner().next().ok_or(fmt_err(
            "Expected an eval time expression, but found none.",
            &span,
          ))?,
          e,
        )?;
        // blocks.push(Block {
        // content: BlockContent::EvalExpr(),
        // line_info: Some(pos.into()),
      }
      Rule::Expr => {
        process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected an expression, but found none.", &span))?,
          e,
        )?;
        // blocks.push(Block {
        // content: BlockContent::Expr(),
        // line_info: Some(pos.into()),
      }
      Rule::Shard => match process_function(pair, e)? {
        _ => {}
        // FunctionValue::Const(value) => blocks.push(Block {
        //   content: BlockContent::Const(value),
        //   line_info: Some(pos.into()),
        // }),
        // FunctionValue::Function(func) => blocks.push(Block {
        //   content: BlockContent::Shard(func),
        //   line_info: Some(pos.into()),
        // }),
        // FunctionValue::Program(program) => blocks.push(Block {
        //   content: BlockContent::Program(program),
        //   line_info: Some(pos.into()),
        // }),
      },
      Rule::Func => match process_function(pair, e)? {
        _ => {}
        // FunctionValue::Const(value) => blocks.push(Block {
        //   content: BlockContent::Const(value),
        //   line_info: Some(pos.into()),
        // }),
        // FunctionValue::Function(func) => blocks.push(Block {
        //   content: BlockContent::Func(func),
        //   line_info: Some(pos.into()),
        // }),
        // FunctionValue::Program(program) => blocks.push(Block {
        //   content: BlockContent::Program(program),
        //   line_info: Some(pos.into()),
        // }),
      },
      Rule::TakeTable => {
        let _tt = process_take_table(pair)?;
      }
      Rule::TakeSeq => {
        let _pair = process_take_seq(pair)?;
      }
      Rule::ConstValue => {
        let _v = process_value(pair, e)?;
      }
      Rule::Enum => {
        let _v = process_value(pair, e)?;
      }
      Rule::Shards => {
        let _inner = process_sequence(
          pair
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected an expression, but found none.", &span))?,
          e,
        )?;
      }
      _ => {
        return Err(fmt_err(
          format!("Unexpected rule ({:?}) in Pipeline.", rule),
          &span,
        ))
      }
    }
  }
  Ok(())
}

fn process_assignment(pair: Pair<Rule>, e: &mut Env) -> Result<(), Error> {
  if pair.as_rule() != Rule::Assignment {
    return Err(fmt_errp(
      "Expected an Assignment rule, but found a different rule.",
      &pair,
    ));
  }

  let span = pair.as_span();
  let mut inner = pair.into_inner();

  let pipeline = if let Some(next) = inner.peek() {
    if next.as_rule() == Rule::Pipeline {
      process_pipeline(
        inner.next().ok_or(fmt_err(
          "Expected a Pipeline in Assignment, but found none.",
          &span,
        ))?,
        e,
      )?
    } else {
      // Pipeline {
      //   blocks: vec![Block {
      //     content: BlockContent::Empty,
      //     line_info: Some(pos.into()),
      //   }],
      // }
    }
  } else {
    unreachable!("Assignment should have at least one inner rule.")
  };

  let assignment_op = inner
    .next()
    .ok_or(fmt_err(
      "Expected an AssignmentOp in Assignment, but found none.",
      &span,
    ))?
    .as_str();

  let iden = inner.next().ok_or(fmt_err(
    "Expected an Identifier in Assignment, but found none.",
    &span,
  ))?;

  let identifier = extract_identifier(iden)?;

  // match assignment_op {
  //   "=" => Ok(Assignment::AssignRef(pipeline, identifier)),
  //   ">=" => Ok(Assignment::AssignSet(pipeline, identifier)),
  //   ">" => Ok(Assignment::AssignUpd(pipeline, identifier)),
  //   ">>" => Ok(Assignment::AssignPush(pipeline, identifier)),
  //   _ => Err(("Unexpected assignment operator.", &pair)),
  // }
  Ok(())
}

fn process_statement(pair: Pair<Rule>, e: &mut Env) -> Result<(), Error> {
  // let pos = pair.as_span().start_pos();
  match pair.as_rule() {
    Rule::Assignment => process_assignment(pair, e),
    Rule::Pipeline => process_pipeline(pair, e),
    _ => Err(fmt_errp(
      format!(
        "Expected an Assignment or a Pipeline, was {:?}",
        pair.as_rule()
      ),
      &pair,
    )),
  }
}

fn process(code: &str, e: &mut Env) -> Result<(), Error> {
  let successful_parse = shards_lang_core::read::parse(code).map_err(|x| x.message)?;
  let root = successful_parse.into_iter().next().unwrap();
  let pos = root.as_span().start_pos();
  if root.as_rule() != Rule::Program {
    return Err("Expected a Program rule, but found a different rule.".into());
  }

  let inner = root.into_inner().next().unwrap();
  process_sequence(inner, e)?;

  // for stmt in inner.into_inner() {}

  Ok(())
}

fn main() {
  let str = fs::read_to_string("C:/Projects/shards/docs/samples/shards/UI/Area/1.shs").unwrap();
  // println!("{}", &str);
  let mut env = Env::default();
  process(&str, &mut env).unwrap();
}
