use crate::cli;
use crate::custom_state::CustomStateContainer;
use crate::{
  ast::{self, *},
  RcStrWrapper,
};
use core::convert::TryInto;
use pest::iterators::{Pair, Pairs};
use pest::{Parser, Position};
use shards::shard::Shard;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, Context, ExposedTypes, InstanceData, ParamVar,
  SeqVar, Type, Types, Var, FRAG_CC, STRINGS_TYPES, STRING_TYPES, STRING_VAR_OR_NONE_SLICE,
};
use shards::{
  fourCharacterCode, ref_counted_object_type_impl, shard, shard_impl, shardsc, shlog_debug,
  shlog_error, shlog_trace,
};
use std::borrow::Cow;
use std::cell::{Ref, RefCell};
use std::collections::{HashMap, HashSet};
use std::iter::Cloned;
use std::mem::swap;
use std::ops::Sub;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Copy)]
enum ReadEnvType {
  Default,
  InlineTemplateSubstitution,
}

pub struct ReadEnv {
  name: RcStrWrapper,
  script_directory: String,
  include_directories: Vec<String>,
  included: RefCell<HashSet<RcStrWrapper>>,
  dependencies: RefCell<Vec<String>>,
  inline_templates: HashMap<Identifier, InlineTemplate>,
  substitutions: HashMap<Identifier, Value>,
  parent: Option<*const ReadEnv>,
  env_type: ReadEnvType,
  file_id: Option<u32>,
}

#[derive(Debug, Clone)]
struct InlineTemplate {
  args: Vec<Value>,
  shards: String,
}

pub type FileRegistryHandle = u8;

#[cfg(not(test))]
extern "C" {
  pub fn shlang_fr_static() -> *const FileRegistryHandle;
  pub fn shlang_fr_get_file_id(
    handle: *const FileRegistryHandle,
    path: shardsc::SHStringWithLen,
  ) -> u32;
}

#[cfg(not(test))]
fn get_debug_file_id(path: &str) -> Option<u32> {
  unsafe {
    let handle = shlang_fr_static();
    let id = shlang_fr_get_file_id(handle, path.into());
    if id == 0 {
      return None;
    } else {
      return Some(id);
    }
  }
}

// Test stub - returns None (no debug file tracking in tests)
#[cfg(test)]
fn get_debug_file_id(_path: &str) -> Option<u32> {
  None
}

impl ReadEnv {
  pub fn new_cwd(name: &str) -> Self {
    Self::new(name, ".".to_string(), vec![".".to_string()])
  }
  pub fn new_root(name: &str, script_directory: String) -> Self {
    Self::new(name, script_directory.clone(), vec![script_directory])
  }
  pub fn new(name: &str, script_directory: String, include_directories: Vec<String>) -> Self {
    Self {
      name: name.to_owned().into(),
      script_directory: script_directory,
      include_directories: include_directories,
      included: RefCell::new(HashSet::new()),
      dependencies: RefCell::new(Vec::new()),
      inline_templates: HashMap::new(),
      substitutions: HashMap::new(),
      parent: None,
      env_type: ReadEnvType::Default,
      file_id: get_debug_file_id(name),
    }
  }

  pub fn set_parent(&mut self, parent: *const ReadEnv) {
    self.parent = Some(parent);
    self.env_type = unsafe { (&*parent).env_type };
  }

  pub fn resolve_file(&self, name: &str) -> Result<PathBuf, String> {
    let script_dir = Path::new(&self.script_directory);
    let file_path = script_dir.join(name);
    if let Ok(canonical) = dunce::canonicalize(&file_path) {
      shlog_debug!("Found include {:?}", file_path);
      return Ok(canonical);
    }

    shlog_debug!("Tried include {:?} (not found)", file_path);

    self.resolve_include_path(name)
  }

  fn resolve_include_path(&self, name: &str) -> Result<PathBuf, String> {
    for dir in &self.include_directories {
      let script_dir = Path::new(&dir);
      let file_path = script_dir.join(name);
      let canonical = dunce::canonicalize(&file_path);
      if let Ok(canonical) = canonical {
        shlog_debug!("Found include {:?}", file_path);
        return Ok(canonical);
      }
      shlog_debug!("Tried include {:?} (not found)", file_path);
    }
    if let Some(parent) = self.parent {
      unsafe {
        return (*parent).resolve_include_path(name);
      }
    }
    return Err(format!("File not found: {}", name).to_string());
  }

  fn find_inline_template(&self, name: &Identifier) -> Option<&InlineTemplate> {
    if let Some(itempl) = self.inline_templates.get(name) {
      return Some(itempl);
    } else if let Some(parent) = self.parent {
      return unsafe { (*parent).find_inline_template(name) };
    }
    None
  }

  fn find_substitution(&self, name: &Identifier) -> Option<&Value> {
    if let ReadEnvType::InlineTemplateSubstitution = self.env_type {
      if let Some(value) = self.substitutions.get(name) {
        return Some(value);
      } else if let Some(parent) = self.parent {
        return unsafe { (*parent).find_substitution(name) };
      }
    }
    None
  }

  fn with_inline_template_scope<F, R>(&mut self, env_type: ReadEnvType, f: F) -> R
  where
    F: FnOnce(&mut Self) -> R,
  {
    let old_env_type = self.env_type;
    let mut new_substitutions = self.substitutions.clone();
    swap(&mut self.substitutions, &mut new_substitutions);
    self.env_type = env_type;
    let result = f(self);
    self.env_type = old_env_type;
    self.substitutions = new_substitutions;
    result
  }

  fn resolve_file_id(&self) -> u32 {
    if let Some(file_id) = self.file_id {
      return file_id;
    }

    if let Some(parent) = self.parent {
      return unsafe { (*parent).resolve_file_id() };
    }

    return 0;
  }

  fn make_line_info_from_pair<'a>(&self, pair: &Pair<'a, Rule>) -> LineInfo {
    let (line, column) = pair.line_col();
    LineInfo {
      line: line as u32,
      column: column as u32,
      file: self.resolve_file_id(),
    }
  }
}

pub fn get_dependencies<'a>(env: &'a ReadEnv) -> Ref<'_, Vec<String>> {
  env.dependencies.borrow()
}

pub fn get_root_env<'a>(env: &'a ReadEnv) -> &'a ReadEnv {
  let mut node: *const ReadEnv = env;
  unsafe {
    while (*node).parent.is_some() {
      node = (*node).parent.unwrap();
    }
    &*node
  }
}

fn check_included<'a>(name: &'a RcStrWrapper, env: &'a ReadEnv) -> bool {
  if env.included.borrow().contains(name) {
    true
  } else if let Some(parent) = env.parent {
    check_included(name, unsafe { &*parent })
  } else {
    false
  }
}

fn extract_identifier(env: &ReadEnv, pair: Pair<Rule>) -> Result<Identifier, ShardsError> {
  // so this can be either a simple identifier or a complex identifier
  // complex identifiers are separated by '/'
  // we want to return a vector of identifiers
  let mut identifiers = Vec::new();
  for pair in pair.into_inner() {
    let rule = pair.as_rule();
    match rule {
      Rule::LowIden => identifiers.push(pair.as_str().to_owned().into()),
      _ => {
        return Err(
          (
            "Unexpected rule in Identifier.",
            env.make_line_info_from_pair(&pair),
          )
            .into(),
        )
      }
    }
  }
  Ok(Identifier {
    name: identifiers.pop().unwrap(), // qed
    namespaces: identifiers,
    custom_state: CustomStateContainer::new(),
  })
}

fn err<'a>(env: &ReadEnv, message: &str, pair: &Pair<'a, Rule>) -> ShardsError {
  ShardsError {
    message: message.to_string(),
    loc: env.make_line_info_from_pair(pair),
  }
}
fn errr<'a, T>(env: &ReadEnv, message: &str, pair: &Pair<'a, Rule>) -> Result<T, ShardsError> {
  return Err(err(env, message, pair));
}

