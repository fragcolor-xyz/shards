use pest::Parser;
use petgraph::dot::{Config, Dot};
use petgraph::graph::{Graph, NodeIndex};
use petgraph::Directed;
use serde::{Deserialize, Serialize};

use crate::custom_state::CustomStateContainer;
use crate::read::{process_program, ReadEnv};
use crate::{ast::*, RcStrWrapper};
use petgraph::visit::NodeRef;
use petgraph::visit::EdgeRef;
use std::fmt::Write;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ASTNode {
  Number(Number),
  Identifier(Identifier),
  Value(Value),
  Param(Param),
  Function(Function),
  Block(Block),
  Pipeline(Pipeline),
  Assignment(Assignment),
  Statement(Statement),
  Metadata(Metadata),
  Sequence(Sequence),
  Program(Program),
}

pub struct ASTGraph {
  graph: Graph<ASTNode, (), Directed>,
}

impl ASTGraph {
  pub fn new() -> Self {
    ASTGraph {
      graph: Graph::<ASTNode, (), Directed>::new(),
    }
  }

  pub fn add_node(&mut self, node: ASTNode) -> NodeIndex {
    self.graph.add_node(node)
  }

  pub fn add_edge(&mut self, parent: NodeIndex, child: NodeIndex) {
    self.graph.add_edge(parent, child, ());
  }

  pub fn build_program(&mut self, program: &Program) -> NodeIndex {
    let program_index = self.add_node(ASTNode::Program(program.clone()));
    let metadata_index = self.build_metadata(&program.metadata);
    self.add_edge(program_index, metadata_index);

    let sequence_index = self.build_sequence(&program.sequence);
    self.add_edge(program_index, sequence_index);

    program_index
  }

  fn build_metadata(&mut self, metadata: &Metadata) -> NodeIndex {
    self.add_node(ASTNode::Metadata(metadata.clone()))
  }

  fn build_sequence(&mut self, sequence: &Sequence) -> NodeIndex {
    let sequence_index = self.add_node(ASTNode::Sequence(sequence.clone()));

    for statement in &sequence.statements {
      let statement_index = self.build_statement(statement);
      self.add_edge(sequence_index, statement_index);
    }

    sequence_index
  }

  fn build_statement(&mut self, statement: &Statement) -> NodeIndex {
    let statement_index = self.add_node(ASTNode::Statement(statement.clone()));

    match statement {
      Statement::Assignment(assignment) => {
        let assignment_index = self.build_assignment(assignment);
        self.add_edge(statement_index, assignment_index);
      }
      Statement::Pipeline(pipeline) => {
        let pipeline_index = self.build_pipeline(pipeline);
        self.add_edge(statement_index, pipeline_index);
      }
    }

    statement_index
  }

  fn build_assignment(&mut self, assignment: &Assignment) -> NodeIndex {
    self.add_node(ASTNode::Assignment(assignment.clone()))
  }

  fn build_pipeline(&mut self, pipeline: &Pipeline) -> NodeIndex {
    let pipeline_index = self.add_node(ASTNode::Pipeline(pipeline.clone()));

    for block in &pipeline.blocks {
      let block_index = self.build_block(block);
      self.add_edge(pipeline_index, block_index);
    }

    pipeline_index
  }

  fn build_block(&mut self, block: &Block) -> NodeIndex {
    let block_index = self.add_node(ASTNode::Block(block.clone()));

    match &block.content {
      BlockContent::Empty => {}
      BlockContent::Shard(function) => {
        let function_index = self.build_function(function);
        self.add_edge(block_index, function_index);
      }
      BlockContent::Shards(sequence) => {
        let sequence_index = self.build_sequence(sequence);
        self.add_edge(block_index, sequence_index);
      }
      BlockContent::Const(value) => {
        let value_index = self.build_value(value);
        self.add_edge(block_index, value_index);
      }
      BlockContent::TakeTable(identifier, vec) => {
        let take_table_index = self.build_take_table(identifier, vec);
        self.add_edge(block_index, take_table_index);
      }
      BlockContent::TakeSeq(identifier, vec) => {
        let take_seq_index = self.build_take_seq(identifier, vec);
        self.add_edge(block_index, take_seq_index);
      }
      BlockContent::EvalExpr(sequence) => {
        let sequence_index = self.build_sequence(sequence);
        self.add_edge(block_index, sequence_index);
      }
      BlockContent::Expr(sequence) => {
        let sequence_index = self.build_sequence(sequence);
        self.add_edge(block_index, sequence_index);
      }
      BlockContent::Func(function) => {
        let function_index = self.build_function(function);
        self.add_edge(block_index, function_index);
      }
      BlockContent::Program(program) => {
        let program_index = self.build_program(program);
        self.add_edge(block_index, program_index);
      }
    }

    block_index
  }

  fn build_function(&mut self, function: &Function) -> NodeIndex {
    let function_index = self.add_node(ASTNode::Function(function.clone()));

    if let Some(params) = &function.params {
      for param in params {
        let param_index = self.build_param(param);
        self.add_edge(function_index, param_index);
      }
    }

    function_index
  }

