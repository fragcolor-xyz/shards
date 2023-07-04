#[cfg(test)]
use crate::read::process_program;
#[cfg(test)]
use pest::Parser;

use crate::ast::*;
#[cfg(test)]
use crate::read::ReadEnv;

const INDENT_LENGTH: usize = 2;

impl Number {
  fn to_string(&self) -> String {
    match self {
      Number::Integer(i) => format!("{}", i),
      Number::Float(f) => format!("{}", f),
      Number::Hexadecimal(s) => format!("{}", s),
    }
  }
}

fn format_f64(f: f64) -> String {
  if f.fract() == 0.0 {
    format!("{:.1}", f)
  } else {
    format!("{}", f)
  }
}

fn format_f32(f: f32) -> String {
  if f.fract() == 0.0 {
    format!("{:.1}", f)
  } else {
    format!("{}", f)
  }
}

struct Context {
  indent: usize,
  previous: Option<*const Context>,
}

impl Default for Context {
  fn default() -> Self {
    Context {
      indent: 0,
      previous: None,
    }
  }
}

impl Value {
  fn to_string(&self, context: &mut Context) -> String {
    match self {
      Value::None => String::from("none"),
      Value::Identifier(s) => s.to_string(),
      Value::Boolean(b) => format!("{}", b),
      Value::Enum(a, b) => format!("{}.{}", a, b),
      Value::Number(n) => n.to_string(),
      Value::String(s) => {
        // if within the string there are quotes, we need to escape them
        let escaped = s.replace("\"", "\\\"");
        format!("\"{}\"", escaped)
      }
      Value::Bytes(b) => {
        let bytes_slice = b.0.as_ref();
        // build a big hex string from the bytes
        let mut hex_string = String::new();
        for byte in bytes_slice {
          hex_string.push_str(&format!("{:02x}", byte));
        }
        format!("@bytes(0x{})", hex_string)
      }
      Value::Int2(arr) => format!("({} {})", arr[0], arr[1]),
      Value::Int3(arr) => format!("({} {} {})", arr[0], arr[1], arr[2]),
      Value::Int4(arr) => format!("({} {} {} {})", arr[0], arr[1], arr[2], arr[3]),
      Value::Float2(arr) => format!("({} {})", format_f64(arr[0]), format_f64(arr[1])),
      Value::Float3(arr) => format!(
        "({} {} {})",
        format_f32(arr[0]),
        format_f32(arr[1]),
        format_f32(arr[2])
      ),
      Value::Float4(arr) => format!(
        "({} {} {} {})",
        format_f32(arr[0]),
        format_f32(arr[1]),
        format_f32(arr[2]),
        format_f32(arr[3])
      ),
      Value::Seq(values) => {
        let values_str = values
          .iter()
          .map(|v| v.to_string(context))
          .collect::<Vec<String>>()
          .join(" ");
        format!("[{}]", values_str)
      }
      Value::TakeTable(name, path) => {
        let path = path.iter().map(|p| p.to_string()).collect::<Vec<String>>();
        let path = path.join(":");
        format!("{}:{}", name, path)
      }
      Value::TakeSeq(name, path) => {
        let path_str = path
          .iter()
          .map(|p| p.to_string())
          .collect::<Vec<String>>()
          .join(":");
        format!("{}:{}", name, path_str)
      }
      Value::Int8(arr) => format!(
        "({} {} {} {} {} {} {} {})",
        arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6], arr[7]
      ),
      Value::Int16(arr) => format!(
        "({} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {})",
        arr[0],
        arr[1],
        arr[2],
        arr[3],
        arr[4],
        arr[5],
        arr[6],
        arr[7],
        arr[8],
        arr[9],
        arr[10],
        arr[11],
        arr[12],
        arr[13],
        arr[14],
        arr[15]
      ),
      Value::Table(values) => {
        let values_str = values
          .iter()
          .map(|(k, v)| format!("{}: {}", k.to_string(context), v.to_string(context)))
          .collect::<Vec<String>>()
          .join(" ");
        format!("{{{}}}", values_str)
      }
      Value::Shards(seq) => format!("{{{}}}", seq.to_string(context)),
      Value::Shard(func) => format!("{}", func.to_string(context)),
      Value::EvalExpr(seq) => format!("#({})", seq.to_string(context)),
      Value::Expr(seq) => format!("({})", seq.to_string(context)),
      Value::Func(func) => format!("@{}", func.to_string(context)),
    }
  }
}

