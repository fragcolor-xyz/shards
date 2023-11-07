use ast_visitor::{process, Env, Visitor};
use pest::iterators::Pair;
use shards_lang_core::{
  ast::{Identifier, Number, Value},
  RcStrWrapper,
};
use std::{fmt::Display, fs, io::Write};

mod ast_visitor;
mod error;
mod span;
use error::*;

pub type Rule = shards_lang_core::ast::Rule;

// Identifies the last written value type
#[derive(PartialEq, Debug)]
enum FormatterTop {
  None,
  Atom,
  Comment,
  LineFunc,
}

#[derive(PartialEq, Clone, Copy, Debug)]
enum Context {
  Unknown,
  Pipeline,
  Seq,
  Table,
}

struct FormatterVisitor {
  file: fs::File,
  top: FormatterTop, // stream: fs::Writer
  line_length: usize,
  line_counter: usize,
  depth: usize,
  line_func_depth: usize,
  last_char: Option<usize>,
  context_stack: Vec<Context>,
  input: String,
}

fn strip_whitespace(s: &str) -> String {
  s.replace(&[' ', '\r', '\n'][..], "")
}

enum UserLine {
  Newline,
  Comment(String),
}

#[derive(Default)]
struct UserStyling {
  lines: Vec<UserLine>,
}

impl FormatterVisitor {
  fn new(output_file: fs::File, input: &str) -> Self {
    Self {
      file: output_file,
      top: FormatterTop::None,
      line_length: 0,
      line_counter: 0,
      depth: 0,
      line_func_depth: 0,
      input: input.into(),
      last_char: None,
      context_stack: vec![Context::Unknown],
    }
  }

  fn get_context(&self) -> Context {
    *self.context_stack.last().unwrap()
  }

  fn with_context<F: FnOnce(&mut Self)>(&mut self, ctx: Context, inner: F) {
    self.context_stack.push(ctx);
    inner(self);
    self.context_stack.pop();
  }

  fn set_last_char(&mut self, ptr: usize) {
    self.last_char = Some(ptr);
  }

  fn extract_styling(&self, until: usize) -> Option<UserStyling> {
    if let Some(from) = self.last_char {
      if until <= from {
        return None;
      }

      let mut us: UserStyling = UserStyling::default();
      let mut comment_start: Option<usize> = None;
      let interpolated = &self.input[from..until];
      for (i, c) in interpolated.chars().enumerate() {
        if c == ';' {
          comment_start = Some(i + from + 1);
        } else if c == '\n' {
          if let Some(start) = comment_start {
            let comment = &self.input[start..i + from];
            let comment_str = comment.trim_start().to_string();
            us.lines.push(UserLine::Comment(comment_str));
            comment_start = None;
          } else {
            us.lines.push(UserLine::Newline);
          }
        }
      }
      if let Some(start) = comment_start {
        let comment = &self.input[start..until];
        us.lines.push(UserLine::Comment(comment.into()));
      }
      if !us.lines.is_empty() {
        // println!("Int7e> ({} lines) {}", us.lines.len(), interpolated);
        return Some(us);
      }
    }
    None
  }

  fn interpolate(&mut self, new_pair: &Pair<Rule>) {
    self.interpolate_at_pos(new_pair.as_span().start());
  }

  // Will interpolate any user-defined comments and newline styling
  // up until the given position
  fn interpolate_at_pos(&mut self, ptr: usize) {
    self.interpolate_at_pos_ext(ptr, false);
  }

  fn interpolate_at_pos_ext(&mut self, ptr: usize, strip_final_newline: bool) {
    if let Some(us) = self.extract_styling(ptr) {
      for (i, line) in us.lines.iter().enumerate() {
        match line {
          UserLine::Newline => {
            if i == us.lines.len() - 1 && strip_final_newline {
              continue;
            }
            // println!("NL>");
            self.newline();
          }
          UserLine::Comment(line) => {
            // println!("CMNT> {}", line);
            self.write(&format!("; {}", line), FormatterTop::Comment);
            if i < us.lines.len() - 1 {
              self.newline();
            }
          }
        }
      }
    }
    self.set_last_char(ptr);
  }

  fn write_raw(&mut self, s: &str) {
    for c in s.chars() {
      if c == '\n' {
        self.line_length = 0;
      } else {
        self.line_length += 1;
      }
    }
    self.file.write_all(s.as_bytes()).unwrap();
  }

