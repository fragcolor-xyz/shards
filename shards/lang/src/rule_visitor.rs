type Rule = crate::ast::Rule;
use crate::error::*;
use pest::iterators::Pair;

pub trait RuleVisitor {
  fn v_pipeline<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_stmt<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_value(&mut self, pair: Pair<Rule>);
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
  fn v_eval_expr<T: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    inner_pair: Pair<Rule>,
    inner: T,
  );
  fn v_expr<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner_pair: Pair<Rule>, inner: T);
  fn v_shards<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T);
  fn v_take_table(&mut self, pair: Pair<Rule>);
  fn v_take_seq(&mut self, pair: Pair<Rule>);
  fn v_end(&mut self, pair: Pair<Rule>);
}

fn process_take_seq<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  v.v_take_seq(pair);
  Ok(())
}

fn process_param<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let span = pair.as_span();
  if pair.as_rule() != Rule::Param {
    return Err(fmt_errp("Expected a Param rule", &pair));
  }

  let mut inner = pair.clone().into_inner();
  let first = inner
    .next()
    .ok_or(fmt_err("Expected a ParamName or Value in Param", &span))?;

  let mut result: Option<Result<(), Error>> = None;
  if first.clone().as_rule() == Rule::ParamName {
    v.v_param(pair, Some(first), |v| {
      result = Some((|| {
        process_value(
          inner
            .next()
            .ok_or(fmt_err("Expected a Value in Param", &span))?
            .into_inner()
            .next()
            .ok_or(fmt_err("Expected a Value in Param", &span))?,
          v,
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
        )?;
        Ok(())
      })());
    });
  }
  result.expect("Visitor didn't call v_param inner")
}

fn process_params<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<Vec<()>, Error> {
  pair.into_inner().map(|x| process_param(x, v)).collect()
}

