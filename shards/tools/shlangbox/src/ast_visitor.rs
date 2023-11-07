use std::cell::RefCell;

use crate::Rule;
use pest::iterators::Pair;

use crate::error::*;
use shards_lang_core::{
  ast::{Identifier, Number, Value},
  RcStrWrapper,
};

pub trait Visitor {
  fn v_pipeline<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_stmt<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_value<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_assign<T: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    op: Pair<Rule>,
    to: Pair<Rule>,
    inner: T,
  );
  fn v_func<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, name: Pair<Rule>, inner: T);
  fn v_param<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, kw: Option<Pair<Rule>>, inner: T);
  fn v_seq<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_table<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_table_val<TK: FnOnce(&mut Self), TV: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    key: Pair<Rule>,
    inner_key: TK,
    inner_val: TV,
  );
  fn v_eval_expr<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_expr<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_shards<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
}

#[derive(Default)]
pub struct Env {
  // matches: Vec<crate::span::Span>,
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

fn process_param<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let span = pair.as_span();
  if pair.as_rule() != Rule::Param {
    return Err(fmt_errp("Expected a Param rule", &pair));
  }

  let mut inner = pair.clone().into_inner();
  let first = inner
    .next()
    .ok_or(fmt_err("Expected a ParamName or Value in Param", &span))?;
  let pos = first.as_span().start_pos();

  let mut result: Option<Result<(), Error>> = None;
  if first.clone().as_rule() == Rule::ParamName {
    // let name = first.as_str();
    // let name: String = name[0..name.len() - 1].into();
    v.v_param(pair, Some(first), |v| {
      result = Some((|| {
        let value = process_value(
          inner
            .next()
            .ok_or(fmt_err("Expected a Value in Param", &span))?
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected a Value in Param", &span))?,
          v,
          e,
        )?;
        Ok(())
      })());
    });
  } else {
    v.v_param(pair, None, |v| {
      result = Some((|| {
        process_value(
          first
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected a Value in Param", &span))?,
          v,
          e,
        )?;
        Ok(())
      })());
    });
  }
  result.expect("Visitor didn't call v_param inner")

  // let (param_name, param_value) = if first.as_rule() == Rule::ParamName {
  //   let name = first.as_str();
  //   let name: String = name[0..name.len() - 1].into();
  //   let value = process_value(
  //     inner
  //       .next()
  //       .ok_or(fmt_err("Expected a Value in Param", &span))?
  //       .into_inner()
  //       .next()
  //       .ok_or(fmt_err("Expected a Value in Param", &span))?,
  //     v,
  //     e,
  //   )?;
  //   (Some(name), value)
  // } else {
  //   (
  //     None,
  //     process_value(
  //       first
  //         .into_inner()
  //         .next()
  //         .ok_or(fmt_err("Expected a Value in Param", &span))?,
  //       v,
  //       e,
  //     )?,
  //   )
  // };
}

fn process_params<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<Vec<()>, Error> {
  pair.into_inner().map(|x| process_param(x, v, e)).collect()
}

fn process_function<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let span = pair.as_span();

  let mut inner = pair.clone().into_inner();
  let exp: Pair<'_, shards_lang_core::ast::Rule> = inner
    .next()
    .ok_or(fmt_err("Expected a Name or Const in Shard", &span))?;

  let mut result: Result<(), Error> = Ok(());
  v.v_func(pair.clone(), exp.clone(), |v| {
    result = (|| {
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
                Some(process_params(pair, v, e)?)
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
                Some(process_params(pair, v, e)?)
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
    })();
  });
  result
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