  fn write(&mut self, s: &str, top: FormatterTop) {
    self.write_pre_space();
    self.write_raw(s);
    self.top = top;
  }

  fn newline_w_offset(&mut self, offset: i32) {
    self.write_raw("\n");
    self.line_length = 0;
    self.line_counter += 1;
    let d = usize::max(self.depth, self.line_func_depth) as i32 + offset;
    if d > 0 {
      for _ in 0..d {
        self.write_raw("  ");
      }
    }
    self.top = FormatterTop::None;
  }

  fn newline(&mut self) {
    self.newline_w_offset(0);
  }

  fn write_pre_space(&mut self) {
    match self.top {
      FormatterTop::None => {}
      FormatterTop::Atom => {
        self.write_raw(" ");
      }
      FormatterTop::Comment => {
        self.newline();
      }
      FormatterTop::LineFunc => {
        self.newline();
      }
    }
    self.top = FormatterTop::None;
  }

  // Write while spacing between previous atoms
  fn write_atom(&mut self, s: &str) {
    self.write(s, FormatterTop::Atom);
  }

  // Write without spacing between atomics (for symbols and such)
  fn write_joined(&mut self, s: &str) {
    match self.top {
      FormatterTop::Atom | FormatterTop::None => {
        self.write_raw(s);
      }
      FormatterTop::Comment | FormatterTop::LineFunc => {
        self.newline();
        self.write_raw(s);
      }
    }
    self.top = FormatterTop::Atom;
  }

  fn filter<'a>(&mut self, v: &'a str) -> String {
    let mut result = String::new();
    let mut comment = false;
    for c in v.chars() {
      if c == ';' {
        comment = true;
      }
      if c == '\n' || c == '\r' {
        comment = false;
        continue;
      }
      if c == ' ' || c == '\t' {
        continue; // Ignore whitespace
      }
      if !comment {
        result.push(c);
      }
    }
    result
  }
}

fn omit_shard_params(func: Pair<Rule>) -> bool {
  let mut inner = func.clone().into_inner();
  let _name = inner.next().unwrap();
  if let Some(params) = inner.next() {
    let params: pest::iterators::Pairs<'_, shards_lang_core::ast::Rule> = params.into_inner();
    let num_params = params.len();
    if num_params == 0 {
      return true;
    }
  } else {
    return true;
  }
  return false;
}

fn omit_shard_param_indent(func: Pair<Rule>) -> bool {
  let mut inner = func.clone().into_inner();
  let _name = inner.next().unwrap();
  if let Some(params) = inner.next() {
    let mut params = params.into_inner();
    // println!("Params> {}", params);
    let num_params = params.len();
    if num_params == 1 {
      let mut param_inner = params.next().unwrap().into_inner();
      let v = if param_inner.len() == 2 {
        param_inner.next();
        param_inner.next()
      } else {
        param_inner.next()
      }
      .unwrap()
      .into_inner()
      .next()
      .unwrap();
      // println!("l {}", v);
      // let k = param_inner.next();
      // let v = param_inner.next().unwrap();

      // println!("first> {}", v);
      // println!("first> {}", v.as_str());
      if let Rule::Shards = v.as_rule() {
        return true;
      }
    }
  }
  return false;
}