fn process_function<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let span = pair.as_span();

  let mut inner = pair.clone().into_inner();
  let exp: Pair<'_, Rule> = inner
    .next()
    .ok_or(fmt_err("Expected a Name or Const in Shard", &span))?;

  let mut result: Result<(), Error> = Ok(());
  v.v_func(pair.clone(), exp.clone(), |v| {
    result = (|| {
      match exp.as_rule() {
        Rule::UppIden => {
          // Definitely a Shard!
          let next = inner.next();
          match next {
            Some(pair) => {
              if pair.as_rule() == Rule::Params {
                Some(process_params(pair, v)?)
              } else {
                return Err(fmt_errp("Expected Params in Shard", &pair));
              }
            }
            None => None,
          };
          Ok(())
        }
        Rule::VarName => {
          let next = inner.next();
          match next {
            Some(pair) => {
              if pair.as_rule() == Rule::Params {
                Some(process_params(pair, v)?)
              } else {
                return Err(fmt_errp("Expected Params in Shard", &pair));
              }
            }
            None => None,
          };

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

fn process_sequence<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  let span = pair.as_span();
  v.v_seq(pair.clone(), |v| {
    result = Some((|| {
      pair
        .into_inner()
        .map(|value| {
          process_value(
            value
              .into_inner()
              .next()
              .ok_or(fmt_err("Expected a Value in the sequence", &span))?,
            v,
          )
        })
        .collect::<Result<Vec<_>, _>>()?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_seq inner")
}

fn process_eval_expr<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  let span = pair.as_span();
  let inner = pair
    .clone()
    .into_inner()
    .next()
    .ok_or(fmt_err("Expected a Sequence in Value", &span))?;

  v.v_eval_expr(pair, inner.clone(), |v| {
    result = Some((|| {
      process_sequence_no_visit(inner, v)?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_eval_expr inner")
}

fn process_expr<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  let span = pair.as_span();
  let inner = pair
    .clone()
    .into_inner()
    .next()
    .ok_or(fmt_err("Expected a Sequence in Value", &span))?;

  v.v_expr(pair, inner.clone(), |v| {
    result = Some((|| {
      process_sequence_no_visit(inner, v)?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_expr inner")
}

fn process_shards<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;

  let span = pair.as_span();
  let contents = pair
    .clone()
    .into_inner()
    .next()
    .ok_or(fmt_err("Expected an expression, but found none.", &span))?;

  v.v_shards(pair.clone(), |v| {
    result = Some((|| {
      process_sequence_no_visit(contents, v)?;
      Ok(())
    })());
  });
  result.expect("Visitor didn't call v_shards inner")
}

fn process_sequence_no_visit<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  for stmt in pair.into_inner() {
    process_statement(stmt, v)?;
  }
  Ok(())
}

fn process_value<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let matched: Result<bool, Error> = {
    let pair = pair.clone();
    match pair.as_rule() {
      Rule::ConstValue => {
        // unwrap the inner rule
        let pair = pair.into_inner().next().unwrap(); // parsed qed
        process_value(pair, v)?;
        Ok(true)
      }
      Rule::Seq => {
        process_sequence(pair, v)?;
        Ok(true)
      }
      Rule::Table => {
        process_table(pair, v)?;
        Ok(true)
      }
      Rule::Shards => {
        process_shards(pair, v)?;
        Ok(true)
      }
      Rule::Shard => {
        let _f = process_function(pair, v)?;
        Ok(true)
      }
      Rule::EvalExpr => {
        process_eval_expr(pair, v)?;
        Ok(true)
      }
      Rule::Expr => {
        process_expr(pair, v)?;
        Ok(true)
      }
      Rule::TakeTable => {
        process_take_table(pair, v)?;
        Ok(true)
      }
      Rule::TakeSeq => {
        process_take_seq(pair, v)?;
        Ok(true)
      }
      Rule::Func => {
        process_function(pair, v)?;
        Ok(true)
      }
      _ => Ok(false),
    }
  };
  if matched? {
    return Ok(());
  }

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
  }?;

  // This is an atomic value (single token)
  v.v_value(pair.clone());

  Ok(())
}

fn process_table<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let mut result: Option<Result<(), Error>> = None;
  let span = pair.as_span();

  v.v_table(pair.clone(), |v| {
    result = Some((|| {
      pair
        .into_inner()
        .map(|pair| {
          assert_eq!(pair.as_rule(), Rule::TableEntry);

          let mut inner = pair.into_inner();

          let key: Pair<'_, Rule> = inner.next().unwrap(); // should not fail
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
              key_result = Some((|| {
                match key.as_rule() {
                  Rule::Iden | Rule::None => process_value(key, v)?,
                  Rule::ConstValue => process_value(key, v)?,
                  Rule::VarName => process_value(key, v)?,
                  _ => unreachable!(),
                };
                Ok(())
              })());
            },
            |v| {
              val_result = Some((|| {
                process_value(
                  value
                    .into_inner()
                    .next()
                    .ok_or(fmt_err("Expected a value in TableEntry", &span))?,
                  v,
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

fn process_take_table<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  v.v_take_table(pair);
  Ok(())
}

fn process_pipeline<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
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
            process_eval_expr(pair, v)?;
          }
          Rule::Expr => {
            process_expr(pair, v)?;
          }
          Rule::Shard => process_function(pair, v)?,
          Rule::Func => process_function(pair, v)?,
          Rule::TakeTable => process_take_table(pair, v)?,
          Rule::TakeSeq => {
            let _pair = process_take_seq(pair, v)?;
          }
          Rule::ConstValue => {
            let _v = process_value(pair, v)?;
          }
          Rule::Enum => {
            let _v = process_value(pair, v)?;
          }
          Rule::Shards => {
            let _inner = process_shards(pair, v)?;
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

fn process_assignment<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  if pair.as_rule() != Rule::Assignment {
    return Err(fmt_errp(
      "Expected an Assignment rule, but found a different rule.",
      &pair,
    ));
  }

  let span = pair.as_span();
  let mut inner = pair.clone().into_inner();

  let pipeline = if let Some(next) = inner.peek() {
    if next.as_rule() == Rule::Pipeline {
      inner.next()
    } else {
      None
    }
  } else {
    None
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
  v.v_assign(pair.clone(), assignment_op, iden, |v: &mut V| {
    result = (|| {
      if let Some(pipeline) = pipeline {
        process_pipeline(pipeline, v)?
        // } else {
        //   v.v_pipeline(pipeline, |v| {});
        // }
      }
      Ok(())
    })();
  });
  result
}

fn process_statement<V: RuleVisitor>(pair: Pair<Rule>, v: &mut V) -> Result<(), Error> {
  let mut result: Result<(), Error> = Ok(());
  v.v_stmt(pair.clone(), |v| {
    result = (|| {
      let rule = pair.as_rule();
      match rule {
        Rule::Assignment => process_assignment(pair, v)?,
        Rule::Pipeline => process_pipeline(pair, v)?,
        _ => return Err(fmt_errp("Unexpected rule in Statement.", &pair)),
      }
      Ok(())
    })();
  });
  result
}

pub fn process<V: RuleVisitor>(code: &str, v: &mut V) -> Result<(), Error> {
  let successful_parse = crate::read::parse(code).map_err(|x| x.message)?;
  let root = successful_parse.into_iter().next().unwrap();
  if root.as_rule() != Rule::Program {
    return Err("Expected a Program rule, but found a different rule.".into());
  }

  let inner = root.into_inner().next().unwrap();
  process_sequence_no_visit(inner.clone(), v)?;
  v.v_end(inner);

  Ok(())
}
