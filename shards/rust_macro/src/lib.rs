extern crate proc_macro;
use std::{boxed, collections::HashSet};

use convert_case::Casing;
use itertools::Itertools;
use proc_macro::TokenStream;
use proc_macro2::Span;
use quote::quote;
use syn::{
  punctuated::Punctuated, token::Comma, Expr, Field, Ident, ImplItem, Lit, LitInt, LitStr, Meta,
};

// type Error = boxed::Box<dyn std::error::Error>;
enum Error {
  CompileError(proc_macro2::TokenStream),
  Generic(boxed::Box<dyn std::error::Error>),
}

impl From<&str> for Error {
  fn from(value: &str) -> Self {
    Error::Generic(value.into())
  }
}

impl From<String> for Error {
  fn from(value: String) -> Self {
    Error::Generic(value.into())
  }
}

impl From<syn::Error> for Error {
  fn from(value: syn::Error) -> Self {
    Error::CompileError(value.into_compile_error())
  }
}

impl Error {
  fn to_compile_error2(self) -> proc_macro2::TokenStream {
    match self {
      Error::CompileError(stream) => stream,
      Error::Generic(err) => syn::Error::new(Span::call_site(), err.to_string()).to_compile_error(),
    }
  }
  fn to_compile_error(self) -> proc_macro::TokenStream {
    self.to_compile_error2().into()
  }
  fn extended(self, e: Error) -> Self {
    let mut stream = self.to_compile_error2();
    stream.extend(e.to_compile_error2());
    Error::CompileError(stream)
  }
}

lazy_static::lazy_static! {
  static ref IMPLS_TO_CHECK: Vec<&'static str> = vec![
    "compose",
    "warmup",
    "mutate",
    "crossover",
    "get_state",
    "set_state",
    "reset_state",
  ];
  static ref IMPLS_TO_CHECK_SET : HashSet<&'static str> = HashSet::from_iter(IMPLS_TO_CHECK.iter().cloned());
}

struct ParamSingle {
  name: String,
  var_name: syn::Ident,
  desc: String,
  types: syn::Expr,
}

struct ParamSet {
  type_name: syn::Type,
  var_name: syn::Ident,
}

enum Param {
  Single(ParamSingle),
  Set(ParamSet),
}

fn get_field_name(fld: &Field) -> String {
  if let Some(id) = &fld.ident {
    id.to_string()
  } else {
    "".to_string()
  }
}

fn get_expr_str_lit(expr: &Expr) -> Result<String, Error> {
  if let syn::Expr::Lit(lit) = expr {
    if let syn::Lit::Str(str) = &lit.lit {
      Ok(str.value())
    } else {
      Err("Value must be a string literal".into())
    }
  } else {
    Err("Value must be a string literal".into())
  }
}

fn get_expr_bool_lit(expr: &Expr) -> Result<bool, Error> {
  if let syn::Expr::Lit(lit) = expr {
    if let syn::Lit::Bool(str) = &lit.lit {
      Ok(str.value())
    } else {
      Err("Value must be a bool literal".into())
    }
  } else {
    Err("Value must be a bool literal".into())
  }
}

struct EnumInfoAttr {
  id: Expr,
  name: Expr,
  desc: Expr,
}

fn read_enum_info_attr(attrs: &Vec<syn::Attribute>) -> Result<EnumInfoAttr, Error> {
  for attr in attrs {
    if attr.path().is_ident("enum_info") {
      let args = attr.parse_args_with(Punctuated::<syn::Expr, Comma>::parse_terminated)?;
      return if let Some((id, name, desc)) =
        args.into_pairs().map(|x| x.into_value()).collect_tuple()
      {
        Ok(EnumInfoAttr { id, name, desc })
      } else {
        Err("shards_enum attribute must have 3 arguments: (Id, Name, Description)".into())
      };
    }
  }
  Err("Missing shards_enum attribute".into())
}

fn read_enum_value_attr(attrs: &Vec<syn::Attribute>) -> Result<Option<LitStr>, Error> {
  for attr in attrs {
    if attr.path().is_ident("enum_value") {
      return Ok(Some(attr.parse_args()?));
    }
  }
  Ok(None)
}