fn process_assignment(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Assignment, ShardsError> {
  if pair.as_rule() != Rule::Assignment {
    return errr(
      env,
      "Expected an Assignment rule, but found a different rule.",
      &pair,
    );
  }

  let mut inner = pair.clone().into_inner();

  let _pipeline = if let Some(next) = inner.peek() {
    if next.as_rule() == Rule::Pipeline {
      process_pipeline(
        inner.next().ok_or_else(|| {
          err(
            env,
            "Expected a Pipeline in Assignment, but found none.",
            &pair,
          )
        })?,
        env,
      )?
    } else {
      Pipeline {
        blocks: vec![Block {
          content: BlockContent::Empty,
          line_info: Some(env.make_line_info_from_pair(&pair)),
          custom_state: CustomStateContainer::new(),
        }],
      }
    }
  } else {
    unreachable!("Assignment should have at least one inner rule.")
  };

  let assignment_op = inner
    .next()
    .ok_or_else(|| {
      err(
        env,
        "Expected an AssignmentOp in Assignment, but found none.",
        &pair,
      )
    })?
    .as_str();

  let iden = inner.next().ok_or_else(|| {
    err(
      env,
      "Expected an Identifier in Assignment, but found none.",
      &pair,
    )
  })?;

  let identifier = extract_identifier(env, iden)?;

  let op = match assignment_op {
    "=" => Ok(AssignmentKind::AssignRef),
    ">=" => Ok(AssignmentKind::AssignSet),
    ">" => Ok(AssignmentKind::AssignUpd),
    ">>" => Ok(AssignmentKind::AssignPush),
    _ => errr(env, "Unexpected assignment operator.", &pair),
  }?;
  Ok(Assignment {
    kind: op,
    identifier,
    line_info: Some(env.make_line_info_from_pair(&pair)),
  })
}

enum FunctionValue {
  Const(Value),
  Function(Function),
  Program(Program),
}

fn extract_params_from_pairs(
  pairs: &mut Pairs<Rule>,
  env: &mut ReadEnv,
  context_pair: &Pair<Rule>,
) -> Result<Option<Vec<Param>>, ShardsError> {
  let params = match pairs.next() {
    Some(pair) => {
      if pair.as_rule() == Rule::Params {
        Some(process_params(pair, env)?)
      } else {
        return errr(env, "Expected Params in Shard", &pair);
      }
    }
    None => None,
  };
  Ok(params)
}

fn substitute_inline_template(
  itempl: &InlineTemplate,
  param_pairs: &mut Pairs<Rule>,
  env: &ReadEnv,
  context_pair: &Pair<Rule>,
) -> Result<String, ShardsError> {
  // Foreach param, match against InlineTemplate and substitute value in string
  let templ_args = &*itempl.args;
  let mut str = itempl.shards.clone();
  let mut i = 0;
  loop {
    if let Some(param) = param_pairs.next() {
      if i >= templ_args.len() {
        return errr(
          env,
          "Expected more arguments in InlineTemplate",
          context_pair,
        );
      }

      // let name = &templ_args[i];
      if let Value::Identifier(iden) = &templ_args[i] {
        let sw = (&iden).resolve();
        let src = sw.as_str();
        let dst = param.as_str();
        eprintln!("Subst {} => {}", src, dst);
        str = str.replace(src, dst);
      } else {
        return errr(env, "Parameter should be an identifier", context_pair);
      }
    } else {
      break;
    }
    i += 1;
  }
  Ok(str)
}

fn convert_to_function_value(
  identifier: Identifier,
  pairs: &mut Pairs<Rule>,
  env: &mut ReadEnv,
  context_pair: &Pair<Rule>,
) -> Result<FunctionValue, ShardsError> {
  let params = extract_params_from_pairs(pairs, env, context_pair)?;
  let itc: Option<InlineTemplate> = env.find_inline_template(&identifier).cloned();
  if let Some(itempl) = itc {
    let itempl = itempl.clone();
    let prog: Program =
      env.with_inline_template_scope(ReadEnvType::InlineTemplateSubstitution, |env| {
        let params = params.ok_or_else(|| err(env, "Expected parameters", context_pair))?;

        // Insert substitutions into environment
        if params.len() != itempl.args.len() {
          return errr(
            env,
            "Number of parameters does not match number of arguments in inline template",
            context_pair,
          );
        }
        for (param, arg) in params.iter().zip(&itempl.args) {
          if let Value::Identifier(iden) = arg {
            let value = param.value.clone();
            env.substitutions.insert(iden.clone(), value);
          } else {
            return errr(env, "Expected argument to be an identifier", context_pair);
          }
        }

        let src_str = &itempl.shards;
        let mut successful_parse = ShardsParser::parse(Rule::Program, &src_str).map_err(|e| {
          err(
            env,
            &format!("Failed to parse template: {}\n{}", e, src_str),
            context_pair,
          )
        })?;
        let root = successful_parse
          .next()
          .ok_or_else(|| err(env, "Expected a sequence", context_pair))?;

        process_program(root, env)
      })?;

    return Ok(FunctionValue::Program(prog));
  };

  Ok(FunctionValue::Function(Function {
    name: identifier,
    params,
    custom_state: CustomStateContainer::new(),
  }))
}

fn process_function(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<FunctionValue, ShardsError> {
  let mut inner = pair.clone().into_inner();
  let exp = inner
    .next()
    .ok_or_else(|| err(env, "Expected a Name or Const in Shard", &pair))?;

  match exp.as_rule() {
    Rule::UppIden => {
      // Definitely a Shard!
      let identifier = Identifier {
        name: exp.as_str().to_owned().into(),
        namespaces: Vec::new(),
        custom_state: CustomStateContainer::new(),
      };
      let next = inner.next();

      let params = match next {
        Some(pair) => {
          if pair.as_rule() == Rule::Params {
            Some(process_params(pair, env)?)
          } else {
            return errr(env, "Expected Params in Shard", &pair);
          }
        }
        None => None,
      };

      Ok(FunctionValue::Function(Function {
        name: identifier,
        params,
        custom_state: CustomStateContainer::new(),
      }))
    }
    Rule::VarName => {
      // Many other things...!
      let identifier = extract_identifier(env, exp)?;

      if identifier.namespaces.is_empty() {
        let name = identifier.name.as_str().to_owned();
        match name.as_str() {
          "inline-template" => {
            let params = inner.next();
            let mut inner = params
              .ok_or_else(|| err(env, "Expected parameters", &pair))?
              .into_inner();

            let param1 = process_param(
              inner
                .next()
                .ok_or_else(|| err(env, "Expected first parameter", &pair))?,
              env,
            )?;
            let param2 = process_param(
              inner
                .next()
                .ok_or_else(|| err(env, "Expected second parameter", &pair))?,
              env,
            )?;
            let remaining = inner.next();

            let func_name = if param1.name.is_none() {
              Some(&param1)
            } else {
              None
            };
            let func_name = func_name.ok_or_else(|| err(env, "Expected a function name", &pair))?;
            let func_name = match &func_name.value {
              Value::Identifier(s) => Ok(s),
              _ => errr(env, "Expected a string value for function name", &pair),
            }?;

            let args = if param2.name.is_none() {
              Some(&param2)
            } else {
              None
            };
            let args = args.ok_or_else(|| err(env, "Expected an argument list", &pair))?;
            let args = match &args.value {
              Value::Seq(s) => Ok(s),
              _ => errr(env, "Expected a sequence value for Args", &pair),
            }?;

            let shards =
              remaining.ok_or_else(|| err(env, "Expected a shards sequence (Shards:)", &pair))?;
            let contents = shards
              .into_inner()
              .next()
              .ok_or_else(|| err(env, "Expected a shards sequence (Shards:)", &pair))?; // (Param)
            let contents = contents
              .into_inner()
              .next()
              .ok_or_else(|| err(env, "Expected a shards sequence (Shards:)", &pair))?; // (Sequence)
            let contents = contents
              .into_inner()
              .next()
              .ok_or_else(|| err(env, "Expected a shards sequence (Shards:)", &pair))?; // (Shards)
            let contents = contents.as_str().to_owned();

            env.inline_templates.insert(
              func_name.clone(),
              InlineTemplate {
                args: args.clone(),
                shards: contents,
              },
            );
            Ok(FunctionValue::Const(Value::None(())))
          }
          "include" => {
            let params = extract_params_from_pairs(&mut inner, env, &pair)?;
            let params = params.ok_or_else(|| err(env, "Expected 2-3 parameters", &pair))?;
            let n_params = params.len();

            let file_name = if n_params > 0 && params[0].name.is_none() {
              Some(&params[0])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("File"))
            };
            let file_name =
              file_name.ok_or_else(|| err(env, "Expected a file name (File:)", &pair))?;
            let file_name = match &file_name.value {
              Value::String(s) => Ok(s),
              _ => errr(env, "Expected a string value for File", &pair),
            }?;

            let once = if n_params > 1 && params[0].name.is_none() && params[1].name.is_none() {
              Some(&params[1])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("Once"))
            };

            let once = once
              .map(|param| match &param.value {
                Value::Boolean(b) => Ok(*b),
                _ => errr(env, "Expected a boolean value for Once", &pair),
              })
              .unwrap_or(Ok(false))?;

            let fallback = if n_params > 2
              && params[0].name.is_none()
              && params[1].name.is_none()
              && params[2].name.is_none()
            {
              Some(&params[2])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("Fallback"))
            };

            let fallback = fallback
              .map(|param| match &param.value {
                Value::String(s) => Ok(s.as_str()),
                _ => errr(env, "Expected a string value for Fallback", &pair),
              })
              .transpose()?;

            let file_path = env
              .resolve_file(file_name)
              .or_else(|_| {
                fallback.map(|fb| env.resolve_file(fb)).unwrap_or(Err(
                  format!("File {} not found and no fallback provided", file_name).to_string(),
                ))
              })
              .map_err(|x| err(env, &x, &pair))?;

            let file_path_str = file_path
              .to_str()
              .ok_or_else(|| err(env, "Failed to convert file path to string", &pair))?
              .to_owned();

            let rc_path = file_path_str.into();

            if once && check_included(&rc_path, env) {
              return Ok(FunctionValue::Const(Value::None(())));
            }

            shlog_trace!("Including file {:?}", file_path);
            {
              // Insert this into the root map so it gets tracked globally
              let root_env = get_root_env(env);
              root_env.dependencies.borrow_mut().push(rc_path.to_string());
              root_env.included.borrow_mut().insert(rc_path);
            }

            // read string from file
            let mut code = std::fs::read_to_string(&file_path).map_err(|e| {
              err(
                env,
                &format!("Failed to read file {:?}: {}", file_path, e),
                &pair,
              )
            })?;
            // add new line at the end of the file to be able to parse it correctly
            code.push('\n');

            let parent = file_path.parent().unwrap_or(Path::new("."));
            let successful_parse = ShardsParser::parse(Rule::Program, &code).map_err(|e| {
              err(
                env,
                &format!("Failed to parse file {:?}: {}", file_path, e),
                &pair,
              )
            })?;
            let mut sub_env: ReadEnv = ReadEnv::new(
              file_path.to_str().unwrap(), // should be qed...
              parent
                .to_str()
                .ok_or_else(|| {
                  err(
                    env,
                    &format!("Failed to convert file path {:?} to string", parent),
                    &pair,
                  )
                })?
                .into(),
              Vec::new(),
            );
            sub_env.set_parent(env);
            let program = process_program(
              successful_parse.into_iter().next().unwrap(), // should be qed because of success parse
              &mut sub_env,
            )?;

            // Merge inline templates into parent
            env.inline_templates.extend(sub_env.inline_templates);

            Ok(FunctionValue::Program(program))
          }
          "env" => {
            // read from environment variable
            let params = extract_params_from_pairs(&mut inner, env, &pair)?;
            let params = params.ok_or_else(|| err(env, "Expected 1 parameter", &pair))?;
            let n_params = params.len();

            let name = if n_params > 0 && params[0].name.is_none() {
              Some(&params[0])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("Name"))
            };
            let name =
              name.ok_or_else(|| err(env, "Expected an environment variable name", &pair))?;
            let name = match &name.value {
              Value::String(s) => Ok(s),
              _ => errr(env, "Expected a string value", &pair),
            }?;

            let value = std::env::var(name.as_str()).unwrap_or("".to_string());

            Ok(FunctionValue::Const(Value::String(value.into())))
          }
          "read" => {
            let params = extract_params_from_pairs(&mut inner, env, &pair)?;
            let params = params.ok_or_else(|| err(env, "Expected 2 parameters", &pair))?;
            let n_params = params.len();

            let file_name = if n_params > 0 && params[0].name.is_none() {
              Some(&params[0])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("File"))
            };
            let file_name =
              file_name.ok_or_else(|| err(env, "Expected a file name (File:)", &pair))?;
            let file_name = match &file_name.value {
              Value::String(s) => Ok(s),
              _ => errr(env, "Expected a string value for File", &pair),
            }?;

            let as_bytes = if n_params > 1 && params[0].name.is_none() && params[1].name.is_none() {
              Some(&params[1])
            } else {
              params
                .iter()
                .find(|param| param.name.as_deref() == Some("Bytes"))
            };

            let as_bytes = as_bytes
              .map(|param| match &param.value {
                Value::Boolean(b) => Ok(*b),
                _ => errr(env, "Expected a boolean value for Bytes", &pair),
              })
              .unwrap_or(Ok(false))?;

            let file_path = env
              .resolve_file(file_name)
              .map_err(|x| err(env, &x, &pair))?;

            {
              // Insert this into the root map so it gets tracked globally
              let root_env = get_root_env(env);
              root_env
                .dependencies
                .borrow_mut()
                .push(file_path.to_string_lossy().to_string());
            }

            if as_bytes {
              // read bytes from file
              let bytes = std::fs::read(&file_path).map_err(|e| {
                err(
                  env,
                  &format!("Failed to read file {:?}: {}", file_path, e),
                  &pair,
                )
              })?;
              Ok(FunctionValue::Const(Value::Bytes(bytes.into())))
            } else {
              // read string from file
              let string = std::fs::read_to_string(&file_path).map_err(|e| {
                err(
                  env,
                  &format!("Failed to read file {:?}: {}", file_path, e),
                  &pair,
                )
              })?;
              Ok(FunctionValue::Const(Value::String(string.into())))
            }
          }
          "script-dir" => {
            let script_dir = RcStrWrapper::new(env.script_directory.to_string());
            Ok(FunctionValue::Const(Value::String(script_dir)))
          }
          "include-dirs" => {
            let include_dirs = env.include_directories.clone();
            Ok(FunctionValue::Const(Value::Seq(
              include_dirs
                .into_iter()
                .map(|x| Value::String(x.into()))
                .collect(),
            )))
          }
          _ => convert_to_function_value(identifier, &mut inner, env, &pair),
        }
      } else {
        convert_to_function_value(identifier, &mut inner, env, &pair)
      }
    }
    _ => errr(
      env,
      &format!("Unexpected rule {:?} in Function.", exp.as_rule()),
      &pair,
    ),
  }
}

