use std::hash::{DefaultHasher, Hash, Hasher};

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{common_type, TableVar, ANY_TABLE_TYPES, STRING_TYPES};
use shards::types::{ClonedVar, Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};
use serde_json;

#[derive(shards::shard)]
#[shard_info("Jinja.Apply", "Apply a Jinja template to an input")]
struct JinjaShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Template", "The template to apply", [common_type::string, common_type::string_var])]
  template: ParamVar,
  previous_template_hash: Option<String>,

  output: ClonedVar,

  env: minijinja::Environment<'static>,
}

impl JinjaShard {
  fn initialize_environment(env: &mut minijinja::Environment<'static>) {
    // Add raise_exception function to allow users to raise custom errors from templates
    env.add_function("raise_exception", |msg: String| -> Result<(), minijinja::Error> {
      Err(minijinja::Error::new(minijinja::ErrorKind::InvalidOperation, msg))
    });
    
    // Add tojson function to serialize an object to JSON and mark it safe for HTML
    env.add_filter("tojson", |value: minijinja::Value| -> minijinja::Value {
      let json = serde_json::to_string(&value).unwrap_or_else(|_| "null".to_string());
      minijinja::Value::from_safe_string(json)
    });
  }
}

impl Default for JinjaShard {
  fn default() -> Self {
    let mut env = minijinja::Environment::new();
    
    JinjaShard::initialize_environment(&mut env);
    
    Self {
      required: ExposedTypes::new(),
      template: ParamVar::default(),
      previous_template_hash: None,
      output: ClonedVar::default(),
      env,
    }
  }
}

#[shards::shard_impl]
impl Shard for JinjaShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TABLE_TYPES
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
    self.env = minijinja::Environment::new();
    JinjaShard::initialize_environment(&mut self.env);
    self.previous_template_hash = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.template.is_none() {
      return Err("Template is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let table: TableVar = input.try_into()?;

    let template: &str = self.template.get().as_ref().try_into()?;
    if self.template.is_variable() {
      // we need to recompile the template, if the template hash has changed
      let mut hasher = DefaultHasher::new();
      template.hash(&mut hasher);
      let template_hash = hasher.finish().to_string();
      if self.previous_template_hash.is_none()
        || &template_hash != self.previous_template_hash.as_ref().unwrap()
      {
        self
          .env
          .add_template_owned(template_hash.clone(), template)
          .map_err(|e| {
            shlog_error!("Failed to add template: {}", e);
            "Failed to add template"
          })?;
        self.previous_template_hash = Some(template_hash);
      }
    } else {
      // this will never change so use previous template hash directly
      if self.previous_template_hash.is_none() {
        let mut hasher = DefaultHasher::new();
        template.hash(&mut hasher);
        let template_hash = hasher.finish().to_string();
        self
          .env
          .add_template_owned(template_hash.clone(), template)
          .map_err(|e| {
            shlog_error!("Failed to add template: {}", e);
            "Failed to add template"
          })?;
        self.previous_template_hash = Some(template_hash);
      }
    }

    let template = self
      .env
      .get_template(&self.previous_template_hash.as_ref().unwrap());
    if template.is_err() {
      return Err("Template not found");
    }
    let template = template.unwrap();
    let output = template.render(table).map_err(|e| {
      shlog_error!("Failed to render template: {}", e);
      "Failed to render template"
    })?;
    self.output = output.into();

    Ok(Some(self.output.0))
  }
}

pub(crate) fn register_shards() {
  register_shard::<JinjaShard>();
}