impl Visitor for FormatterVisitor {
  fn v_pipeline<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.with_context(Context::Pipeline, |s| {
      s.interpolate(&pair);
      inner(s);
    });
  }
  fn v_stmt<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    // println!("Stmt> {}", pair.as_str());
    inner(self);
  }
  fn v_value<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    // println!("Value> {}", pair.as_str());
    let val = self.filter(pair.as_str());
    self.write_atom(&format!("{}", val));
    inner(self);
  }
  fn v_assign<T: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    op: Pair<Rule>,
    to: Pair<Rule>,
    inner: T,
  ) {
    self.interpolate(&pair);
    // println!("Assign> {}", pair.as_str());
    inner(self);
    let op = self.filter(op.as_str());
    let to = self.filter(to.as_str());
    self.write_atom(&format!("{} {}", op, to));
  }
  fn v_func<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, name: Pair<Rule>, inner: T) {
    let ctx = self.get_context();
    self.with_context(Context::Unknown, |_self| {
      _self.interpolate_at_pos(pair.as_span().start());

      let name_str = _self.filter(name.as_str());
      // println!("Func> {}, {}", name, pair.as_str());

      if _self.top == FormatterTop::Atom {
        // println!("Ctx> {:?}", ctx);
        if ctx == Context::Pipeline {
          _self.write_atom("|");
        }
      }

      match pair.as_rule() {
        Rule::Func => {
          _self.write(&format!("@{}(", name_str), FormatterTop::None);
          _self.interpolate_at_pos(name.as_span().end());
          let prev_d = _self.line_func_depth;
          _self.line_func_depth = _self.depth + 1;
          inner(_self);
          _self.line_func_depth = prev_d;
          _self.write_joined(")");
          _self.top = FormatterTop::Atom;
          if name_str == "wire"
            || name_str == "define"
            || name_str == "template"
            || name_str == "mesh"
            || name_str == "schedule"
            || name_str == "run"
          {
            _self.top = FormatterTop::LineFunc;
          }
        }
        _ => {
          if omit_shard_params(pair.clone()) {
            _self.write_atom(&format!("{}", name_str));
          } else {
            _self.write(&format!("{}(", name_str), FormatterTop::None);
            let start_line = _self.line_counter;
            let omit_indent = omit_shard_param_indent(pair.clone());
            if omit_indent {
              inner(_self);
            } else {
              _self.depth += 1;
              inner(_self);
              _self.depth -= 1;
            }

            if !omit_indent && start_line != _self.line_counter {
              _self.newline();
            }
            _self.write_joined(")");
          }
        }
      }
      _self.set_last_char(pair.as_span().end());
    });
  }
  fn v_param<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, kw: Option<Pair<Rule>>, inner: T) {
    self.with_context(Context::Unknown, |_self| {
      _self.interpolate(&pair);
      // println!(
      //   "Param> {}, {:?}",
      //   kw.map_or("<none>", |f| f.as_str()),
      //   pair.as_str()
      // );
      if let Some(kw) = kw {
        let kw_str = _self.filter(kw.as_str());
        _self.write_atom(&format!("{}", kw_str));
        _self.interpolate_at_pos(kw.as_span().end());
      }
      inner(_self);
    });
  }
  fn v_seq<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.with_context(Context::Seq, |_self| {
      _self.interpolate(&pair);
      // println!("Seq> {}", pair.as_str());
      _self.write("[", FormatterTop::None);
      _self.depth += 1;
      inner(_self);
      _self.depth -= 1;
      _self.write_joined("]");
    });
  }
  fn v_expr<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    self.write("(", FormatterTop::None);
    self.depth += 1;
    inner(self);
    self.depth -= 1;
    self.write_joined(")");
  }
  fn v_eval_expr<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    self.write("#(", FormatterTop::None);
    self.depth += 1;
    inner(self);
    self.depth -= 1;
    self.write_joined(")");
  }
  fn v_shards<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.with_context(Context::Pipeline, |_self| {
      let start = pair.as_span().start();
      // println!("Shards> {}", pair.as_str());
      _self.interpolate_at_pos(start);
      _self.write("{", FormatterTop::None);
      let starting_line = _self.line_counter;

      _self.depth += 1;
      inner(_self);
      let end = pair.as_span().end();
      _self.interpolate_at_pos_ext(end, true);
      _self.depth -= 1;

      if _self.line_counter != starting_line {
        if _self.line_func_depth == (_self.depth + 1) {
          // Remove indent before }) closing of @template, etc.
          _self.newline_w_offset(-1);
        } else {
          _self.newline();
        }
      }
      _self.write_joined("}");
    });
  }
  fn v_table<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    self.write("{", FormatterTop::None);
    self.depth += 1;
    inner(self);
    self.depth -= 1;
    self.write_joined("}");
  }
  fn v_table_val<TK: FnOnce(&mut Self), TV: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    key: Pair<Rule>,
    inner_key: TK,
    inner_val: TV,
  ) {
    self.with_context(Context::Table, |_self| {
      _self.interpolate(&pair);
      inner_key(_self);
      _self.write_joined(":");
      inner_val(_self);
    });
  }
}

fn main() {
  let str = fs::read_to_string("C:/Projects/shards/shards/tests/gfx-effect.shs").unwrap();
  // println!("{}", &str);
  let mut env = Env::default();

  let file = fs::File::create("out.shs").unwrap();
  let mut v = FormatterVisitor::new(file, &str);
  process(&str, &mut v, &mut env).unwrap();

  // process(&str, &mut env).unwrap();
}