fn process_take_table(
  pair: Pair<Rule>,
  env: &mut ReadEnv,
) -> Result<(Identifier, Vec<RcStrWrapper>), ShardsError> {
  // first is the identifier which has to be VarName
  // followed by N Iden which are the keys

  let mut inner = pair.clone().into_inner();
  let identity = inner
    .next()
    .ok_or_else(|| err(env, "Expected an identifier in TakeTable", &pair))?;

  let identifier = extract_identifier(env, identity)?;

  let mut keys = Vec::new();
  for inner_pair in inner {
    match inner_pair.as_rule() {
      Rule::Iden => keys.push(inner_pair.as_str().to_owned().into()),
      _ => return errr(env, "Expected an identifier in TakeTable", &inner_pair),
    }
  }

  // wrap the shards into an Expr Sequence
  Ok((identifier, keys))
}

fn process_take_seq(
  pair: Pair<Rule>,
  env: &mut ReadEnv,
) -> Result<(Identifier, Vec<u32>), ShardsError> {
  // first is the identifier which has to be VarName
  // followed by N Integer which are the indices

  let mut inner = pair.clone().into_inner();
  let identity = inner
    .next()
    .ok_or_else(|| err(env, "Expected an identifier in TakeSeq", &pair))?;

  let identifier = extract_identifier(env, identity)?;

  let mut indices = Vec::new();
  for inner_pair in inner {
    match inner_pair.as_rule() {
      Rule::Integer => {
        let value = inner_pair
          .as_str()
          .parse()
          .map_err(|_| err(env, "Failed to parse Integer", &inner_pair))?;
        indices.push(value);
      }
      _ => return errr(env, "Expected an integer in TakeSeq", &inner_pair),
    }
  }

  Ok((identifier, indices))
}

