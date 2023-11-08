use crate::ast_visitor::Visitor;
use pest::iterators::Pair;

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

pub struct FormatterVisitor<'a> {
  out: &'a mut dyn std::io::Write,
  top: FormatterTop,
  line_length: usize,
  line_counter: usize,
  depth: usize,
  last_char: Option<usize>,
  context_stack: Vec<Context>,
  input: String,
}

enum UserLine {
  Newline,
  Comment(String),
}

#[derive(Default)]
struct UserStyling {
  lines: Vec<UserLine>,
}

struct QuoteState {
  start: usize,
  num_quotes: i32,
}

impl<'a> FormatterVisitor<'a> {
  pub fn new(out: &'a mut dyn std::io::Write, input: &str) -> Self {
    Self {
      out,
      top: FormatterTop::None,
      line_length: 0,
      line_counter: 0,
      depth: 0,
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
        if c == ';' && comment_start.is_none() {
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
            self.newline();
          }
          UserLine::Comment(line) => {
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
    self.out.write_all(s.as_bytes()).unwrap();
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
    let d = self.depth as i32 + offset;
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

  fn filter<'b>(&mut self, v: &'b str) -> String {
    let mut result = String::new();
    let mut comment = false;
    let mut quote_open: Option<QuoteState> = None;
    let mut quote_close: Option<QuoteState> = None;
    for (i, c) in v.chars().enumerate() {
      if !comment && c == '"' {
        match &mut quote_open {
          None => {
            quote_open = Some(QuoteState {
              start: i,
              num_quotes: 1,
            });
          }
          Some(qo) => {
            if qo.num_quotes < 3 && (i - 1) == qo.start {
              qo.num_quotes += 1;
              qo.start = i;
            } else {
              match &mut quote_close {
                None => {
                  quote_close = Some(QuoteState {
                    start: i,
                    num_quotes: 1,
                  });
                }
                Some(qc) => {
                  qc.num_quotes += 1;
                  if qc.num_quotes == qo.num_quotes {
                    quote_close = None;
                    quote_open = None;
                  }
                }
              }
            }
          }
        };
      } else {
        quote_close = None;
      }
      if quote_open.is_some() {
        result.push(c);
        continue;
      }
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

  fn write_func_after_open<F: FnOnce(&mut Self)>(&mut self, pair: &Pair<Rule>, inner: F) {
    let start_line = self.line_counter;
    let omit_indent = omit_shard_param_indent(pair.clone());
    if omit_indent {
      inner(self);
    } else {
      self.depth += 1;
      inner(self);
      self.depth -= 1;
    }

    if !omit_indent && start_line != self.line_counter {
      self.newline();
    }
    self.write_joined(")");
  }
}

// Checks if this is a function without any parameters
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

fn omit_builtin_params(func: Pair<Rule>) -> bool {
  let mut inner = func.clone().into_inner();
  let _name = inner.next().unwrap();
  if let Some(_) = inner.next() {
    return false;
  }
  return true;
}

// Checks if all param starting points are on the same line as the function name
// Examples are:
// Once({... , If(Something {..., etc.
fn omit_shard_param_indent(func: Pair<Rule>) -> bool {
  let mut inner = func.clone().into_inner();
  let _name = inner.next().unwrap();
  let start_line = _name.line_col().0;
  if let Some(params) = inner.next() {
    let params = params.into_inner();
    let end_line = params
      .clone()
      .last()
      .map(|x| x.as_span().end_pos().line_col().0)
      .unwrap_or(start_line);
    for param in params {
      let mut param_inner = param.into_inner();
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

      let col = v.line_col().0;
      if col != start_line && col != end_line {
        return false;
      }
    }
  }
  return true;
}

impl<'a> Visitor for FormatterVisitor<'a> {
  fn v_pipeline<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.with_context(Context::Pipeline, |s| {
      s.interpolate(&pair);
      inner(s);
    });
  }
  fn v_stmt<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    inner(self);
  }
  fn v_value(&mut self, pair: Pair<Rule>) {
    self.interpolate(&pair);

    let ctx = self.get_context();
    if self.top == FormatterTop::Atom {
      if ctx == Context::Pipeline {
        self.write_atom("|");
      }
    }

    let inner = pair.clone().into_inner();
    if inner.len() > 0 {
      self.write_atom(inner.as_str());
      let last = inner.last().unwrap();
      self.set_last_char(last.as_span().end());
    } else {
      self.write_atom(pair.as_str());
      self.set_last_char(pair.as_span().end());
    }
  }
  fn v_assign<T: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    op: Pair<Rule>,
    to: Pair<Rule>,
    inner: T,
  ) {
    self.interpolate(&pair);
    inner(self);
    let op = self.filter(op.as_str());
    let to = self.filter(to.as_str());
    self.write_atom(&format!("{} {}", op, to));
  }
  fn v_func<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, name: Pair<Rule>, inner: T) {
    let ctx = self.get_context();
    self.with_context(Context::Unknown, |_self| {
      _self.interpolate_at_pos(pair.as_span().start());

      if _self.top == FormatterTop::Atom {
        if ctx == Context::Pipeline {
          _self.write_atom("|");
        }
      }

      let name_str = _self.filter(name.as_str());
      match pair.as_rule() {
        Rule::Func => {
          if omit_builtin_params(pair.clone()) {
            // This is most likely a @define
            _self.write_atom(&format!("@{}", name_str));
          } else {
            _self.write(&format!("@{}(", name_str), FormatterTop::None);
            _self.write_func_after_open(&pair, inner);
            if name_str == "wire"
              || name_str == "define"
              || name_str == "template"
              || name_str == "mesh"
              || name_str == "schedule"
            {
              _self.top = FormatterTop::LineFunc;
            }
          }
        }
        _ => {
          if omit_shard_params(pair.clone()) {
            _self.write_atom(&format!("{}", name_str));
          } else {
            _self.write(&format!("{}(", name_str), FormatterTop::None);
            _self.write_func_after_open(&pair, inner);
          }
        }
      }

      let last = pair.clone().into_inner().last().unwrap();
      _self.set_last_char(last.as_span().end());
    });
  }

  fn v_param<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, kw: Option<Pair<Rule>>, inner: T) {
    self.with_context(Context::Unknown, |_self| {
      _self.interpolate(&pair);
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
      _self.interpolate_at_pos(start);
      _self.write("{", FormatterTop::None);
      let starting_line = _self.line_counter;

      _self.depth += 1;
      inner(_self);
      let end = pair.as_span().end();
      _self.interpolate_at_pos_ext(end, true);
      _self.depth -= 1;

      if _self.line_counter != starting_line {
        _self.newline();
      }
      _self.write_joined("}");
    });
  }
  fn v_table<T: FnOnce(&mut Self)>(&mut self, pair: Pair<Rule>, inner: T) {
    self.interpolate(&pair);
    self.write("{", FormatterTop::None);
    let start_line = self.line_counter;
    self.depth += 1;
    inner(self);
    self.depth -= 1;
    if self.line_counter != start_line {
      self.newline();
    }
    self.write_joined("}");
    self.set_last_char(pair.as_span().end());
  }
  fn v_table_val<TK: FnOnce(&mut Self), TV: FnOnce(&mut Self)>(
    &mut self,
    pair: Pair<Rule>,
    _key: Pair<Rule>,
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
  fn v_take_seq(&mut self, pair: Pair<Rule>) {
    let str = self.filter(pair.as_str());
    self.write_atom(&str);
  }
  fn v_take_table(&mut self, pair: Pair<Rule>) {
    let str = self.filter(pair.as_str());
    self.write_atom(&str);
  }
}