fn generate_enum_wrapper(enum_: syn::ItemEnum) -> Result<TokenStream, Error> {
  let vis = enum_.vis;
  let enum_id = enum_.ident;
  let enum_name = enum_id.to_string();

  let mut value_ids = Vec::new();
  let mut value_str_ids = Vec::new();
  let mut value_desc_lits = Vec::new();
  let mut value_name_lits = Vec::new();

  let shards_enum_attr = read_enum_info_attr(&enum_.attrs)?;

  for var in &enum_.variants {
    let var_name = var.ident.to_string();
    let desc_lit = read_enum_value_attr(&var.attrs)?;

    value_ids.push(var.ident.clone());
    value_str_ids.push(Ident::new(
      &format!("{}_str", var_name),
      proc_macro2::Span::call_site(),
    ));
    value_name_lits.push(LitStr::new(&var_name, proc_macro2::Span::call_site()));

    if let Some(lit) = desc_lit {
      value_desc_lits.push(lit);
    } else {
      value_desc_lits.push(LitStr::new("", proc_macro2::Span::call_site()));
    }
  }

  let enum_info_id = Ident::new(
    &format!("{}EnumInfo", enum_name),
    proc_macro2::Span::call_site(),
  );

  let enum_name_upper = enum_name.to_uppercase();

  let enum_info_instance_id = Ident::new(
    &format!("{}_ENUM_INFO", enum_name_upper),
    proc_macro2::Span::call_site(),
  );

  let typedef_id = Ident::new(
    &format!("{}_TYPE", enum_name_upper),
    proc_macro2::Span::call_site(),
  );

  let typedef_vec_id = Ident::new(
    &format!("{}_TYPES", enum_name_upper),
    proc_macro2::Span::call_site(),
  );

  let enum_id_expr = shards_enum_attr.id;
  let enum_name_expr = shards_enum_attr.name;
  let enum_desc_expr = shards_enum_attr.desc;

  Ok(
    quote! {
      #vis struct #enum_info_id {
        name: &'static str,
        help: shards::types::OptionalString,
        enum_type: shards::types::Type,
        labels: shards::types::Strings,
        values: Vec<i32>,
        descriptions: shards::types::OptionalStrings,
      }

      lazy_static::lazy_static! {
        #vis static ref #enum_info_instance_id: #enum_info_id = #enum_info_id::new();
        #vis static ref #typedef_id: shards::types::Type = #enum_info_instance_id.enum_type;
        #vis static ref #typedef_vec_id: shards::types::Types = vec![*#typedef_id];
      }

      impl shards::core::EnumRegister for #enum_id {
        fn register() {
          let e = unsafe { &#enum_info_instance_id.enum_type.details.enumeration };
          shards::core::register_enum_internal(e.vendorId, e.typeId, (&*#enum_info_instance_id).into());
        }
      }

      #[allow(non_upper_case_globals)]
      impl<'a> #enum_info_id {
        #(
          pub const #value_ids: shards::SHEnum = #enum_id::#value_ids as i32;
        )*
        #(
          pub const #value_str_ids: &'static str = shards::cstr!(#value_name_lits);
        )*

        fn new() -> Self {
          let mut labels = shards::types::Strings::new();
          #(
            labels.push(Self::#value_str_ids);
          )*

          let mut descriptions = shards::types::OptionalStrings::new();
          #(
            descriptions.push(shards::types::OptionalString(shards::shccstr!(#value_desc_lits)));
          )*

          Self {
            name: shards::cstr!(#enum_name_expr),
            help: shards::types::OptionalString(shards::shccstr!(#enum_desc_expr)),
            enum_type: shards::types::Type::enumeration(shards::types::FRAG_CC, shards::fourCharacterCode(*#enum_id_expr)),
            labels,
            values: vec![#(Self::#value_ids,)*],
            descriptions,
          }
        }
      }

      impl TryFrom<i32> for #enum_id {
        type Error = &'static str;
        fn try_from(value: i32) -> Result<Self, Self::Error> {
          match value {
            #(#enum_info_id::#value_ids => Ok(#enum_id::#value_ids),)*
            _ => Err("Invalid enum value"),
          }
        }
      }

      impl From<#enum_id> for i32 {
        fn from(value: #enum_id) -> Self {
          match value {
            #(#enum_id::#value_ids => #enum_info_id::#value_ids,)*
          }
        }
      }

      impl From<#enum_id> for shards::types::Var {
        fn from(value: #enum_id) -> Self {
          let e = unsafe { &#typedef_id.details.enumeration };
          Self {
            valueType: shards::SHType_Enum,
            payload: shards::SHVarPayload {
              __bindgen_anon_1: shards::SHVarPayload__bindgen_ty_1 {
                __bindgen_anon_3: shards::SHVarPayload__bindgen_ty_1__bindgen_ty_3 {
                  enumValue: value.into(),
                  enumVendorId: e.vendorId,
                  enumTypeId: e.typeId,
                },
              },
            },
            ..Default::default()
          }
        }
      }

      impl TryFrom<&shards::types::Var> for #enum_id {
        type Error = &'static str;
        fn try_from(value: &shards::types::Var) -> Result<Self, Self::Error> {
          if value.valueType != shards::SHType_Enum {
            return Err("Value is not an enum");
          }

          let e = unsafe { &value.payload.__bindgen_anon_1.__bindgen_anon_3 } ;
          let e1 = unsafe { &#typedef_id.details.enumeration };
          if e.enumVendorId != e1.vendorId {
            return Err("Enum vendor id does not match");
          }
          if e.enumTypeId != e1.typeId {
            return Err("Enum type id does not match");
          }
          e.enumValue.try_into()
        }
      }

      impl From<&#enum_info_id> for shards::shardsc::SHEnumInfo {
        fn from(info: &#enum_info_id) -> Self {
          Self {
            name: info.name.as_ptr() as *const std::os::raw::c_char,
            help: info.help.0,
            labels: info.labels.s,
            values: shards::shardsc::SHEnums {
              elements: (&info.values).as_ptr() as *mut i32,
              len: info.values.len() as u32,
              cap: 0
            },
            descriptions: (&info.descriptions).into(),
          }
        }
      }
    }
    .into(),
  )
}

#[proc_macro_derive(shards_enum, attributes(enum_info, enum_value))]
pub fn derive_shards_enum(enum_def: TokenStream) -> TokenStream {
  let enum_: syn::ItemEnum = syn::parse_macro_input!(enum_def);

  match generate_enum_wrapper(enum_) {
    Ok(result) => {
      // eprintln!("derive_shards_enum:\n{}", result);
      result
    }
    Err(err) => err.to_compile_error(),
  }
}

fn parse_param_single(fld: &syn::Field, attr: &syn::Attribute) -> Result<ParamSingle, Error> {
  let Meta::List(list) = &attr.meta else {
    panic!("Param attribute must be a list");
  };
  let args = list
    .parse_args_with(Punctuated::<syn::Expr, syn::Token![,]>::parse_terminated)
    .expect("Expected parsing");

  if let Some((name, desc, types)) = args.into_pairs().map(|x| x.into_value()).collect_tuple() {
    let name = get_expr_str_lit(&name)?;
    let desc = get_expr_str_lit(&desc)?;
    Ok(ParamSingle {
      name,
      var_name: fld.ident.clone().expect("Expected field name"),
      desc,
      types,
    })
  } else {
    Err(
      syn::Error::new(
        attr.bracket_token.span.open(),
        "Param attribute must have 3 arguments: (Name, Description, [Type1, Type2,...]/Types)",
      )
      .into(),
    )
  }
}

fn crc32(name: String) -> u32 {
  let crc = crc::Crc::<u32>::new(&crc::CRC_32_BZIP2);
  let checksum = crc.checksum(name.as_bytes());
  checksum
}

struct Warmable {
  warmup: proc_macro2::TokenStream,
  cleanup: proc_macro2::TokenStream,
}

fn default_warmable(fld: &Field) -> Warmable {
  let ident: &Ident = fld.ident.as_ref().expect("Expected field name");
  Warmable {
    warmup: quote! {self.#ident.warmup(context)?;},
    cleanup: quote! {self.#ident.cleanup(context);},
  }
}

fn to_warmable(
  fld: &Field,
  is_param_set: bool,
  param_set_has_custom_interface: bool,
) -> Option<Warmable> {
  let rust_type = &fld.ty;
  let ident: &Ident = fld.ident.as_ref().expect("Expected field name");
  if let syn::Type::Path(p) = &rust_type {
    let last_type_id = &p.path.segments.last().expect("Empty path").ident;
    if last_type_id == "ParamVar" {
      return Some(Warmable {
        warmup: quote! {self.#ident.warmup(context);},
        cleanup: quote! {self.#ident.cleanup(context);},
      });
    } else if is_param_set {
      if param_set_has_custom_interface {
        return Some(default_warmable(fld));
      } else {
        return Some(Warmable {
          warmup: quote! {self.#ident.warmup_helper(context)?;},
          cleanup: quote! {self.#ident.cleanup_helper(context)?;},
        });
      }
    } else if last_type_id == "ShardsVar" {
      return Some(default_warmable(fld));
    }
  }
  return None;
}

#[derive(Default)]
struct ShardFields {
  params: Vec<Param>,
  required: Option<syn::Ident>,
  warmables: Vec<Warmable>,
}

fn parse_param_set_has_custom_interface(attr: &syn::Attribute) -> Result<bool, Error> {
  let Meta::List(list) = &attr.meta else {
    return Ok(false);
  };

  let args = list
    .parse_args_with(Punctuated::<syn::Expr, syn::Token![,]>::parse_terminated)
    .expect("Expected parsing");

  if let Some((b,)) = args.into_pairs().map(|x| x.into_value()).collect_tuple() {
    let b = get_expr_bool_lit(&b)?;
    return Ok(b);
  }

  Ok(false)
}

fn parse_shard_fields<'a>(
  fields: impl IntoIterator<Item = &'a Field>,
) -> Result<ShardFields, Error> {
  let mut result = ShardFields::default();
  for fld in fields {
    let name: String = get_field_name(&fld);

    for attr in &fld.attrs {
      if attr.path().is_ident("shard_param") {
        match parse_param_single(&fld, &attr) {
          Ok(param) => {
            result.params.push(Param::Single(param));
            if let Some(warmable) = to_warmable(&fld, false, false) {
              result.warmables.push(warmable);
            }
          }
          Err(e) => {
            return Err(e.extended(format!("Failed to parse param for field {}", name).into()))
          }
        }
      } else if attr.path().is_ident("shard_param_set") {
        let has_custom_interface = parse_param_set_has_custom_interface(&attr)?;

        let param_set_ty = fld.ty.clone();
        result.params.push(Param::Set(ParamSet {
          type_name: param_set_ty,
          var_name: fld.ident.clone().expect("Expected field name"),
        }));
        result
          .warmables
          .push(to_warmable(fld, true, has_custom_interface).unwrap());
      } else if attr.path().is_ident("shard_required") {
        result.required = Some(fld.ident.as_ref().expect("Expected field name").clone());
      } else if attr.path().is_ident("shard_warmup") {
        if let Some(warmable) = to_warmable(&fld, false, false) {
          result.warmables.push(warmable);
        }
      }
    }
  }
  Ok(result)
}

struct ShardInfoAttr {
  name: Expr,
  desc: Expr,
}

fn read_shard_info_attr(
  err_span: Span,
  attrs: &Vec<syn::Attribute>,
) -> Result<ShardInfoAttr, Error> {
  for attr in attrs {
    if attr.path().is_ident("shard_info") {
      let args: Punctuated<Expr, Comma> =
        attr.parse_args_with(Punctuated::<syn::Expr, Comma>::parse_terminated)?;
      return if let Some((name, desc)) = args.into_pairs().map(|x| x.into_value()).collect_tuple() {
        // Check if desc is a string literal and not empty
        if let Expr::Lit(syn::ExprLit {
          lit: Lit::Str(lit_str),
          ..
        }) = &desc
        {
          if lit_str.value().trim().is_empty() {
            return Err("Description must not be empty".into());
          }
        } else {
          return Err("Description must be a string literal".into());
        }
        Ok(ShardInfoAttr { name, desc })
      } else {
        Err("shard_info attribute must have 2 arguments: (Name, Description)".into())
      };
    }
  }
  Err(syn::Error::new(err_span, "Missing shard_info attribute").into())
}

struct ParameterAccessor {
  get: proc_macro2::TokenStream,
  set: proc_macro2::TokenStream,
}

fn generate_parameter_accessor(
  offset_id: Ident,
  in_id: Ident,
  p: &Param,
) -> Result<ParameterAccessor, Error> {
  match p {
    Param::Single(single) => {
      let var_name = &single.var_name;
      Ok(ParameterAccessor {
        get: quote! {
          if #in_id == #offset_id {
            return (&self.#var_name).into();
          }
          #offset_id += 1;
        },
        set: quote! {
          if(#in_id == #offset_id) {
            return self.#var_name.set_param(value);
          }
          #offset_id += 1;
        },
      })
    }
    Param::Set(set) => {
      let var_name = &set.var_name;
      let set_type = &set.type_name;
      Ok(ParameterAccessor {
        get: quote! {
          let local_id = #in_id - #offset_id;
          if local_id >= 0 && local_id < (#set_type::num_params() as i32) {
              return (&mut self.#var_name).get_param(local_id);
          }
          #offset_id += #set_type::num_params() as i32;
        },
        set: quote! {
          let local_id = #in_id - #offset_id;
          if local_id >= 0 && local_id < (#set_type::num_params() as i32) {
              return (&mut self.#var_name).set_param(local_id, value);
          }
          #offset_id += #set_type::num_params() as i32;
        },
      })
    }
  }
}

fn generate_parameter_accessors(params: &Vec<Param>) -> Result<proc_macro2::TokenStream, Error> {
  let static_params = params
    .iter()
    .filter_map(|p| match p {
      Param::Single(single) => Some(single),
      Param::Set(_) => None,
    })
    .collect::<Vec<_>>();
  let is_complex = static_params.len() != params.len();

  Ok(if is_complex {
    let offset_id = Ident::new("offset", proc_macro2::Span::call_site());
    let in_id = Ident::new("index", proc_macro2::Span::call_site());
    let accessors = params
      .iter()
      .map(|p| generate_parameter_accessor(offset_id.clone(), in_id.clone(), p))
      .collect::<Result<Vec<_>, _>>()?;
    let getters = accessors.iter().map(|x| &x.get);
    let setters = accessors.iter().map(|x| &x.set);

    quote! {
      fn set_param(&mut self, #in_id: i32, value: &shards::types::Var) -> std::result::Result<(), &'static str> {
        let mut #offset_id: i32 = 0;
        #(#setters)*
        Err("Invalid parameter index")
      }

      fn get_param(&mut self, #in_id: i32) -> shards::types::Var {
        let mut #offset_id: i32 = 0;
        #(#getters)*
        shards::types::Var::default()
      }
    }
  } else {
    let params_idents: Vec<_> = static_params.iter().map(|x| &x.var_name).collect();
    let params_indices: Vec<_> = (0..static_params.len())
      .map(|x| LitInt::new(&format!("{}", x), proc_macro2::Span::call_site()))
      .collect();

    quote! {
      fn set_param(&mut self, index: i32, value: &shards::types::Var) -> std::result::Result<(), &'static str> {
        match index {
          #(
            #params_indices => self.#params_idents.set_param(value),
          )*
          _ => Err("Invalid parameter index"),
        }
      }

      fn get_param(&mut self, index: i32) -> shards::types::Var {
        match index {
          #(
            #params_indices => (&self.#params_idents).into(),
          )*
          _ => shards::types::Var::default(),
        }
      }
    }
  })
}

struct ParamWrapperCode {
  prelude: proc_macro2::TokenStream,
  // warmup bodies
  warmups: Vec<proc_macro2::TokenStream>,
  // cleanup bodies, note that you should reverse these before using them
  cleanups_rev: Vec<proc_macro2::TokenStream>,
  // get_param & set_param
  accessors: proc_macro2::TokenStream,
  // Id for static parameters
  params_static_id: Ident,
  composes: Vec<proc_macro2::TokenStream>,
  shard_fields: ShardFields,
}

fn generate_param_wrapper_code(struct_: &syn::ItemStruct) -> Result<ParamWrapperCode, Error> {
  let struct_id = &struct_.ident;
  let struct_name_upper = struct_id.to_string().to_uppercase();
  let struct_name_lower = struct_id.to_string().to_case(convert_case::Case::Snake);

  let shard_fields = parse_shard_fields(&struct_.fields)?;
  let params = &shard_fields.params;

  let static_params = params
    .iter()
    .filter_map(|p| match p {
      Param::Single(single) => Some(single),
      Param::Set(_) => None,
    })
    .collect::<Vec<_>>();

  let mut array_initializers = Vec::new();
  let param_names: Vec<_> = static_params
    .iter()
    .map(|x| LitStr::new(&x.name, proc_macro2::Span::call_site()))
    .collect();
  let param_descs: Vec<_> = static_params
    .iter()
    .map(|x| LitStr::new(&x.desc, proc_macro2::Span::call_site()))
    .collect();
  let param_types: Vec<_> = static_params
    .iter()
    .map(|x| {
      if let Expr::Array(arr) = &x.types {
        let tmp_id: Ident = Ident::new(
          &format!("{}_{}_TYPES", struct_name_upper, x.name.to_uppercase()),
          proc_macro2::Span::call_site(),
        );
        array_initializers.push(quote! { static ref #tmp_id: shards::types::Types = vec!#arr; });
        syn::parse_quote! { #tmp_id }
      } else {
        x.types.clone()
      }
    })
    .collect();

  let params_static_id: Ident = Ident::new(
    &format!("{}_PARAMETERS", struct_name_upper),
    proc_macro2::Span::call_site(),
  );

  // Generate warmup/cleanup calls for supported types
  let mut warmups = Vec::new();
  let mut cleanups_rev = Vec::new();
  for x in &shard_fields.warmables {
    warmups.push(x.warmup.clone());
    cleanups_rev.push(x.cleanup.clone());
  }

  let mut composes = Vec::new();
  for param in params {
    match param {
      Param::Single(single) => {
        let var_name = &single.var_name;
        let param_name = &single.name;
        let param_types = &single.types;
        // Add optimization: check at runtime if any types can have context variables
        composes.push(quote! {
          {
            let can_have_context_vars = (#param_types).iter().any(|t| shards::util::has_context_variables(t));
            if can_have_context_vars {
              shards::util::collect_required_variables_typed(data, out_required, (&self.#var_name).into(), &#param_types[..], #param_name)?;
            }
          }
        });
      }
      Param::Set(set) => {
        let var_name = &set.var_name;
        composes.push(quote! {
          (&mut self.#var_name).compose_helper(out_required, data)?;
        });
      }
    }
  }

  let build_params_id = Ident::new(
    &format!("build_params_{}", struct_name_lower),
    proc_macro2::Span::call_site(),
  );

  let accessors = generate_parameter_accessors(params)?;
  let append_params = params.iter().map(|p| match p {
    Param::Single(_) => {
      quote! {
        params.push(static_params[static_idx].clone());
        static_idx += 1;
      }
    }
    Param::Set(set) => {
      let set_type = &set.type_name;
      quote! {
        for param in #set_type::parameters() {
          params.push(param.clone());
        }
      }
    }
  });

  let prelude = quote! {
    fn #build_params_id() -> Vec<shards::types::ParameterInfo> {
      let static_params : Vec<shards::types::ParameterInfo> = vec![
        #((
          shards::cstr!(#param_names),
          shards::shccstr!(#param_descs),
          &#param_types[..]
        ).into()),*
      ];
      let mut params = Vec::new();
      let mut static_idx: usize = 0;
      #(#append_params)*
      params
    }

    lazy_static::lazy_static! {
      #(#array_initializers)*
      static ref #params_static_id: shards::types::Parameters = #build_params_id();
    }
  };

  Ok(ParamWrapperCode {
    prelude,
    warmups,
    params_static_id: params_static_id,
    cleanups_rev,
    accessors,
    composes,
    shard_fields,
  })
}

fn process_param_set_impl(struct_: syn::ItemStruct) -> Result<TokenStream, Error> {
  let struct_id = &struct_.ident;
  let ParamWrapperCode {
    prelude,
    warmups,
    cleanups_rev,
    accessors,
    params_static_id,
    composes,
    ..
  } = generate_param_wrapper_code(&struct_)?;
  let cleanups = cleanups_rev.iter().rev();

  Ok(quote! {
    #prelude

    impl shards::shard::ParameterSet for #struct_id {
      fn parameters() -> &'static shards::types::Parameters {
          &#params_static_id
      }

      fn num_params() -> usize { #params_static_id.len() }

      #accessors

      fn warmup_helper(&mut self, context: &shards::types::Context) -> std::result::Result<(), &'static str> {
        #( #warmups )*
        Ok(())
      }

      fn cleanup_helper(&mut self, context: std::option::Option<&shards::types::Context>) -> std::result::Result<(), &'static str> {
        #( #cleanups )*
        Ok(())
      }

      fn compose_helper(&mut self, out_required: &mut shards::types::ExposedTypes, data: &shards::types::InstanceData) -> std::result::Result<(), &'static str> {
        #( #composes )*
        Ok(())
      }
    }
  }.into())
}

fn process_shard_helper_impl(struct_: syn::ItemStruct) -> Result<TokenStream, Error> {
  let struct_id = &struct_.ident;

  let shard_info = read_shard_info_attr(struct_id.span(), &struct_.attrs)?;

  let ParamWrapperCode {
    prelude,
    warmups,
    cleanups_rev,
    accessors,
    params_static_id,
    composes,
    shard_fields,
  } = generate_param_wrapper_code(&struct_)?;
  let cleanups = cleanups_rev.iter().rev();

  let shard_name_expr = shard_info.name;
  let shard_name = get_expr_str_lit(&shard_name_expr)?;
  let shard_desc_expr = shard_info.desc;

  let crc = crc32(format!("{}-rust-0x20250822", shard_name));

  let (required_variables_opt, compose_helper) = if let Some(required) = &shard_fields.required {
    (
      quote! { Some(&self.#required) },
      quote! {
        fn compose_helper(&mut self, data: &shards::types::InstanceData) -> std::result::Result<(), &'static str> {
          self.#required.clear();
          let out_required = &mut self.#required;
          #(#composes)*
          Ok(())
        }
      },
    )
  } else {
    (quote! { None }, quote! {})
  };

  Ok(quote! {
    #prelude

    impl shards::shard::ShardGenerated for #struct_id {
      fn register_name() -> &'static str {
        shards::cstr!(#shard_name_expr)
      }

      fn name(&mut self) -> &str {
        #shard_name_expr
      }

      fn hash() -> u32
      where
        Self: Sized,
      {
        #crc
      }

      fn help(&mut self) -> shards::types::OptionalString {
        shards::types::OptionalString(shards::shccstr!(#shard_desc_expr))
      }

      fn parameters(&mut self) -> Option<&shards::types::Parameters> {
          Some(&#params_static_id)
      }

      #accessors

      fn required_variables(&mut self) -> Option<&shards::types::ExposedTypes> {
        #required_variables_opt
      }
    }

    impl #struct_id {
      #compose_helper

      fn warmup_helper(&mut self, context: &shards::types::Context) -> std::result::Result<(), &'static str> {
        #( #warmups )*
        Ok(())
      }


      fn cleanup_helper(&mut self, context: std::option::Option<&shards::types::Context>) -> std::result::Result<(), &'static str> {
        #( #cleanups )*
        Ok(())
      }
    }
  }.into())
}

#[proc_macro_derive(
  shard,
  attributes(shard_info, shard_param, shard_param_set, shard_required, shard_warmup)
)]
pub fn derive_shard(struct_def: TokenStream) -> TokenStream {
  let struct_: syn::ItemStruct = syn::parse_macro_input!(struct_def as syn::ItemStruct);

  match process_shard_helper_impl(struct_) {
    Ok(result) => {
      // eprintln!("derive_shard:\n{}", result);
      result
    }
    Err(err) => err.to_compile_error(),
  }
}

#[proc_macro_derive(param_set, attributes(shard_param, shard_param_set, shard_warmup))]
pub fn derive_param_set(struct_def: TokenStream) -> TokenStream {
  let struct_: syn::ItemStruct = syn::parse_macro_input!(struct_def as syn::ItemStruct);

  match process_param_set_impl(struct_) {
    Ok(result) => {
      // eprintln!("derive_param_set:\n{}", result);
      result
    }
    Err(err) => err.to_compile_error(),
  }
}

fn generate_impl_wrapper(impl_: syn::ItemImpl) -> Result<TokenStream, Error> {
  let struct_ty = impl_.self_ty.as_ref();

  let mut have_impls: HashSet<String> = HashSet::new();
  for item in &impl_.items {
    if let ImplItem::Fn(fn_item) = item {
      let fn_name = fn_item.sig.ident.to_string();
      if IMPLS_TO_CHECK_SET.contains(fn_name.as_str()) {
        have_impls.insert(fn_name);
      }
    }
  }

  // Generate hasXXX() -> bool functions for all optional functions
  let impls = IMPLS_TO_CHECK.iter().map(|x| {
    let has_fn_id = Ident::new(&format!("has_{}", x), proc_macro2::Span::call_site());
    let have_function = syn::LitBool::new(have_impls.contains(*x), proc_macro2::Span::call_site());
    quote! { fn #has_fn_id() -> bool { #have_function } }
  });

  Ok(
    quote! {
      #[allow(non_snake_case)]
      impl shards::shard::ShardGeneratedOverloads for #struct_ty {
        #(#impls)*
      }

      #[allow(non_snake_case)]
      #impl_
    }
    .into(),
  )
}

#[proc_macro_attribute]
pub fn shard_impl(_attr: TokenStream, item: TokenStream) -> TokenStream {
  let impl_: syn::ItemImpl = syn::parse_macro_input!(item);
  match generate_impl_wrapper(impl_) {
    Ok(result) => {
      // eprintln!("shard_impl:\n{}", result);
      result
    }
    Err(err) => err.to_compile_error(),
  }
}

// ============================================================================
// Simple Shard Macro - Simplified shard definition via function attributes
// ============================================================================

struct SimpleShardAttrArgs {
  name: LitStr,
  help: LitStr,
}

impl syn::parse::Parse for SimpleShardAttrArgs {
  fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
    let name: LitStr = input.parse()?;
    input.parse::<syn::Token![,]>()?;
    let help: LitStr = input.parse()?;
    Ok(Self { name, help })
  }
}

struct SimpleParamInfo {
  name: String,
  rust_name: syn::Ident,
  rust_type: syn::Type,
  description: String,
  default: Option<syn::Expr>,
  is_var: bool, // true for ParamVar (context variables)
}

struct SimpleExternalInfo {
  external_name: String, // The context variable name to look up
  rust_name: syn::Ident,
  rust_type: syn::Type,
}

// Helper struct to parse #[external("name")]
struct SimpleExternalAttr {
  name: String,
}

impl syn::parse::Parse for SimpleExternalAttr {
  fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
    let name: LitStr = input.parse()?;
    Ok(Self { name: name.value() })
  }
}

// Helper struct to parse #[param("Name", "Desc")] or #[param("Name", "Desc", default = value)]
struct SimpleParamAttr {
  name: String,
  description: String,
  default: Option<syn::Expr>,
}

impl syn::parse::Parse for SimpleParamAttr {
  fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
    let name: LitStr = input.parse()?;
    input.parse::<syn::Token![,]>()?;
    let description: LitStr = input.parse()?;

    let default = if input.peek(syn::Token![,]) {
      input.parse::<syn::Token![,]>()?;
      let ident: syn::Ident = input.parse()?;
      if ident != "default" {
        return Err(syn::Error::new(ident.span(), "Expected 'default'"));
      }
      input.parse::<syn::Token![=]>()?;
      Some(input.parse()?)
    } else {
      None
    };

    Ok(Self {
      name: name.value(),
      description: description.value(),
      default,
    })
  }
}

fn generate_simple_shard(args: SimpleShardAttrArgs, func: syn::ItemFn) -> Result<TokenStream, Error> {
  let shard_name = args.name.value();
  let shard_help = args.help.value();

  // Generate struct name from shard name (e.g., "Math.Scale" -> "MathScaleShard")
  let struct_name = shard_name.replace(".", "");
  let struct_id = Ident::new(&format!("{}Shard", struct_name), Span::call_site());
  let params_static_id = Ident::new(
    &format!("{}_PARAMETERS", struct_name.to_uppercase()),
    Span::call_site(),
  );

  // Parse function signature
  let mut input_type: Option<syn::Type> = None;
  let mut input_name: Option<syn::Ident> = None;
  let mut is_unit_input = false;
  let mut params: Vec<SimpleParamInfo> = Vec::new();
  let mut externals: Vec<SimpleExternalInfo> = Vec::new();

  for (i, arg) in func.sig.inputs.iter().enumerate() {
    let syn::FnArg::Typed(pat_type) = arg else {
      return Err("Expected typed argument".into());
    };

    let arg_type = pat_type.ty.as_ref().clone();

    // First arg is input
    if i == 0 {
      // Check if input type is unit ()
      if let syn::Type::Tuple(tuple) = &arg_type {
        if tuple.elems.is_empty() {
          is_unit_input = true;
          input_type = Some(arg_type);
          // Use a dummy name for unit input
          input_name = Some(Ident::new("_input", Span::call_site()));
          continue;
        }
      }

      // Handle both ident patterns and wildcard patterns
      let arg_name = match pat_type.pat.as_ref() {
        syn::Pat::Ident(pat_ident) => pat_ident.ident.clone(),
        syn::Pat::Wild(_) => Ident::new("_input", Span::call_site()),
        _ => return Err("Expected identifier or wildcard pattern".into()),
      };

      input_type = Some(arg_type);
      input_name = Some(arg_name);
      continue;
    }

    // Rest are parameters - need ident pattern
    let syn::Pat::Ident(pat_ident) = pat_type.pat.as_ref() else {
      return Err("Expected identifier pattern for parameter".into());
    };

    let arg_name = pat_ident.ident.clone();

    // Rest are parameters - parse #[param(...)] or #[param_var(...)] attribute
    let mut param_name = arg_name.to_string();
    let mut param_desc = String::new();
    let mut param_default: Option<syn::Expr> = None;
    let mut is_var = false;

    let mut is_external = false;
    let mut external_name = String::new();

    for attr in &pat_type.attrs {
      if attr.path().is_ident("param") {
        let parsed: SimpleParamAttr = attr.parse_args()?;
        param_name = parsed.name;
        param_desc = parsed.description;
        param_default = parsed.default;
      } else if attr.path().is_ident("param_var") {
        // For context variable parameters
        let parsed: SimpleParamAttr = attr.parse_args()?;
        param_name = parsed.name;
        param_desc = parsed.description;
        param_default = parsed.default;
        is_var = true;
      } else if attr.path().is_ident("external") {
        // For external context variables (not user-configurable params)
        let parsed: SimpleExternalAttr = attr.parse_args()?;
        external_name = parsed.name;
        is_external = true;
      }
    }

    if is_external {
      externals.push(SimpleExternalInfo {
        external_name,
        rust_name: arg_name,
        rust_type: arg_type,
      });
    } else {
      params.push(SimpleParamInfo {
        name: param_name,
        rust_name: arg_name,
        rust_type: arg_type,
        description: param_desc,
        default: param_default,
        is_var,
      });
    }
  }

  let input_type = input_type.ok_or("Function must have at least one argument (input)")?;
  let input_name = input_name.ok_or("Function must have at least one argument (input)")?;

  // Get output type from return type
  let mut returns_result = false;
  let mut returns_typed_out = false; // BytesOut, StringOut, etc.
  let output_type = match &func.sig.output {
    syn::ReturnType::Type(_, ty) => {
      // Handle Result<T, _> wrapper
      if let syn::Type::Path(path) = ty.as_ref() {
        let type_name = path.path.segments.last().map(|s| s.ident.to_string());

        if type_name.as_deref() == Some("Result") {
          returns_result = true;
          // Extract T from Result<T, E>
          if let syn::PathArguments::AngleBracketed(args) =
            &path.path.segments.last().unwrap().arguments
          {
            if let Some(syn::GenericArgument::Type(t)) = args.args.first() {
              // Check if inner type is BytesOut/StringOut
              if let syn::Type::Path(inner_path) = t {
                let inner_name = inner_path.path.segments.last().map(|s| s.ident.to_string());
                if matches!(inner_name.as_deref(), Some("BytesOut") | Some("StringOut")) {
                  returns_typed_out = true;
                }
              }
              t.clone()
            } else {
              return Err("Invalid Result type".into());
            }
          } else {
            return Err("Invalid Result type".into());
          }
        } else if matches!(type_name.as_deref(), Some("BytesOut") | Some("StringOut")) {
          returns_typed_out = true;
          ty.as_ref().clone()
        } else {
          ty.as_ref().clone()
        }
      } else {
        ty.as_ref().clone()
      }
    }
    syn::ReturnType::Default => return Err("Function must have return type".into()),
  };

  // Generate struct fields for params
  let param_fields: Vec<_> = params
    .iter()
    .map(|p| {
      let name = &p.rust_name;
      if p.is_var {
        quote! { #name: shards::types::ParamVar }
      } else {
        quote! { #name: shards::types::ClonedVar }
      }
    })
    .collect();

  // Generate struct fields for externals
  let external_fields: Vec<_> = externals
    .iter()
    .map(|e| {
      let name = &e.rust_name;
      quote! { #name: shards::types::ParamVar }
    })
    .collect();

  // Generate default values for params
  let param_defaults: Vec<_> = params
    .iter()
    .map(|p| {
      let name = &p.rust_name;
      let default_val = p
        .default
        .as_ref()
        .map(|d| quote! { (#d).into() })
        .unwrap_or_else(|| quote! { Default::default() });

      if p.is_var {
        quote! { #name: shards::types::ParamVar::new(#default_val) }
      } else {
        quote! { #name: #default_val }
      }
    })
    .collect();

  // Generate default values for externals (using new_named)
  let external_defaults: Vec<_> = externals
    .iter()
    .map(|e| {
      let name = &e.rust_name;
      let ext_name = &e.external_name;
      quote! { #name: shards::types::ParamVar::new_named(#ext_name) }
    })
    .collect();

  // Generate parameter info
  let param_names: Vec<_> = params
    .iter()
    .map(|p| LitStr::new(&p.name, Span::call_site()))
    .collect();
  let param_descs: Vec<_> = params
    .iter()
    .map(|p| LitStr::new(&p.description, Span::call_site()))
    .collect();
  let param_rust_names: Vec<_> = params.iter().map(|p| &p.rust_name).collect();
  let param_types: Vec<_> = params.iter().map(|p| &p.rust_type).collect();
  let param_indices: Vec<_> = (0..params.len())
    .map(|i| LitInt::new(&format!("{}", i), Span::call_site()))
    .collect();

  // Generate parameter extraction in activate
  let param_extractions: Vec<_> = params
    .iter()
    .map(|p| {
      let name = &p.rust_name;
      let ty = &p.rust_type;
      if p.is_var {
        quote! {
          let #name: #ty = self.#name.get().as_ref().try_into()?;
        }
      } else {
        quote! {
          let #name: #ty = self.#name.0.as_ref().try_into()?;
        }
      }
    })
    .collect();

  // Generate warmup/cleanup calls for ParamVar params
  let param_warmups: Vec<_> = params
    .iter()
    .filter(|p| p.is_var)
    .map(|p| {
      let name = &p.rust_name;
      quote! { self.#name.warmup(context); }
    })
    .collect();

  let param_cleanups: Vec<_> = params
    .iter()
    .filter(|p| p.is_var)
    .map(|p| {
      let name = &p.rust_name;
      quote! { self.#name.cleanup(context); }
    })
    .collect();

  // Generate warmup/cleanup calls for externals
  let external_warmups: Vec<_> = externals
    .iter()
    .map(|e| {
      let name = &e.rust_name;
      quote! { self.#name.warmup(context); }
    })
    .collect();

  let external_cleanups: Vec<_> = externals
    .iter()
    .map(|e| {
      let name = &e.rust_name;
      quote! { self.#name.cleanup(context); }
    })
    .collect();

  // The original function body
  let func_body = &func.block;
  let func_name = &func.sig.ident;

  // Collect external rust names for function call (needed early for activate_call)
  let external_rust_names: Vec<_> = externals.iter().map(|e| &e.rust_name).collect();

  // CRC for shard hash
  let crc = crc32(format!("{}-rust-0x20250822", shard_name));

  // Determine if we have params
  let has_params = !params.is_empty();

  let parameters_impl = if has_params {
    quote! {
      fn parameters(&mut self) -> Option<&shards::types::Parameters> {
        Some(&#params_static_id)
      }
    }
  } else {
    quote! {
      fn parameters(&mut self) -> Option<&shards::types::Parameters> {
        None
      }
    }
  };

  let set_get_param_impl = if has_params {
    quote! {
      fn set_param(&mut self, index: i32, value: &shards::types::Var) -> std::result::Result<(), &'static str> {
        match index {
          #(
            #param_indices => self.#param_rust_names.set_param(value),
          )*
          _ => Err("Invalid parameter index"),
        }
      }

      fn get_param(&mut self, index: i32) -> shards::types::Var {
        match index {
          #(
            #param_indices => (&self.#param_rust_names).into(),
          )*
          _ => shards::types::Var::default(),
        }
      }
    }
  } else {
    quote! {
      fn set_param(&mut self, _index: i32, _value: &shards::types::Var) -> std::result::Result<(), &'static str> {
        Err("No parameters")
      }

      fn get_param(&mut self, _index: i32) -> shards::types::Var {
        shards::types::Var::default()
      }
    }
  };

  // Generate parameter type arrays that include both base type and var type
  let param_type_array_ids: Vec<_> = params
    .iter()
    .enumerate()
    .map(|(i, _)| {
      Ident::new(
        &format!("{}_PARAM_{}_TYPES", struct_name.to_uppercase(), i),
        Span::call_site(),
      )
    })
    .collect();

  let params_static_def = if has_params {
    let param_type_arrays: Vec<_> = params
      .iter()
      .zip(param_type_array_ids.iter())
      .map(|(p, id)| {
        let ty = &p.rust_type;
        quote! {
          static ref #id: shards::types::Types = vec![
            <#ty as shards::types::ShardType>::shards_type(),
            <#ty as shards::types::ShardType>::shards_var_type()
          ];
        }
      })
      .collect();

    quote! {
      lazy_static::lazy_static! {
        #(#param_type_arrays)*
        static ref #params_static_id: shards::types::Parameters = vec![
          #(
            (
              shards::cstr!(#param_names),
              shards::shccstr!(#param_descs),
              #param_type_array_ids.as_slice()
            ).into()
          ),*
        ];
      }
    }
  } else {
    quote! {}
  };

  // Generate activate call - differs for unit input vs normal input, and Result vs plain return
  // Note: externals are passed after params
  let activate_call = match (is_unit_input, returns_result) {
    (true, true) => quote! { #func_name((), #(#param_rust_names,)* #(#external_rust_names),*)? },
    (true, false) => quote! { #func_name((), #(#param_rust_names,)* #(#external_rust_names),*) },
    (false, true) => quote! {
      {
        let #input_name: #input_type = input.try_into()?;
        #func_name(#input_name, #(#param_rust_names,)* #(#external_rust_names),*)?
      }
    },
    (false, false) => quote! {
      {
        let #input_name: #input_type = input.try_into()?;
        #func_name(#input_name, #(#param_rust_names,)* #(#external_rust_names),*)
      }
    },
  };

  // Generate output assignment - typed outputs (BytesOut, StringOut) are already ClonedVar wrappers
  let output_assignment = if returns_typed_out {
    quote! { self.output = result.0; }
  } else {
    quote! { self.output = result.into(); }
  };

  // Generate compose calls for param_var parameters
  let has_var_params = params.iter().any(|p| p.is_var);
  let has_externals = !externals.is_empty();
  let param_var_composes: Vec<_> = params
    .iter()
    .filter(|p| p.is_var)
    .map(|p| {
      let name = &p.rust_name;
      let param_name = &p.name;
      let ty = &p.rust_type;
      quote! {
        {
          let param_types: shards::types::Types = vec![
            <#ty as shards::types::ShardType>::shards_type(),
            <#ty as shards::types::ShardType>::shards_var_type()
          ];
          shards::util::collect_required_variables_typed(
            data,
            &mut self.required,
            (&self.#name).into(),
            &param_types[..],
            #param_name
          )?;
        }
      }
    })
    .collect();

  // Generate compose calls for externals - directly add to required
  let external_composes: Vec<_> = externals
    .iter()
    .map(|e| {
      let ext_name = &e.external_name;
      let ty = &e.rust_type;
      quote! {
        {
          let exp_info = shards::types::ExposedInfo {
            exposedType: <#ty as shards::types::ShardType>::shards_type(),
            name: shards::cstr!(#ext_name).as_ptr() as *const std::os::raw::c_char,
            help: shards::shccstr!(""),
            ..shards::types::ExposedInfo::default()
          };
          self.required.push(exp_info);
        }
      }
    })
    .collect();

  // Generate extraction for externals in activate
  let external_extractions: Vec<_> = externals
    .iter()
    .map(|e| {
      let name = &e.rust_name;
      let ty = &e.rust_type;
      quote! {
        let #name: #ty = self.#name.get().as_ref().try_into()?;
      }
    })
    .collect();

  // Collect external types for function signature
  let external_types: Vec<_> = externals.iter().map(|e| &e.rust_type).collect();

  // Generate the inner function signature based on whether it returns Result or plain type
  let inner_function = if returns_result {
    quote! {
      #[inline]
      fn #func_name(#input_name: #input_type, #(#param_rust_names: #param_types,)* #(#external_rust_names: #external_types),*) -> std::result::Result<#output_type, &'static str> {
        #func_body
      }
    }
  } else {
    quote! {
      #[inline]
      fn #func_name(#input_name: #input_type, #(#param_rust_names: #param_types,)* #(#external_rust_names: #external_types),*) -> #output_type {
        #func_body
      }
    }
  };

  // Determine if compose is needed (var params or externals)
  let needs_compose = has_var_params || has_externals;

  // Generate the full shard implementation
  let output = quote! {
    // The inner function with the actual logic
    #inner_function

    pub struct #struct_id {
      required: shards::types::ExposedTypes,
      #(#param_fields,)*
      #(#external_fields,)*
      output: shards::types::ClonedVar,
    }

    impl Default for #struct_id {
      fn default() -> Self {
        Self {
          required: shards::types::ExposedTypes::new(),
          #(#param_defaults,)*
          #(#external_defaults,)*
          output: shards::types::ClonedVar::default(),
        }
      }
    }

    #params_static_def

    impl shards::shard::ShardGenerated for #struct_id {
      fn register_name() -> &'static str {
        shards::cstr!(#shard_name)
      }

      fn name(&mut self) -> &str {
        #shard_name
      }

      fn hash() -> u32 {
        #crc
      }

      fn help(&mut self) -> shards::types::OptionalString {
        shards::types::OptionalString(shards::shccstr!(#shard_help))
      }

      #parameters_impl

      #set_get_param_impl

      fn required_variables(&mut self) -> Option<&shards::types::ExposedTypes> {
        Some(&self.required)
      }
    }

    impl shards::shard::ShardGeneratedOverloads for #struct_id {
      fn has_compose() -> bool { #needs_compose }
      fn has_warmup() -> bool { true }
      fn has_mutate() -> bool { false }
      fn has_crossover() -> bool { false }
      fn has_get_state() -> bool { false }
      fn has_set_state() -> bool { false }
      fn has_reset_state() -> bool { false }
    }

    impl shards::shard::Shard for #struct_id {
      fn input_types(&mut self) -> &shards::types::Types {
        <#input_type as shards::types::ShardType>::shards_types()
      }

      fn output_types(&mut self) -> &shards::types::Types {
        <#output_type as shards::types::ShardType>::shards_types()
      }

      fn warmup(&mut self, context: &shards::types::Context) -> std::result::Result<(), &str> {
        #(#param_warmups)*
        #(#external_warmups)*
        Ok(())
      }

      fn cleanup(&mut self, context: std::option::Option<&shards::types::Context>) -> std::result::Result<(), &str> {
        #(#param_cleanups)*
        #(#external_cleanups)*
        self.output = shards::types::ClonedVar::default();
        Ok(())
      }

      fn compose(&mut self, data: &shards::types::InstanceData) -> std::result::Result<shards::types::Type, &str> {
        self.required.clear();
        #(#param_var_composes)*
        #(#external_composes)*
        Ok(<#output_type as shards::types::ShardType>::shards_type())
      }

      fn activate(&mut self, _context: &shards::types::Context, input: &shards::types::Var) -> std::result::Result<std::option::Option<shards::types::Var>, &str> {
        #(#param_extractions)*
        #(#external_extractions)*

        let result = #activate_call;
        #output_assignment
        Ok(Some(self.output.0))
      }
    }
  };

  Ok(output.into())
}

/// Simple shard definition via function attribute.
///
/// This macro allows defining shards with a simple function syntax,
/// automatically generating all the boilerplate code.
///
/// # Example
/// ```rust,ignore
/// #[simple_shard("Math.Scale", "Scales input by factor")]
/// fn scale(
///     input: i64,
///     #[param("Factor", "Scale factor", default = 2)]
///     factor: i64,
/// ) -> Result<i64, &'static str> {
///     Ok(input * factor)
/// }
/// ```
///
/// This will generate a `MathScaleShard` struct with all necessary
/// trait implementations.
#[proc_macro_attribute]
pub fn simple_shard(attr: TokenStream, item: TokenStream) -> TokenStream {
  let args = syn::parse_macro_input!(attr as SimpleShardAttrArgs);
  let func = syn::parse_macro_input!(item as syn::ItemFn);

  match generate_simple_shard(args, func) {
    Ok(result) => {
      // eprintln!("simple_shard:\n{}", result);
      result
    }
    Err(err) => err.to_compile_error(),
  }
}