fn process_sequence<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  let span = pair.as_span();
  v.v_seq(pair.clone(), |v| {
    result = Some((|| {
      let values = pair
        .into_inner()
        .map(|value| {
          let pos = value.as_span().start_pos();
          process_value(
            value
              .into_inner()
              .next()
              .ok_or(fmt_err("Expected a Value in the sequence", &span))?,
            v,
            e,
          )
        })
        .collect::<Result<Vec<_>, _>>()?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_seq inner")
}

fn process_eval_expr<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  v.v_eval_expr(pair.clone(), |v| {
    result = Some((|| {
      process_sequence_no_visit(pair, v, e)?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_eval_expr inner")
}

fn process_expr<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  v.v_expr(pair.clone(), |v| {
    result = Some((|| {
      process_sequence_no_visit(pair, v, e)?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_expr inner")
}

fn process_shards<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  let span = pair.as_span();
  let contents = pair.clone()
    .into_inner()
    .next()
    .ok_or(fmt_err("Expected an expression, but found none.", &span))?;

  v.v_shards(pair.clone(), |v| {
    result = Some((|| {
      process_sequence_no_visit(contents, v, e)?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_shards inner")
}

fn process_sequence_no_visit<V: Visitor>(
  pair: Pair<Rule>,
  v: &mut V,
  e: &mut Env,
) -> Result<(), Error> {
  for stmt in pair.into_inner() {
    process_statement(stmt, v, e)?;
  }
  Ok(())
}

fn process_value<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let span = pair.as_span();

  let matched: Result<bool, Error> = {
    let pair = pair.clone();
    match pair.as_rule() {
      Rule::ConstValue => {
        // unwrap the inner rule
        let pair = pair.into_inner().next().unwrap(); // parsed qed
        process_value(pair, v, e)?;
        Ok(true)
      }
      Rule::Seq => {
        process_sequence(pair, v, e)?;
        Ok(true)
      }
      Rule::Table => {
        process_table(pair, v, e)?;
        Ok(true)
      }
      Rule::Shards => {
        process_shards(pair, v, e)?;
        Ok(true)
      }
      Rule::Shard => {
        let _f = process_function(pair, v, e)?;
        Ok(true)
      }
      Rule::EvalExpr => {
        process_eval_expr(
          pair
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected a Sequence in Value", &span))?,
          v,
          e,
        )?;
        Ok(true)
      }
      Rule::Expr => {
        process_expr(
          pair
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected a Sequence in Value", &span))?,
          v,
          e,
        )?;
        Ok(true)
      }
      Rule::TakeTable => {
        let pair = process_take_table(pair)?;
        Ok(true)
      }
      Rule::TakeSeq => {
        let pair = process_take_seq(pair)?;
        Ok(true)
      }
      Rule::Func => {
        let _f = process_function(pair, v, e)?;
        Ok(true)
      }
      _ => Ok(false),
    }
  };
  if matched? {
    return Ok(());
  }

  // This is an atomic value (single token)
  let mut result: Option<Result<(), Error>> = None;
  v.v_value(pair.clone(), |v| {
    result = Some((|| {
      let span = pair.as_span();
      match pair.as_rule() {
        Rule::None => Ok(()),
        Rule::Boolean => Ok(()),
        Rule::VarName => Ok(()),
        Rule::Enum => Ok(()),
        Rule::Number => Ok(()),
        Rule::String => Ok(()),
        Rule::Iden => Ok(()),
        _ => Err(fmt_err(
          format!("Unexpected rule ({:?}) in Value", pair.as_rule()),
          &span,
        )),
      }
    })());
  });

  result.expect("Visitor didn't call v_value inner")
}

fn process_table<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;
  let span = pair.as_span();

  let rc_e = RefCell::new(e);
  v.v_table(pair.clone(), |v| {
    result = Some((|| {
      let pairs = pair
        .into_inner()
        .map(|pair| {
          assert_eq!(pair.as_rule(), Rule::TableEntry);

          let mut inner = pair.into_inner();

          let key: Pair<'_, shards_lang_core::ast::Rule> = inner.next().unwrap(); // should not fail
          assert_eq!(key.as_rule(), Rule::TableKey);
          let key = key
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected a Table key", &span))?;

          let value = inner
            .next()
            .ok_or(fmt_err("Expected a value in TableEntry", &span))?;

          let mut key_result: Option<Result<(), Error>> = None;
          let mut val_result: Option<Result<(), Error>> = None;
          v.v_table_val(
            value.clone(),
            key.clone(),
            |v| {
              let mut e = rc_e.borrow_mut();
              key_result = Some((|| {
                match key.as_rule() {
                  Rule::Iden | Rule::None => process_value(key, v, *e)?,
                  Rule::Value => process_value(
                    key.into_inner().next().unwrap(), // parsed qed
                    v,
                    *e,
                  )?,
                  _ => unreachable!(),
                };
                Ok(())
              })());
            },
            |v| {
              let mut e = rc_e.borrow_mut();
              val_result = Some((|| {
                let pos = value.as_span().start_pos();
                let value = process_value(
                  value
                    .into_inner()
                    .next()
                    .ok_or(fmt_err("Expected a value in TableEntry", &span))?,
                  v,
                  *e,
                )?;
                Ok(())
              })());
            },
          );
          key_result.expect("Visitor didn't call v_table_val key inner")?;
          val_result.expect("Visitor didn't call v_table_val value inner")?;

          Ok(())
        })
        .collect::<Result<Vec<_>, Error>>()?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_table inner")
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

fn process_pipeline<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  if pair.as_rule() != Rule::Pipeline {
    return Err(fmt_errp(
      "Expected a Pipeline rule, but found a different rule.",
      &pair,
    ));
  }

  let mut result: Result<(), Error> = Ok(());
  v.v_pipeline(pair.clone(), |v| {
    result = (|| {
      let span = pair.as_span();
      for pair in pair.into_inner() {
        let rule = pair.as_rule();
        match rule {
          Rule::EvalExpr => {
            process_eval_expr(
              pair.into_inner().next().ok_or(fmt_err(
                "Expected an eval time expression, but found none.",
                &span,
              ))?,
              v,
              e,
            )?;
            // blocks.push(Block {
            // content: BlockContent::EvalExpr(),
            // line_info: Some(pos.into()),
          }
          Rule::Expr => {
            process_expr(
              pair
                .into_inner()
                .next()
                .ok_or(fmt_err("Expected an expression, but found none.", &span))?,
              v,
              e,
            )?;
            // blocks.push(Block {
            // content: BlockContent::Expr(),
            // line_info: Some(pos.into()),
          }
          Rule::Shard => match process_function(pair, v,e)? {
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
          Rule::Func => match process_function(pair, v,e)? {
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
            let _v = process_value(pair, v, e)?;
          }
          Rule::Enum => {
            let _v = process_value(pair, v, e)?;
          }
          Rule::Shards => {
            let _inner = process_shards(pair, v, e)?;
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
    })();
  });
  result
}

fn process_assignment<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  if pair.as_rule() != Rule::Assignment {
    return Err(fmt_errp(
      "Expected an Assignment rule, but found a different rule.",
      &pair,
    ));
  }

  let span = pair.as_span();
  let mut inner = pair.clone().into_inner();

  let pipeline = if let Some(next) = inner.next() {
    next
  } else {
    unreachable!("Assignment should have at least one inner rule.")
  };

  let assignment_op = inner.next().ok_or(fmt_err(
    "Expected an AssignmentOp in Assignment, but found none.",
    &span,
  ))?;

  let iden = inner.next().ok_or(fmt_err(
    "Expected an Identifier in Assignment, but found none.",
    &span,
  ))?;

  let mut result: Result<(), Error> = Ok(());
  v.v_assign(pair.clone(), assignment_op, iden, |v| {
    result = (|| {
      if pipeline.as_rule() == Rule::Pipeline {
        process_pipeline(pipeline, v, e)?
      } else {
        v.v_pipeline(pipeline, |v| {});
      }
      // match assignment_op {
      //   "=" => Ok(Assignment::AssignRef(pipeline, identifier)),
      //   ">=" => Ok(Assignment::AssignSet(pipeline, identifier)),
      //   ">" => Ok(Assignment::AssignUpd(pipeline, identifier)),
      //   ">>" => Ok(Assignment::AssignPush(pipeline, identifier)),
      //   _ => Err(("Unexpected assignment operator.", &pair)),
      // }
      Ok(())
    })();
  });
  result
}

fn process_statement<V: Visitor>(pair: Pair<Rule>, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let mut result: Result<(), Error> = Ok(());
  v.v_stmt(pair.clone(), |v| {
    result = (|| {
      // let span = pair.as_span();
      let rule = pair.as_rule();
      match rule {
        Rule::Assignment => process_assignment(pair, v, e)?,
        Rule::Pipeline => process_pipeline(pair, v, e)?,
        _ => return Err(fmt_errp("Unexpected rule in Statement.", &pair)),
      }
      Ok(())
    })();
  });
  result
}

pub fn process<V: Visitor>(code: &str, v: &mut V, e: &mut Env) -> Result<(), Error> {
  let successful_parse = shards_lang_core::read::parse(code).map_err(|x| x.message)?;
  let root = successful_parse.into_iter().next().unwrap();
  let pos = root.as_span().start_pos();
  if root.as_rule() != Rule::Program {
    return Err("Expected a Program rule, but found a different rule.".into());
  }

  let inner = root.into_inner().next().unwrap();
  process_sequence_no_visit(inner, v, e)?;

  // for stmt in inner.into_inner() {}

  Ok(())
}
