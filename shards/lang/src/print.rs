#[cfg(test)]
use crate::read::process_program;
#[cfg(test)]
use pest::Parser;

use crate::types::*;

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

impl Value {
  fn to_string(&self) -> String {
    match self {
      Value::None => String::from("none"),
      Value::Identifier(s) => s.clone(),
      Value::Boolean(b) => format!("{}", b),
      Value::Enum(a, b) => format!("{}.{}", a, b),
      Value::Number(n) => n.to_string(),
      Value::String(s) => {
        // if within the string there are quotes, we need to escape them
        let escaped = s.replace("\"", "\\\"");
        format!("\"{}\"", escaped)
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
          .map(|v| v.to_string())
          .collect::<Vec<String>>()
          .join(" ");
        format!("[{}]", values_str)
      }
      Value::TakeTable(name, path) => {
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
          .map(|(k, v)| format!("{}: {}", k.to_string(), v.to_string()))
          .collect::<Vec<String>>()
          .join(" ");
        format!("{{{}}}", values_str)
      }
      Value::Shards(seq) => format!("{{{}}}", seq.to_string()),
      Value::EvalExpr(seq) => format!("#({})", seq.to_string()),
      Value::Expr(seq) => format!("({})", seq.to_string()),
    }
  }
}

impl Param {
  fn to_string(&self) -> String {
    match &self.name {
      Some(name) => format!("{}: {}", name, self.value.to_string()),
      None => self.value.to_string(),
    }
  }
}

impl Function {
  fn to_string(&self) -> String {
    let params: Vec<String> = match &self.params {
      Some(params) => params.iter().map(|p| p.to_string()).collect(),
      None => vec![],
    };
    if params.is_empty() {
      self.name.clone()
    } else {
      format!("{}({})", self.name, params.join(" "))
    }
  }
}

impl BlockContent {
  fn to_string(&self) -> String {
    match self {
      BlockContent::Shard(func) => func.to_string(),
      BlockContent::BuiltIn(func) => format!("@{}", func.to_string()),
      BlockContent::Shards(seq) => format!("{{{}}}", seq.to_string()),
      BlockContent::Const(value) => value.to_string(),
      BlockContent::TakeTable(name, path) => {
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
      BlockContent::EvalExpr(seq) => format!("#({})", seq.to_string()),
      BlockContent::Expr(seq) => format!("({})", seq.to_string()),
    }
  }
}

impl Block {
  fn to_string(&self) -> String {
    self.content.to_string()
  }
}

impl Pipeline {
  fn to_string(&self) -> String {
    let blocks_str = self
      .blocks
      .iter()
      .map(|b| b.to_string())
      .collect::<Vec<String>>()
      .join(" | ");
    format!("{}", blocks_str)
  }
}

impl Assignment {
  fn to_string(&self) -> String {
    match self {
      Assignment::AssignRef(pipeline, name) => format!("{} = {}", pipeline.to_string(), name),
      Assignment::AssignSet(pipeline, name) => format!("{} >= {}", pipeline.to_string(), name),
      Assignment::AssignUpd(pipeline, name) => format!("{} > {}", pipeline.to_string(), name),
      Assignment::AssignPush(pipeline, name) => format!("{} >> {}", pipeline.to_string(), name),
    }
  }
}

impl Statement {
  fn to_string(&self) -> String {
    match self {
      Statement::Assignment(assign) => assign.to_string(),
      Statement::Pipeline(pipeline) => pipeline.to_string(),
    }
  }
}

impl Sequence {
  fn to_string(&self) -> String {
    let statements_str = self
      .statements
      .iter()
      .map(|s| s.to_string())
      .collect::<Vec<String>>()
      .join("\n");
    format!("{}", statements_str)
  }
}

pub fn print_ast(ast: &Sequence) -> String {
  ast.to_string()
}

#[test]
fn test_print1() {
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let seq = process_program(successful_parse.into_iter().next().unwrap()).unwrap();
  let s = print_ast(&seq);
  println!("{}", s);
}