  fn build_param(&mut self, param: &Param) -> NodeIndex {
    let param_index = self.add_node(ASTNode::Param(param.clone()));
    let value_index = self.build_value(&param.value);
    self.add_edge(param_index, value_index);

    param_index
  }

  fn build_value(&mut self, value: &Value) -> NodeIndex {
    let value_index = self.add_node(ASTNode::Value(value.clone()));

    match value {
      Value::Identifier(identifier) => {
        let identifier_index = self.build_identifier(identifier);
        self.add_edge(value_index, identifier_index);
      }
      Value::Seq(seq) => {
        for val in seq {
          let val_index = self.build_value(val);
          self.add_edge(value_index, val_index);
        }
      }
      Value::Table(table) => {
        for (key, val) in table {
          let key_index = self.build_value(key);
          let val_index = self.build_value(val);
          self.add_edge(value_index, key_index);
          self.add_edge(value_index, val_index);
        }
      }
      Value::Shard(function) => {
        let function_index = self.build_function(function);
        self.add_edge(value_index, function_index);
      }
      Value::Shards(sequence) => {
        let sequence_index = self.build_sequence(sequence);
        self.add_edge(value_index, sequence_index);
      }
      Value::EvalExpr(sequence) => {
        let sequence_index = self.build_sequence(sequence);
        self.add_edge(value_index, sequence_index);
      }
      Value::Expr(sequence) => {
        let sequence_index = self.build_sequence(sequence);
        self.add_edge(value_index, sequence_index);
      }
      Value::TakeTable(identifier, vec) => {
        let take_table_index = self.build_take_table(identifier, vec);
        self.add_edge(value_index, take_table_index);
      }
      Value::TakeSeq(identifier, vec) => {
        let take_seq_index = self.build_take_seq(identifier, vec);
        self.add_edge(value_index, take_seq_index);
      }
      Value::Func(function) => {
        let function_index = self.build_function(function);
        self.add_edge(value_index, function_index);
      }
      _ => {}
    }

    value_index
  }

  fn build_identifier(&mut self, identifier: &Identifier) -> NodeIndex {
    self.add_node(ASTNode::Identifier(identifier.clone()))
  }

  fn build_take_table(&mut self, identifier: &Identifier, vec: &Vec<RcStrWrapper>) -> NodeIndex {
    let identifier_index = self.build_identifier(identifier);
    let node = ASTNode::Value(Value::TakeTable(identifier.clone(), vec.clone()));
    let take_table_index = self.add_node(node);
    self.add_edge(take_table_index, identifier_index);
    for item in vec {
      let item_index = self.add_node(ASTNode::Value(Value::Identifier(Identifier {
        name: item.clone(),
        namespaces: vec![],
      })));
      self.add_edge(take_table_index, item_index);
    }
    take_table_index
  }

  fn build_take_seq(&mut self, identifier: &Identifier, vec: &Vec<u32>) -> NodeIndex {
    let identifier_index = self.build_identifier(identifier);
    let node = ASTNode::Value(Value::TakeSeq(identifier.clone(), vec.clone()));
    let take_seq_index = self.add_node(node);
    self.add_edge(take_seq_index, identifier_index);
    for item in vec {
      let item_index = self.add_node(ASTNode::Value(Value::Number(Number::Integer(*item as i64))));
      self.add_edge(take_seq_index, item_index);
    }
    take_seq_index
  }

  pub fn visualize(&self) -> String {
    let mut dot_string = String::new();
    writeln!(dot_string, "digraph {{").unwrap();
    writeln!(dot_string, "    node [shape=box];").unwrap();

    // Add nodes with custom labels
    for node_index in self.graph.node_indices() {
      let node = &self.graph[node_index];
      let node_type = match node {
        ASTNode::Number(_) => "Number",
        ASTNode::Identifier(_) => "Identifier",
        ASTNode::Value(_) => "Value",
        ASTNode::Param(_) => "Param",
        ASTNode::Function(_) => "Function",
        ASTNode::Block(_) => "Block",
        ASTNode::Pipeline(_) => "Pipeline",
        ASTNode::Assignment(_) => "Assignment",
        ASTNode::Statement(_) => "Statement",
        ASTNode::Metadata(_) => "Metadata",
        ASTNode::Sequence(_) => "Sequence",
        ASTNode::Program(_) => "Program",
      };
      writeln!(
        dot_string,
        "    {} [label=\"{} ({})\"];",
        node_index.index(),
        node_type,
        node_index.index()
      )
      .unwrap();
    }

    // Add edges
    for edge in self.graph.edge_references() {
      writeln!(
        dot_string,
        "    {} -> {};",
        edge.source().index(),
        edge.target().index()
      )
      .unwrap();
    }

    writeln!(dot_string, "}}").unwrap();
    dot_string
  }
}

#[test]
fn test1() {
  // Example usage
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new("", ".", ".");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();

  let mut ast_graph = ASTGraph::new();
  let program_index = ast_graph.build_program(&program);

  // Do something with the graph, like printing or analyzing it
  let viz = ast_graph.visualize();
  // write to _ file
  std::fs::write("ast.dot", viz).unwrap();
}
