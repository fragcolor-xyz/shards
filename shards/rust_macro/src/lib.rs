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
  has_custom_interface: bool,
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
        desc: shards::types::OptionalString,
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
          shards::core::register_legacy_enum(e.vendorId, e.typeId, (&*#enum_info_instance_id).into());
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
            desc: shards::types::OptionalString(shards::shccstr!(#enum_desc_expr)),
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
          has_custom_interface,
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
        composes.push(quote! {
          shards::util::collect_required_variables(&data.shared, out_required, (&self.#var_name).into())?;
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

  let crc = crc32(format!("{}-rust-0x20200101", shard_name));

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