fn process_pipeline(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Pipeline, ShardsError> {
  if pair.as_rule() != Rule::Pipeline {
    return errr(
      env,
      "Expected a Pipeline rule, but found a different rule.",
      &pair,
    );
  }

  let mut blocks = Vec::new();

  for inner_pair in pair.into_inner() {
    let line_info = env.make_line_info_from_pair(&inner_pair);
    let rule = inner_pair.as_rule();
    match rule {
      Rule::EvalExpr => blocks.push(Block {
        content: BlockContent::EvalExpr(process_sequence(
          inner_pair.clone().into_inner().next().ok_or_else(|| {
            err(
              env,
              "Expected an eval time expression, but found none.",
              &inner_pair,
            )
          })?,
          env,
        )?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      Rule::Expr => blocks.push(Block {
        content: BlockContent::Expr(process_sequence(
          inner_pair
            .clone()
            .into_inner()
            .next()
            .ok_or_else(|| err(env, "Expected an expression, but found none.", &inner_pair))?,
          env,
        )?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      Rule::Shard => {
        match process_function(inner_pair, env)? {
          FunctionValue::Const(value) => blocks.push(Block {
            content: BlockContent::Const(value),
            line_info: Some(line_info),
            custom_state: CustomStateContainer::new(),
          }),
          FunctionValue::Function(func) => blocks.push(Block {
            content: BlockContent::Shard(func),
            line_info: Some(line_info),
            custom_state: CustomStateContainer::new(),
          }),
          FunctionValue::Program(program) => blocks.push(Block {
            content: BlockContent::Program(program),
            line_info: Some(line_info),
            custom_state: CustomStateContainer::new(),
          }),
        }
      }
      Rule::Func => match process_function(inner_pair, env)? {
        FunctionValue::Const(value) => blocks.push(Block {
          content: BlockContent::Const(value),
          line_info: Some(line_info),
          custom_state: CustomStateContainer::new(),
        }),
        FunctionValue::Function(func) => blocks.push(Block {
          content: BlockContent::Func(func),
          line_info: Some(line_info),
          custom_state: CustomStateContainer::new(),
        }),
        FunctionValue::Program(program) => blocks.push(Block {
          content: BlockContent::Program(program),
          line_info: Some(line_info),
          custom_state: CustomStateContainer::new(),
        }),
      },
      Rule::TakeTable => blocks.push(Block {
        content: {
          let pair_result = process_take_table(inner_pair, env)?;
          BlockContent::TakeTable(pair_result.0, pair_result.1)
        },
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      Rule::TakeSeq => blocks.push(Block {
        content: {
          let pair_result = process_take_seq(inner_pair, env)?;
          BlockContent::TakeSeq(pair_result.0, pair_result.1)
        },
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      Rule::ConstValue => blocks.push(Block {
        // this is an indirection, process_value will handle the case of a ConstValue
        content: BlockContent::Const(process_value(inner_pair, env)?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      Rule::Enum => blocks.push(Block {
        content: BlockContent::Const(process_value(inner_pair, env)?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      Rule::Shards => blocks.push(Block {
        content: BlockContent::Shards(process_sequence(
          inner_pair
            .clone()
            .into_inner()
            .next()
            .ok_or_else(|| err(env, "Expected an expression, but found none.", &inner_pair))?,
          env,
        )?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      }),
      _ => {
        return errr(
          env,
          &format!("Unexpected rule ({:?}) in Pipeline.", rule),
          &inner_pair,
        )
      }
    }
  }
  Ok(Pipeline { blocks })
}

fn process_statement(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Statement, ShardsError> {
  match pair.as_rule() {
    Rule::Assignment => process_assignment(pair, env).map(Statement::Assignment),
    Rule::Pipeline => process_pipeline(pair, env).map(Statement::Pipeline),
    _ => errr(env, "Expected an Assignment or a Pipeline", &pair),
  }
}

pub(crate) fn process_sequence(
  pair: Pair<Rule>,
  env: &mut ReadEnv,
) -> Result<Sequence, ShardsError> {
  let statements = pair
    .into_inner()
    .map(|x| process_statement(x, env))
    .collect::<Result<Vec<_>, _>>()?;
  Ok(Sequence {
    statements,
    custom_state: CustomStateContainer::new(),
  })
}

pub fn process_program(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Program, ShardsError> {
  if pair.as_rule() != Rule::Program {
    return errr(
      env,
      "Expected a Program rule, but found a different rule.",
      &pair,
    );
  }
  let pair = pair.into_inner().next().unwrap(); // parsed qed
  Ok(Program {
    sequence: process_sequence(pair, env)?,
    metadata: Metadata {
      name: env.name.clone(),
      debug_info: RefCell::new(DebugInfo::default()),
    },
  })
}

fn process_value(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Value, ShardsError> {
  match pair.as_rule() {
    Rule::ConstValue => {
      // unwrap the inner rule
      let pair = pair.into_inner().next().unwrap(); // parsed qed
      process_value(pair, env)
    }
    Rule::None => Ok(Value::None(())),
    Rule::Boolean => {
      // check if string content is true or false
      let bool_str = pair.as_str();
      if bool_str == "true" {
        Ok(Value::Boolean(true))
      } else if bool_str == "false" {
        Ok(Value::Boolean(false))
      } else {
        errr(env, "Expected a boolean value", &pair)
      }
    }
    Rule::VarName => {
      let identifier = extract_identifier(env, pair)?;
      if let Some(value) = env.find_substitution(&identifier) {
        return Ok(value.clone());
      }
      Ok(Value::Identifier(identifier))
    }
    Rule::Enum => {
      let text = pair.as_str();
      let splits: Vec<_> = text.split("::").collect();
      if splits.len() != 2 {
        return errr(env, "Expected an enum value", &pair);
      }
      let enum_name = splits[0].to_owned();
      let variant_name = splits[1].to_owned();
      Ok(Value::Enum(enum_name.into(), variant_name.into()))
    }
    Rule::Number => process_number(
      pair
        .clone()
        .into_inner()
        .next()
        .ok_or_else(|| err(env, "Expected a Number value", &pair))?,
      env,
    )
    .map(Value::Number),
    Rule::String => {
      let inner = pair.clone().into_inner().next().unwrap(); // parsed qed
      match inner.as_rule() {
        Rule::SimpleString => Ok(Value::String({
          let full_str = inner.as_str();
          // remove quotes AND
          // with this case we need to transform escaped characters
          // so we need to iterate over the string
          let mut chars: std::str::Chars = full_str[1..full_str.len() - 1].chars();
          let mut new_str = String::new();
          while let Some(c) = chars.next() {
            if c == '\\' {
              // we need to check the next character
              let c = chars
                .next()
                .ok_or_else(|| err(env, "Unexpected end of string", &pair))?;
              match c {
                'n' => new_str.push('\n'),
                'r' => new_str.push('\r'),
                't' => new_str.push('\t'),
                '\\' => new_str.push('\\'),
                '"' => new_str.push('"'),
                '\'' => new_str.push('\''),
                '0' => new_str.push('\0'),
                'b' => new_str.push('\u{0008}'), // Backspace
                'f' => new_str.push('\u{000C}'), // Form feed
                'v' => new_str.push('\u{000B}'), // Vertical tab
                _ => return errr(env, &format!("Unexpected escaped character {:?}", c), &pair),
              }
            } else {
              new_str.push(c);
            }
          }
          new_str.into()
        })),
        Rule::ComplexString => Ok(Value::String({
          let full_str = inner.as_str().to_owned();
          // remove triple quotes
          full_str[3..full_str.len() - 3].to_owned().into()
        })),
        _ => unreachable!(),
      }
    }
    Rule::Seq => {
      let values = pair
        .into_inner()
        .map(|pipe_value| process_pipe_value(pipe_value, env))
        .collect::<Result<Vec<_>, _>>()?;
      Ok(Value::Seq(values))
    }
    Rule::Table => {
      let pairs = pair
        .into_inner()
        .map(|pair| {
          assert_eq!(pair.as_rule(), Rule::TableEntry);

          let mut inner = pair.clone().into_inner();

          let key = inner.next().unwrap(); // should not fail
          assert_eq!(key.as_rule(), Rule::TableKey);
          let pos = key.as_span().start_pos();
          let key = key
            .clone()
            .into_inner()
            .next()
            .ok_or_else(|| err(env, "Expected a Table key", &key))?;
          let key = match key.as_rule() {
            Rule::None => Value::None(()),
            Rule::Iden => Value::String(key.as_str().to_owned().into()),
            Rule::VarName => Value::Identifier(extract_identifier(env, key)?),
            Rule::ConstValue => process_value(
              key.into_inner().next().unwrap(), // parsed qed
              env,
            )?,
            _ => {
              eprintln!("Unexpected rule in TableKey: {:?}", key.as_rule());
              unreachable!()
            }
          };

          let pipe_value = inner
            .next()
            .ok_or_else(|| err(env, "Expected a value in TableEntry", &pair))?;
          let value = process_pipe_value(pipe_value, env)?;
          Ok((key, value))
        })
        .collect::<Result<Vec<_>, _>>()?;
      Ok(Value::Table(pairs))
    }
    Rule::Shards => process_sequence(
      pair
        .clone()
        .into_inner()
        .next()
        .ok_or_else(|| err(env, "Expected a Sequence in Value", &pair))?,
      env,
    )
    .map(Value::Shards),
    Rule::Shard => match process_function(pair.clone(), env)? {
      FunctionValue::Function(func) => Ok(Value::Shard(func)),
      _ => errr(env, "Invalid Shard in value", &pair),
    },
    Rule::EvalExpr => process_sequence(
      pair
        .clone()
        .into_inner()
        .next()
        .ok_or_else(|| err(env, "Expected a Sequence in Value", &pair))?,
      env,
    )
    .map(Value::EvalExpr),
    Rule::Expr => process_sequence(
      pair
        .clone()
        .into_inner()
        .next()
        .ok_or_else(|| err(env, "Expected a Sequence in Value", &pair))?,
      env,
    )
    .map(Value::Expr),
    Rule::TakeTable => {
      let pair = process_take_table(pair, env)?;
      Ok(Value::TakeTable(pair.0, pair.1))
    }
    Rule::TakeSeq => {
      let pair = process_take_seq(pair, env)?;
      Ok(Value::TakeSeq(pair.0, pair.1))
    }
    Rule::Func => match process_function(pair.clone(), env)? {
      FunctionValue::Const(val) => return Ok(val),
      FunctionValue::Function(func) => Ok(Value::Func(func)),
      _ => errr(env, "Function cannot be used as value", &pair),
    },
    _ => errr(
      env,
      &format!("Unexpected rule ({:?}) in Value", pair.as_rule()),
      &pair,
    ),
  }
}

fn process_number(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Number, ShardsError> {
  match pair.as_rule() {
    Rule::Integer => Ok(Number::Integer(
      pair
        .as_str()
        .parse()
        .map_err(|_| err(env, "Failed to parse Integer", &pair))?,
    )),
    Rule::Float => Ok(Number::Float(
      pair
        .as_str()
        .parse()
        .map_err(|_| err(env, "Failed to parse Float", &pair))?,
    )),
    Rule::Hexadecimal => Ok(Number::Hexadecimal(pair.as_str().to_owned().into())),
    _ => errr(env, "Unexpected rule in Number", &pair),
  }
}

/// Process a PipeValue rule which contains one or more blocks separated by pipes.
/// If there's a single block, converts it directly to a Value.
/// If there are multiple blocks, wraps them in a Pipeline → Sequence → Value::Expr.
fn process_pipe_value(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Value, ShardsError> {
  if pair.as_rule() != Rule::PipeValue {
    return errr(env, "Expected a PipeValue rule", &pair);
  }

  let mut blocks = Vec::new();
  for inner_pair in pair.clone().into_inner() {
    let line_info = env.make_line_info_from_pair(&inner_pair);
    let rule = inner_pair.as_rule();
    let block = match rule {
      Rule::EvalExpr => Block {
        content: BlockContent::EvalExpr(process_sequence(
          inner_pair.clone().into_inner().next().ok_or_else(|| {
            err(env, "Expected an eval time expression in PipeValue", &inner_pair)
          })?,
          env,
        )?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      Rule::Expr => Block {
        content: BlockContent::Expr(process_sequence(
          inner_pair.clone().into_inner().next().ok_or_else(|| {
            err(env, "Expected an expression in PipeValue", &inner_pair)
          })?,
          env,
        )?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      Rule::Shard => {
        match process_function(inner_pair.clone(), env)? {
          FunctionValue::Const(value) => Block {
            content: BlockContent::Const(value),
            line_info: Some(line_info),
            custom_state: CustomStateContainer::new(),
          },
          FunctionValue::Function(func) => Block {
            content: BlockContent::Shard(func),
            line_info: Some(line_info),
            custom_state: CustomStateContainer::new(),
          },
          FunctionValue::Program(program) => Block {
            content: BlockContent::Program(program),
            line_info: Some(line_info),
            custom_state: CustomStateContainer::new(),
          },
        }
      }
      Rule::Func => match process_function(inner_pair.clone(), env)? {
        FunctionValue::Const(value) => Block {
          content: BlockContent::Const(value),
          line_info: Some(line_info),
          custom_state: CustomStateContainer::new(),
        },
        FunctionValue::Function(func) => Block {
          content: BlockContent::Func(func),
          line_info: Some(line_info),
          custom_state: CustomStateContainer::new(),
        },
        FunctionValue::Program(program) => Block {
          content: BlockContent::Program(program),
          line_info: Some(line_info),
          custom_state: CustomStateContainer::new(),
        },
      },
      Rule::TakeTable => Block {
        content: {
          let pair_result = process_take_table(inner_pair, env)?;
          BlockContent::TakeTable(pair_result.0, pair_result.1)
        },
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      Rule::TakeSeq => Block {
        content: {
          let pair_result = process_take_seq(inner_pair, env)?;
          BlockContent::TakeSeq(pair_result.0, pair_result.1)
        },
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      Rule::ConstValue => Block {
        content: BlockContent::Const(process_value(inner_pair, env)?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      Rule::Enum => Block {
        content: BlockContent::Const(process_value(inner_pair, env)?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      Rule::Shards => Block {
        content: BlockContent::Shards(process_sequence(
          inner_pair.clone().into_inner().next().ok_or_else(|| {
            err(env, "Expected a sequence in PipeValue", &inner_pair)
          })?,
          env,
        )?),
        line_info: Some(line_info),
        custom_state: CustomStateContainer::new(),
      },
      _ => {
        return errr(
          env,
          &format!("Unexpected rule ({:?}) in PipeValue", rule),
          &inner_pair,
        )
      }
    };
    blocks.push(block);
  }

  if blocks.len() == 1 {
    // Single block - convert directly to Value
    let block = blocks.remove(0);
    match block.content {
      BlockContent::Const(v) => Ok(v),
      BlockContent::Shard(f) => Ok(Value::Shard(f)),
      BlockContent::Func(f) => Ok(Value::Func(f)),
      BlockContent::Expr(s) => Ok(Value::Expr(s)),
      BlockContent::EvalExpr(s) => Ok(Value::EvalExpr(s)),
      BlockContent::Shards(s) => Ok(Value::Shards(s)),
      BlockContent::TakeTable(id, keys) => Ok(Value::TakeTable(id, keys)),
      BlockContent::TakeSeq(id, indices) => Ok(Value::TakeSeq(id, indices)),
      BlockContent::Program(prog) => {
        // Flatten program into its sequence as Expr
        Ok(Value::Expr(prog.sequence))
      }
      BlockContent::Empty => Ok(Value::None(())),
    }
  } else {
    // Multiple blocks - wrap in Pipeline → Sequence → Value::Expr
    let pipeline = Pipeline { blocks };
    let sequence = Sequence {
      statements: vec![Statement::Pipeline(pipeline)],
      custom_state: CustomStateContainer::new(),
    };
    Ok(Value::Expr(sequence))
  }
}

fn process_param(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Param, ShardsError> {
  if pair.as_rule() != Rule::Param {
    return errr(env, "Expected a Param rule", &pair);
  }

  let mut inner = pair.clone().into_inner();
  let first = inner
    .next()
    .ok_or_else(|| err(env, "Expected a ParamName or PipeValue in Param", &pair))?;
  let (param_name, param_value) = if first.as_rule() == Rule::ParamName {
    let name = first.as_str().to_owned();
    let name = name[0..name.len() - 1].to_owned().into();
    let pipe_value = inner
      .next()
      .ok_or_else(|| err(env, "Expected a PipeValue in Param", &pair))?;
    let value = process_pipe_value(pipe_value, env)?;
    (Some(name), value)
  } else {
    // first is the PipeValue itself
    (None, process_pipe_value(first, env)?)
  };

  Ok(Param {
    name: param_name,
    value: param_value,
    custom_state: CustomStateContainer::new(),
    is_default: None,
  })
}

fn process_params(pair: Pair<Rule>, env: &mut ReadEnv) -> Result<Vec<Param>, ShardsError> {
  pair.into_inner().map(|x| process_param(x, env)).collect()
}

pub fn parse(code: &str) -> Result<pest::iterators::Pairs<'_, Rule>, ShardsError> {
  profiling::scope!("parse");

  ShardsParser::parse(Rule::Program, code).map_err(|e| {
    (
      format!("Failed to parse file: {}", e),
      LineInfo {
        line: 0,
        column: 0,
        file: 0,
      },
    )
      .into()
  })
}

pub fn read_with_env(code: &str, env: &mut ReadEnv) -> Result<Program, ShardsError> {
  profiling::scope!("read_with_env");

  let successful_parse: pest::iterators::Pairs<'_, Rule> = {
    ShardsParser::parse(Rule::Program, code).map_err(|e| {
      (
        format!("Failed to parse file {:?}: {}", env.script_directory, e),
        LineInfo {
          line: 0,
          column: 0,
          file: env.resolve_file_id(),
        },
      )
        .into()
    })?
  };
  process_program(
    successful_parse.into_iter().next().unwrap(), // parsed qed
    env,
  )
}

pub fn read(
  code: &str,
  name: &str,
  path: String,
  include_dirs: Vec<String>,
) -> Result<Program, ShardsError> {
  let mut env = ReadEnv::new(name, path, include_dirs);
  read_with_env(&code, &mut env)
}

use lazy_static::lazy_static;

lazy_static! {
  pub static ref AST_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"ASTa")); // last letter used as version
  pub static ref AST_TYPE_VEC: Vec<Type> = vec![*AST_TYPE];
  pub static ref AST_VAR_TYPE: Type = Type::context_variable(&AST_TYPE_VEC);
  pub static ref READ_ADVANCED_OUTPUT_KEYS_AST: Vec<Var> = vec![
    shards::shstr!("ast").into(),
    shards::shstr!("dependencies").into(),
  ];
  pub static ref READ_ADVANCED_OUTPUT_VALUES_AST: Vec<Type> = vec![*AST_TYPE, common_type::strings];
  pub static ref READ_ADVANCED_OUTPUT_TYPE_AST: Type = Type::table(&READ_ADVANCED_OUTPUT_KEYS_AST, &READ_ADVANCED_OUTPUT_VALUES_AST);
  pub static ref READ_ADVANCED_OUTPUT_KEYS_BYTES: Vec<Var> = vec![
    shards::shstr!("bytes").into(),
    shards::shstr!("dependencies").into(),
  ];

  pub static ref READ_ADVANCED_OUTPUT_VALUES_BYTES: Vec<Type> = vec![common_type::bytes, common_type::strings];
  pub static ref READ_ADVANCED_OUTPUT_TYPE_BYTES: Type = Type::table(&READ_ADVANCED_OUTPUT_KEYS_BYTES, &READ_ADVANCED_OUTPUT_VALUES_BYTES);
  pub static ref READ_ADVANCED_OUTPUT_TYPES: Vec<Type> = vec![*READ_ADVANCED_OUTPUT_TYPE_AST, *READ_ADVANCED_OUTPUT_TYPE_BYTES];
  pub static ref READ_OUTPUT_TYPES: Vec<Type> = vec![common_type::string, common_type::bytes, *AST_TYPE, *READ_ADVANCED_OUTPUT_TYPE_AST, *READ_ADVANCED_OUTPUT_TYPE_BYTES];
  pub static ref AST_TYPES: Vec<Type> = vec![common_type::string, common_type::bytes, *AST_TYPE];
}

#[derive(shards::shards_enum)]
#[enum_info(
  b"ASTt",
  "AstType",
  "Variants of AST representation of a Shards program."
)]
pub enum AstType {
  #[enum_value("Binary AST as Bytes type")]
  Bytes = 0x0,
  #[enum_value("JSON String type AST")]
  Json = 0x1,
  #[enum_value("Live Object AST to be used within a live environment")]
  Object = 0x2,
  #[enum_value("Binary AST as Bytes type + additional output values.")]
  AdvancedBytes = 0x3,
  #[enum_value("Live Object AST to be used within a live environment + additional output values.")]
  AdvancedObject = 0x4,
}

lazy_static! {
  pub static ref STRINGS_VAR: Type = Type::context_variable(&STRING_TYPES);
}

#[derive(shard)]
#[shard_info(
  "Shards.Read",
  "Reads the textual representation of a Shards program and outputs the binary or json AST representation.",
)]
pub struct ReadShard {
  output: ClonedVar,
  #[shard_param(
    "OutputType",
    "Determines the type of AST to be outputted.",
    ASTTYPE_TYPES
  )]
  output_type: ClonedVar,
  #[shard_param(
    "BasePath",
    "The base path used when interpreting file references.",
    STRING_VAR_OR_NONE_SLICE
  )]
  base_path: ParamVar,
  #[shard_param(
    "Include",
    "The list of include paths.",
    [common_type::strings, *STRINGS_VAR]
  )]
  include: ParamVar,
  #[shard_param("Filename", "The filename of the script.", STRING_VAR_OR_NONE_SLICE)]
  filename: ParamVar,
  #[shard_required]
  required_variables: ExposedTypes,
}

impl Default for ReadShard {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      output_type: ClonedVar::from(AstType::Bytes),
      base_path: ParamVar::new(Var::ephemeral_string(".")),
      include: ParamVar::new(SeqVar::leaking_new().0),
      filename: ParamVar::default(),
      required_variables: ExposedTypes::default(),
    }
  }
}

ref_counted_object_type_impl!(Program);

#[shard_impl]
impl Shard for ReadShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &READ_OUTPUT_TYPES
  }

  fn warmup(&mut self, _context: &Context) -> Result<(), &str> {
    self.warmup_helper(_context)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(_data)?;

    match self.output_type.0.as_ref().try_into() {
      Ok(AstType::Bytes) => Ok(common_type::bytes),
      Ok(AstType::Json) => Ok(common_type::string),
      Ok(AstType::Object) => Ok(*AST_TYPE),
      Ok(AstType::AdvancedBytes) => Ok(*READ_ADVANCED_OUTPUT_TYPE_BYTES),
      Ok(AstType::AdvancedObject) => Ok(*READ_ADVANCED_OUTPUT_TYPE_AST),
      Err(_) => {
        shlog_error!("Invalid output type for ReadShard");
        Err("Invalid output type for ReadShard")
      }
    }
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let code: &str = input.try_into()?;

    let output_type = self.output_type.0.as_ref().try_into() as Result<AstType, _>;

    let parsed = ShardsParser::parse(Rule::Program, code).map_err(|e| {
      shlog_error!("Failed to parse shards code: {}", e);
      "Failed to parse Shards code"
    })?;

    let filename_var = self.filename.get();
    let filename = if filename_var.is_none() {
      ""
    } else {
      (filename_var).try_into()?
    };

    let fn_var = self.filename.get();
    let name = if fn_var.is_none() {
      if (filename.is_empty()) {
        ""
      } else {
        Path::new(filename)
          .file_name()
          .and_then(|f| f.to_str())
          .unwrap_or(filename)
      }
    } else {
      (fn_var).try_into()?
    };

    let bp_var = self.base_path.get();
    let mut tmpBasePath: Option<String> = None;
    let base_path = if bp_var.is_none() {
      if name.is_empty() {
        "."
      } else {
        // Parent path of name
        tmpBasePath = Some(
          Path::new(&name)
            .parent()
            .unwrap()
            .to_str()
            .unwrap()
            .to_string(),
        );
        &tmpBasePath.unwrap()
      }
    } else {
      bp_var.try_into()?
    };

    let mut includes: Vec<String> = Vec::new();
    for inc in self.include.get().as_seq()? {
      includes.push((&inc).try_into()?);
    }

    let mut env = ReadEnv::new(name, base_path.to_string(), includes);
    let prog = process_program(
      parsed.into_iter().next().unwrap(), // parsed qed
      &mut env,
    )
    .map_err(|e| {
      shlog_error!("Failed to process shards code: {:?}", e);
      "Failed to tokenize Shards code"
    })?;

    match output_type {
      Ok(AstType::Bytes) => {
        // Serialize using flexbuffers
        let encoded_bin: Vec<u8> = flexbuffers::to_vec(&prog).map_err(|e| {
          shlog_error!("Failed to serialize shards code: {}", e);
          "Failed to serialize Shards code"
        })?;

        self.output = encoded_bin.as_slice().into();
      }
      Ok(AstType::Json) => {
        // Serialize using json
        let encoded_json = serde_json::to_string(&prog).map_err(|e| {
          shlog_error!("Failed to serialize shards code: {}", e);
          "Failed to serialize Shards code"
        })?;

        let s = Var::ephemeral_string(encoded_json.as_str());
        self.output = s.into();
      }
      Ok(AstType::Object) => {
        self.output = Var::new_ref_counted(prog, &AST_TYPE).into();
      }
      Ok(AstType::AdvancedBytes) => {
        let mut output_table = AutoTableVar::new();

        // Serialize AST using flexbuffers
        let encoded_bin: Vec<u8> = flexbuffers::to_vec(&prog).map_err(|e| {
          shlog_error!("Failed to serialize shards code: {}", e);
          "Failed to serialize Shards code"
        })?;
        let ast_output = encoded_bin.as_slice().into();

        // Get dependencies and convert to Var
        let deps = get_dependencies(&env);
        let mut deps_var = AutoSeqVar::new();
        for dep in deps.iter() {
          deps_var.0.emplace(ClonedVar::new_string(dep));
        }

        // Set table values
        output_table.0.insert_fast_static("bytes", &ast_output);
        output_table
          .0
          .insert_fast_static("dependencies", &deps_var.0 .0);

        self.output = output_table.to_cloned();
      }
      Ok(AstType::AdvancedObject) => {
        let mut output_table = AutoTableVar::new();

        // Serialize AST using flexbuffers
        let encoded_bin: Vec<u8> = flexbuffers::to_vec(&prog).map_err(|e| {
          shlog_error!("Failed to serialize shards code: {}", e);
          "Failed to serialize Shards code"
        })?;

        let ast_output = encoded_bin.as_slice().into();

        // Get dependencies and convert to Var
        let deps = get_dependencies(&env);
        let mut deps_var = AutoSeqVar::new();
        for dep in deps.iter() {
          deps_var.0.emplace(ClonedVar::new_string(dep));
        }

        // Set table values
        output_table.0.insert_fast_static("ast", &ast_output);
        output_table
          .0
          .insert_fast_static("dependencies", &deps_var.0 .0);

        self.output = output_table.to_cloned();
      }
      Err(_) => {
        shlog_error!("Invalid output type for ReadShard");
        return Err("Invalid output type for ReadShard");
      }
    }

    Ok(Some(self.output.0))
  }
}

// Shards.Errors
// A shard to fetch all errors from a live AstType::Object input

#[derive(shards::shard)]
#[shard_info("Shards.Errors", "Fetches all errors from a live AST Object input")]
pub struct ShardsErrorsShard {
  #[shard_required]
  required: ExposedTypes,

  output: AutoSeqVar,
}

impl Default for ShardsErrorsShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ShardsErrorsShard {
  fn input_types(&mut self) -> &Types {
    &AST_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &STRINGS_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let object = unsafe { &*Var::from_ref_counted_object::<Program>(input, &AST_TYPE)? };
    self.output.0.clear();
    self.process_sequence(&object.sequence);
    Ok(Some(self.output.0 .0))
  }
}

impl ShardsErrorsShard {
  fn process_sequence(&mut self, seq: &Sequence) {
    for child in seq.statements.iter() {
      match child {
        Statement::Pipeline(p) => {
          self.process_pipeline(p);
        }
        _ => (),
      }
    }
    seq.custom_state.with::<ShardsError, _, _>(|e| {
      let mut table = AutoTableVar::new();
      table
        .0
        .insert_fast_static("message", &Var::ephemeral_string(e.message.as_str()));
      table.0.insert_fast_static("line", &e.loc.line.into());
      table.0.insert_fast_static("column", &e.loc.column.into());
      self.output.0.emplace(table.to_cloned());
    });
  }

  fn process_pipeline(&mut self, pipeline: &Pipeline) {
    for block in pipeline.blocks.iter() {
      match &block.content {
        BlockContent::Shard(f) | BlockContent::Func(f) => {
          if let Some(params) = &f.params {
            for param in params {
              self.process_value(&param.value);
            }
          }
          f.custom_state.with::<ShardsError, _, _>(|e| {
            let mut table = AutoTableVar::new();
            table
              .0
              .insert_fast_static("message", &Var::ephemeral_string(e.message.as_str()));
            table.0.insert_fast_static("line", &e.loc.line.into());
            table.0.insert_fast_static("column", &e.loc.column.into());
            self.output.0.emplace(table.to_cloned());
          });
        }
        BlockContent::Shards(s) | BlockContent::EvalExpr(s) | BlockContent::Expr(s) => {
          self.process_sequence(s)
        }
        BlockContent::Const(v) => self.process_value(v),
        _ => (),
      }
    }
  }

  fn process_value(&mut self, value: &Value) {
    match value {
      Value::None(_) => {}
      Value::Identifier(identifier) => {
        identifier
          .custom_state
          .with::<ShardsError, _, _>(|e: &ShardsError| {
            let mut table = AutoTableVar::new();
            table
              .0
              .insert_fast_static("message", &Var::ephemeral_string(e.message.as_str()));
            table.0.insert_fast_static("line", &e.loc.line.into());
            table.0.insert_fast_static("column", &e.loc.column.into());
            self.output.0.emplace(table.to_cloned());
          });
      }
      Value::Boolean(_) => {}
      Value::Enum(_, _) => {}
      Value::Number(_) => {}
      Value::String(_) => {}
      Value::Bytes(_) => {}
      Value::Int2(_) | Value::Int3(_) | Value::Int4(_) | Value::Int8(_) | Value::Int16(_) => {}
      Value::Float2(_) | Value::Float3(_) | Value::Float4(_) => {}
      Value::Seq(seq) => {
        for item in seq {
          self.process_value(item);
        }
      }
      Value::Table(table) => {
        for (key, value) in table {
          self.process_value(key);
          self.process_value(value);
        }
      }
      Value::Shard(function) | Value::Func(function) => {
        function.custom_state.with::<ShardsError, _, _>(|e| {
          let mut table = AutoTableVar::new();
          table
            .0
            .insert_fast_static("message", &Var::ephemeral_string(e.message.as_str()));
          table.0.insert_fast_static("line", &e.loc.line.into());
          table.0.insert_fast_static("column", &e.loc.column.into());
          self.output.0.emplace(table.to_cloned());
        });
      }
      Value::Shards(sequence) | Value::EvalExpr(sequence) | Value::Expr(sequence) => {
        self.process_sequence(sequence);
      }
      Value::TakeTable(identifier, _) | Value::TakeSeq(identifier, _) => {
        identifier.custom_state.with::<ShardsError, _, _>(|e| {
          let mut table = AutoTableVar::new();
          table
            .0
            .insert_fast_static("message", &Var::ephemeral_string(e.message.as_str()));
          table.0.insert_fast_static("line", &e.loc.line.into());
          table.0.insert_fast_static("column", &e.loc.column.into());
          self.output.0.emplace(table.to_cloned());
        });
      }
    }
  }
}

// Test stubs for external C functions that are normally provided by the C++ runtime
#[cfg(test)]
mod test_stubs {
  use std::os::raw::{c_char, c_int};

  #[no_mangle]
  pub extern "C" fn shards_log(
    _level: c_int,
    _msg: shards::shardsc::SHStringWithLen,
    _file: *const c_char,
    _function: *const c_char,
    _line: c_int,
  ) {
    // No-op stub for tests
  }

  #[no_mangle]
  pub extern "C" fn shlang_fr_static() -> *const u8 {
    std::ptr::null()
  }

  #[no_mangle]
  pub extern "C" fn shlang_fr_get_file_id(
    _handle: *const u8,
    _path: shards::shardsc::SHStringWithLen,
  ) -> u32 {
    0 // Return 0 to indicate no file ID
  }
}

#[test]
fn test_parsing1() {
  // use std::num::NonZeroUsize;
  // pest::set_call_limit(NonZeroUsize::new(25000));
  // let code = include_str!("nested.shs");
  let code = include_str!("sample1.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();
  let seq = seq.sequence;

  // Serialize using flexbuffers
  let encoded_bin: Vec<u8> = flexbuffers::to_vec(&seq).unwrap();

  // Deserialize using flexbuffers
  let decoded_bin: Sequence = flexbuffers::from_slice(&encoded_bin).unwrap();

  // Serialize using json
  let encoded_json = serde_json::to_string(&seq).unwrap();
  println!("Json Serialized = {}", encoded_json);

  let encoded_json2 = serde_json::to_string(&decoded_bin).unwrap();
  assert_eq!(encoded_json, encoded_json2);

  // Deserialize using json
  let decoded_json: Sequence = serde_json::from_str(&encoded_json).unwrap();

  let encoded_bin2: Vec<u8> = flexbuffers::to_vec(&decoded_json).unwrap();
  assert_eq!(encoded_bin, encoded_bin2);
}

#[test]
fn test_parsing2() {
  let code = include_str!("explained.shs");
  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let seq = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();
  let seq = seq.sequence;

  // Serialize using flexbuffers
  let encoded_bin: Vec<u8> = flexbuffers::to_vec(&seq).unwrap();

  // Deserialize using flexbuffers
  let decoded_bin: Sequence = flexbuffers::from_slice(&encoded_bin).unwrap();

  // Serialize using json
  let encoded_json = serde_json::to_string(&seq).unwrap();
  println!("Json Serialized = {}", encoded_json);

  let encoded_json2 = serde_json::to_string(&decoded_bin).unwrap();
  assert_eq!(encoded_json, encoded_json2);

  // Deserialize using json
  let decoded_json: Sequence = serde_json::from_str(&encoded_json).unwrap();

  let encoded_bin2: Vec<u8> = flexbuffers::to_vec(&decoded_json).unwrap();
  assert_eq!(encoded_bin, encoded_bin2);
}

#[test]
fn test_pipe_in_params() {
  // Test the new pipe-in-params syntax: Add(3 | Mul(4)) instead of Add((3 | Mul(4)))
  let code = r#"
    // Basic pipe in params - should work without extra parens
    2 | Add(3 | Mul(4))

    // Multiple params with pipes (comma is whitespace, params separated by natural boundaries)
    Func(1 | Add(2) 3 | Mul(4))

    // Named params with pipes
    Shard(X: 1 | Add(2) Y: 3 | Mul(4))

    // Nested pipes
    Outer(Inner(1 | Add(2)) | Process)

    // Single value params (should still work)
    Simple(42)
    Simple(x)
    Simple("string")

    // Mixed: single values and pipes as separate params
    // This should be 3 params: 1, (2 | Add(3)), 4
    Mixed(1 2 | Add(3) 4)
  "#;

  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env);

  // Should parse successfully without errors
  assert!(program.is_ok(), "Failed to parse pipe-in-params: {:?}", program.err());

  let seq = program.unwrap().sequence;

  // Verify we have statements (not checking exact structure, just that it parses)
  assert!(!seq.statements.is_empty(), "Expected statements in parsed program");

  // Serialize and deserialize to verify AST structure is valid
  let encoded_bin: Vec<u8> = flexbuffers::to_vec(&seq).unwrap();
  let decoded_bin: Sequence = flexbuffers::from_slice(&encoded_bin).unwrap();

  let encoded_json = serde_json::to_string(&seq).unwrap();
  let encoded_json2 = serde_json::to_string(&decoded_bin).unwrap();
  assert_eq!(encoded_json, encoded_json2);
}

#[test]
fn test_pipe_in_params_structure() {
  // Verify that "Mixed(1 2 | Add(3) 4)" produces 3 separate params
  let code = "Mixed(1 2 | Add(3) 4)";

  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();

  // Get the first statement which should be a Pipeline containing the Mixed shard
  let stmt = &program.sequence.statements[0];
  if let Statement::Pipeline(pipeline) = stmt {
    assert_eq!(pipeline.blocks.len(), 1, "Expected 1 block in pipeline");
    if let BlockContent::Shard(func) = &pipeline.blocks[0].content {
      assert_eq!(func.name.name.as_str(), "Mixed");
      let params = func.params.as_ref().expect("Expected params");
      assert_eq!(params.len(), 3, "Expected 3 params: 1, (2 | Add(3)), 4");

      // First param should be a simple number 1
      if let Value::Number(Number::Integer(n)) = &params[0].value {
        assert_eq!(*n, 1, "First param should be 1");
      } else {
        panic!("First param should be Number::Integer(1), got {:?}", params[0].value);
      }

      // Second param should be an Expr (the pipe creates a sub-expression)
      assert!(matches!(&params[1].value, Value::Expr(_)),
        "Second param should be Expr (pipeline), got {:?}", params[1].value);

      // Third param should be a simple number 4
      if let Value::Number(Number::Integer(n)) = &params[2].value {
        assert_eq!(*n, 4, "Third param should be 4");
      } else {
        panic!("Third param should be Number::Integer(4), got {:?}", params[2].value);
      }
    } else {
      panic!("Expected Shard block content");
    }
  } else {
    panic!("Expected Pipeline statement");
  }
}

#[test]
fn test_pipe_in_seq() {
  // Test pipes in sequence literals: [1 2 | Add(3) 4]
  let code = r#"
    // Basic pipe in sequence
    [1 2 | Add(3) 4]

    // Multiple pipes in sequence
    [x | Transform y | Process z]

    // Simple sequence (should still work)
    [1 2 3 4 5]

    // Mixed values and pipes
    ["hello" 1 | Add(2) @f3(1 2 3)]
  "#;

  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env);

  assert!(program.is_ok(), "Failed to parse pipe-in-seq: {:?}", program.err());
}

#[test]
fn test_pipe_in_seq_structure() {
  // Verify that "[1 2 | Add(3) 4]" produces 3 elements
  let code = "[1 2 | Add(3) 4]";

  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();

  let stmt = &program.sequence.statements[0];
  if let Statement::Pipeline(pipeline) = stmt {
    if let BlockContent::Const(Value::Seq(elements)) = &pipeline.blocks[0].content {
      assert_eq!(elements.len(), 3, "Expected 3 elements: 1, (2 | Add(3)), 4");

      // First element should be 1
      if let Value::Number(Number::Integer(n)) = &elements[0] {
        assert_eq!(*n, 1, "First element should be 1");
      } else {
        panic!("First element should be Number::Integer(1), got {:?}", elements[0]);
      }

      // Second element should be an Expr (the pipe)
      assert!(matches!(&elements[1], Value::Expr(_)),
        "Second element should be Expr (pipeline), got {:?}", elements[1]);

      // Third element should be 4
      if let Value::Number(Number::Integer(n)) = &elements[2] {
        assert_eq!(*n, 4, "Third element should be 4");
      } else {
        panic!("Third element should be Number::Integer(4), got {:?}", elements[2]);
      }
    } else {
      panic!("Expected Const(Seq) block content, got {:?}", pipeline.blocks[0].content);
    }
  } else {
    panic!("Expected Pipeline statement");
  }
}

#[test]
fn test_pipe_in_table() {
  // Test pipes in table literals: {a: 1 b: 1 | Add(3)}
  let code = r#"
    // Basic pipe in table value
    {a: 1 b: 1 | Add(3)}

    // Multiple entries with pipes
    {x: val | Transform y: other | Process z: 42}

    // Simple table (should still work)
    {name: "test" value: 123}

    // Mixed values
    {static: 1 dynamic: x | Compute}
  "#;

  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env);

  assert!(program.is_ok(), "Failed to parse pipe-in-table: {:?}", program.err());
}

#[test]
fn test_pipe_in_table_structure() {
  // Verify that "{a: 1 b: 2 | Add(3)}" has correct structure
  let code = "{a: 1 b: 2 | Add(3)}";

  let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let program = process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();

  let stmt = &program.sequence.statements[0];
  if let Statement::Pipeline(pipeline) = stmt {
    if let BlockContent::Const(Value::Table(pairs)) = &pipeline.blocks[0].content {
      assert_eq!(pairs.len(), 2, "Expected 2 table entries");

      // First entry: a: 1
      if let Value::String(key) = &pairs[0].0 {
        assert_eq!(key.as_str(), "a");
      } else {
        panic!("First key should be 'a'");
      }
      if let Value::Number(Number::Integer(n)) = &pairs[0].1 {
        assert_eq!(*n, 1, "First value should be 1");
      } else {
        panic!("First value should be Number::Integer(1), got {:?}", pairs[0].1);
      }

      // Second entry: b: (2 | Add(3))
      if let Value::String(key) = &pairs[1].0 {
        assert_eq!(key.as_str(), "b");
      } else {
        panic!("Second key should be 'b'");
      }
      assert!(matches!(&pairs[1].1, Value::Expr(_)),
        "Second value should be Expr (pipeline), got {:?}", pairs[1].1);
    } else {
      panic!("Expected Const(Table) block content, got {:?}", pipeline.blocks[0].content);
    }
  } else {
    panic!("Expected Pipeline statement");
  }
}

#[test]
fn test_pipe_advanced_cases() {
  // Test nested pipes - pipe within pipe param
  let nested = "Outer(Inner(1 | Add(2)) | Process)";
  let parsed = ShardsParser::parse(Rule::Program, nested).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let result = process_program(parsed.into_iter().next().unwrap(), &mut env);
  assert!(result.is_ok(), "Nested pipes should parse: {:?}", result.err());

  // Test eval expressions as pipe values
  let eval_expr = "Func(#(1 | Add(2)) | Process)";
  let parsed = ShardsParser::parse(Rule::Program, eval_expr).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let result = process_program(parsed.into_iter().next().unwrap(), &mut env);
  assert!(result.is_ok(), "Eval expr in pipe should parse: {:?}", result.err());

  // Test @func style calls with pipes
  let func_style = "@wire(test { x | @transform(1 | Add(2)) | Log })";
  let parsed = ShardsParser::parse(Rule::Program, func_style).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let result = process_program(parsed.into_iter().next().unwrap(), &mut env);
  assert!(result.is_ok(), "@func with pipes should parse: {:?}", result.err());

  // Test deeply nested pipes
  let deep = "A(B(C(1 | X) | Y) | Z)";
  let parsed = ShardsParser::parse(Rule::Program, deep).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let result = process_program(parsed.into_iter().next().unwrap(), &mut env);
  assert!(result.is_ok(), "Deep nested pipes should parse: {:?}", result.err());

  // Test comments between pipe elements
  let with_comments = r#"Func(
    1 // first param
    2 | Add(3) // piped param
    4 // last param
  )"#;
  let parsed = ShardsParser::parse(Rule::Program, with_comments).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let result = process_program(parsed.into_iter().next().unwrap(), &mut env);
  assert!(result.is_ok(), "Comments in pipes should parse: {:?}", result.err());

  // Test expression parens in pipe
  let expr_in_pipe = "Func((x | Y) | Z)";
  let parsed = ShardsParser::parse(Rule::Program, expr_in_pipe).unwrap();
  let mut env = ReadEnv::new_cwd("");
  let result = process_program(parsed.into_iter().next().unwrap(), &mut env);
  assert!(result.is_ok(), "Expr parens in pipe should parse: {:?}", result.err());
}

// Shards.Docs shard for getting documentation for shards and enums
#[derive(shards::shard)]
#[shard_info("Shards.Docs", "Outputs documentation for a shard or enum.")]
pub struct DocsShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,

  #[shard_param(
    "Type",
    "The type of documentation to get, either 'shard' or 'enum'.",
    STRING_VAR_OR_NONE_SLICE
  )]
  doc_type: ParamVar,
}

impl Default for DocsShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
      doc_type: ParamVar::new(Var::ephemeral_string("shard")),
    }
  }
}

#[shards::shard_impl]
impl Shard for DocsShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(common_type::string)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let name: &str = input.try_into()?;
    let doc_type: &str = self.doc_type.get().try_into()?;

    // Use a buffer to collect the output
    let mut buffer = Vec::new();

    // Call help_to_writer from cli.rs
    crate::cli::help_to_writer(&mut buffer, name, doc_type)
      .map_err(|_| "Failed to get documentation")?;

    // Convert the buffer to a string
    let docs =
      String::from_utf8(buffer).map_err(|_| "Failed to convert documentation to string")?;

    // Output the documentation string
    self.output = Var::ephemeral_string(&docs).into();

    Ok(Some(self.output.0))
  }
}
