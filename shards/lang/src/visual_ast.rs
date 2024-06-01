use crate::{ast_visitor::*, formatter::Rule};

pub struct VisualAst {
  indent: usize,
}

impl Visitor for VisualAst {
  fn v_pipeline<T: FnOnce(&mut Self)>(&mut self, pair: pest::iterators::Pair<Rule>, inner: T) {
    todo!()
  }

  fn v_stmt<T: FnOnce(&mut Self)>(&mut self, pair: pest::iterators::Pair<Rule>, inner: T) {
    todo!()
  }

  fn v_value(&mut self, pair: pest::iterators::Pair<Rule>) {
    todo!()
  }

  fn v_assign<T: FnOnce(&mut Self)>(
    &mut self,
    pair: pest::iterators::Pair<Rule>,
    op: pest::iterators::Pair<Rule>,
    to: pest::iterators::Pair<Rule>,
    inner: T,
  ) {
    todo!()
  }

  fn v_func<T: FnOnce(&mut Self)>(
    &mut self,
    pair: pest::iterators::Pair<Rule>,
    name: pest::iterators::Pair<Rule>,
    inner: T,
  ) {
    todo!()
  }

  fn v_param<T: FnOnce(&mut Self)>(
    &mut self,
    pair: pest::iterators::Pair<Rule>,
    kw: Option<pest::iterators::Pair<Rule>>,
    inner: T,
  ) {
    todo!()
  }

  fn v_seq<T: FnOnce(&mut Self)>(&mut self, pair: pest::iterators::Pair<Rule>, inner: T) {
    todo!()
  }

  fn v_table<T: FnOnce(&mut Self)>(&mut self, pair: pest::iterators::Pair<Rule>, inner: T) {
    todo!()
  }

  fn v_table_val<TK: FnOnce(&mut Self), TV: FnOnce(&mut Self)>(
    &mut self,
    pair: pest::iterators::Pair<Rule>,
    key: pest::iterators::Pair<Rule>,
    inner_key: TK,
    inner_val: TV,
  ) {
    todo!()
  }

  fn v_eval_expr<T: FnOnce(&mut Self)>(
    &mut self,
    pair: pest::iterators::Pair<Rule>,
    inner_pair: pest::iterators::Pair<Rule>,
    inner: T,
  ) {
    todo!()
  }

  fn v_expr<T: FnOnce(&mut Self)>(
    &mut self,
    pair: pest::iterators::Pair<Rule>,
    inner_pair: pest::iterators::Pair<Rule>,
    inner: T,
  ) {
    todo!()
  }

  fn v_shards<T: FnOnce(&mut Self)>(&mut self, pair: pest::iterators::Pair<Rule>, inner: T) {
    todo!()
  }

  fn v_take_table(&mut self, pair: pest::iterators::Pair<Rule>) {
    todo!()
  }

  fn v_take_seq(&mut self, pair: pest::iterators::Pair<Rule>) {
    todo!()
  }
}

pub fn render_ast(code: &str) {
  let mut visitor = VisualAst { indent: 0 };
  process(code, &mut visitor).unwrap();
}