impl Param {
  fn to_string(&self, context: &mut Context) -> String {
    match &self.name {
      Some(name) => format!("{}: {}", name, self.value.to_string(context)),
      None => self.value.to_string(context),
    }
  }
}

impl Function {
  fn to_string(&self, context: &mut Context) -> String {
    let params: Vec<String> = match &self.params {
      Some(params) => params.iter().map(|p| p.to_string(context)).collect(),
      None => vec![],
    };
    if params.is_empty() {
      self.name.to_string()
    } else {
      format!("{}({})", self.name, params.join(" "))
    }
  }
}

impl BlockContent {
  fn to_string(&self, context: &mut Context) -> String {
    match self {
      BlockContent::Shard(func) => func.to_string(context),
      BlockContent::Func(func) => format!("@{}", func.to_string(context)),
      BlockContent::Shards(seq) => format!("{{{}}}", seq.to_string(context)),
      BlockContent::Const(value) => value.to_string(context),
      BlockContent::TakeTable(name, path) => {
        let path = path.iter().map(|p| p.to_string()).collect::<Vec<String>>();
        let path = path.join(":");
        format!("{}:{}", name, path)
      }
      BlockContent::TakeSeq(name, path) => {
        let path_str = path
          .iter()
          .map(|p| p.to_string())
          .collect::<Vec<String>>()
          .join(":");
        format!("{}:{}", name, path_str)
      }
      BlockContent::EvalExpr(seq) => format!("#({})", seq.to_string(context)),
      BlockContent::Expr(seq) => format!("({})", seq.to_string(context)),
      BlockContent::Embed(seq) => format!("{}", seq.to_string(context)),
    }
  }
}

impl Block {
  fn to_string(&self, context: &mut Context) -> String {
    self.content.to_string(context)
  }
}

impl Pipeline {
  fn to_string(&self, context: &mut Context) -> String {
    let blocks_str = self
      .blocks
      .iter()
      .map(|b| b.to_string(context))
      .collect::<Vec<String>>()
      .join(" | ");
    format!("{}", blocks_str)
  }
}

impl Assignment {
  fn to_string(&self, context: &mut Context) -> String {
    match self {
      Assignment::AssignRef(pipeline, name) => {
        format!("{} = {}", pipeline.to_string(context), name)
      }
      Assignment::AssignSet(pipeline, name) => {
        format!("{} >= {}", pipeline.to_string(context), name)
      }
      Assignment::AssignUpd(pipeline, name) => {
        format!("{} > {}", pipeline.to_string(context), name)
      }
      Assignment::AssignPush(pipeline, name) => {
        format!("{} >> {}", pipeline.to_string(context), name)
      }
    }
  }
}

impl Statement {
  fn to_string(&self, context: &mut Context) -> String {
    match self {
      Statement::Assignment(assign) => assign.to_string(context),
      Statement::Pipeline(pipeline) => pipeline.to_string(context),
    }
  }
}

impl Sequence {
  fn to_string(&self, context: &mut Context) -> String {
    let mut inner_context = Context::default();
    inner_context.previous = Some(context);
    if self.statements.len() > 1 && context.previous.is_some() {
      inner_context.indent = context.indent + INDENT_LENGTH;
    }

    let statements_str = self
      .statements
      .iter()
      .map(|s| s.to_string(&mut inner_context))
      .collect::<Vec<String>>()
      .join("\n");

    let indent_str = " ".repeat(inner_context.indent);
    let new_line = if inner_context.indent > 0 { "\n" } else { "" };
    let indented_blocks_str = statements_str.replace("\n", &format!("\n{}", indent_str));
    format!(
      "{}{}{}{}",
      new_line, indent_str, indented_blocks_str, new_line
    )
  }
}

pub fn print_ast(ast: &Sequence) -> String {
  let mut context = Context::default();
  ast.to_string(&mut context)
}

#[test]
fn test_print1() {
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new(".");
  let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();
  let s = print_ast(&seq);
  println!("{}", s);
}

#[test]
fn test_print2() {
  let code = include_str!("explained.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new(".");
  let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();
  let s = print_ast(&seq);
  println!("{}", s);
}
