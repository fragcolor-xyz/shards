extern crate proc_macro;
use std::{boxed, collections::HashSet};

use itertools::Itertools;
use proc_macro::TokenStream;
use proc_macro2::Span;
use quote::quote;
use syn::{
  punctuated::Punctuated, token::Comma, Expr, Field, Ident, ImplItem, LitInt, LitStr, Meta,
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

struct Param {
  name: String,
  var_name: String,
  desc: String,
  types: syn::Expr,
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
      struct #enum_info_id {
        name: &'static str,
        desc: shards::types::OptionalString,
        enum_type: shards::types::Type,
        labels: shards::types::Strings,
        values: Vec<i32>,
        descriptions: shards::types::OptionalStrings,
      }

      lazy_static! {
        static ref #enum_info_instance_id: #enum_info_id = #enum_info_id::new();
        static ref #typedef_id: shards::types::Type = #enum_info_instance_id.enum_type;
        static ref #typedef_vec_id: shards::types::Types = vec![*#typedef_id];
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
          pub const #value_str_ids: &'static str = cstr!(#value_name_lits);
        )*

        fn new() -> Self {
          let mut labels = shards::types::Strings::new();
          #(
            labels.push(Self::#value_str_ids);
          )*

          let mut descriptions = shards::types::OptionalStrings::new();
          #(
            descriptions.push(shards::types::OptionalString(shccstr!(#value_desc_lits)));
          )*

          Self {
            name: cstr!(#enum_name_expr),
            desc: shards::types::OptionalString(shccstr!(#enum_desc_expr)),
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

fn parse_param_field(fld: &syn::Field, attr: &syn::Attribute) -> Result<Param, Error> {
  let Meta::List(list) = &attr.meta else {
    panic!("Param attribute must be a list");
  };
  let args = list
    .parse_args_with(Punctuated::<syn::Expr, syn::Token![,]>::parse_terminated)
    .expect("Expected parsing");

  if let Some((name, desc, types)) = args.into_pairs().map(|x| x.into_value()).collect_tuple() {
    let name = get_expr_str_lit(&name)?;
    let desc = get_expr_str_lit(&desc)?;
    let var_name = get_field_name(&fld);
    Ok(Param {
      name,
      var_name,
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

#[derive(Default)]
struct ShardFields {
  params: Vec<Param>,
  required: Option<syn::Ident>,
  warmables: Vec<syn::Field>,
}

fn parse_shard_fields<'a>(
  fields: impl IntoIterator<Item = &'a Field>,
) -> Result<ShardFields, Error> {
  let mut result = ShardFields::default();
  for fld in fields {
    let name: String = get_field_name(&fld);

    for attr in &fld.attrs {
      if attr.path().is_ident("shard_param") {
        match parse_param_field(&fld, &attr) {
          Ok(param) => {
            result.params.push(param);
            result.warmables.push(fld.clone());
          }
          Err(e) => {
            return Err(e.extended(format!("Failed to parse param for field {}", name).into()))
          }
        }
      } else if attr.path().is_ident("shard_required") {
        result.required = Some(fld.ident.as_ref().expect("Expected field name").clone());
      } else if attr.path().is_ident("shard_warmup") {
        result.warmables.push(fld.clone());
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
        Ok(ShardInfoAttr { name, desc })
      } else {
        Err("shard_info attribute must have 2 arguments: (Name, Description)".into())
      };
    }
  }
  Err(syn::Error::new(err_span, "Missing shard_info attribute").into())
}

fn process_shard_helper_impl(struct_: syn::ItemStruct) -> Result<TokenStream, Error> {
  let struct_id = struct_.ident;
  let struct_name_upper = struct_id.to_string().to_uppercase();

  let shard_info = read_shard_info_attr(struct_id.span(), &struct_.attrs)?;

  let shard_fields = parse_shard_fields(&struct_.fields)?;
  let params = &shard_fields.params;

  let param_names: Vec<_> = params
    .iter()
    .map(|x| LitStr::new(&x.name, proc_macro2::Span::call_site()))
    .collect();
  let param_descs: Vec<_> = params
    .iter()
    .map(|x| LitStr::new(&x.desc, proc_macro2::Span::call_site()))
    .collect();
  let mut array_initializers = Vec::new();
  let param_types: Vec<_> = params
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
  let param_idents: Vec<_> = params
    .iter()
    .map(|x| Ident::new(&x.var_name, proc_macro2::Span::call_site()))
    .collect();

  let params_static_id: Ident = Ident::new(
    &format!("{}_PARAMETERS", struct_name_upper),
    proc_macro2::Span::call_site(),
  );

  // Generate warmup/cleanup calls for supported types
  let mut warmups = Vec::new();
  let mut cleanups_rev = Vec::new();
  for x in &shard_fields.warmables {
    let rust_type = &x.ty;
    let ident = x.ident.as_ref().expect("Expected field name");
    if let syn::Type::Path(p) = &rust_type {
      let last_type_id = &p.path.segments.last().expect("Empty path").ident;
      if last_type_id == "ParamVar" {
        warmups.push(quote! {self.#ident.warmup(context);});
        cleanups_rev.push(quote! {self.#ident.cleanup();});
      } else if last_type_id == "ShardsVar" {
        warmups.push(quote! {self.#ident.warmup(context)?;});
        cleanups_rev.push(quote! {self.#ident.cleanup();});
      }
    }
  }
  let cleanups = cleanups_rev.iter().rev();

  let shard_name_expr = shard_info.name;
  let shard_name = get_expr_str_lit(&shard_name_expr)?;
  let shard_desc_expr = shard_info.desc;

  let params_idents: Vec<_> = params
    .iter()
    .map(|x| Ident::new(&x.var_name, proc_macro2::Span::call_site()))
    .collect();
  let params_indices: Vec<_> = (0..params.len())
    .map(|x| LitInt::new(&format!("{}", x), proc_macro2::Span::call_site()))
    .collect();

  let crc = crc32(format!("{}-rust-0x20200101", shard_name));

  let (required_variables_opt, compose_helper) = if let Some(required) = &shard_fields.required {
    (
      quote! { Some(&self.#required) },
      quote! {
        fn compose_helper(&mut self, data: &shards::types::InstanceData) -> std::result::Result<(), &'static str> {
          self.#required.clear();
          #(
            shards::util::collect_required_variables(&data.shared, &mut self.#required, (&self.#param_idents).into())?;
          )*
          Ok(())
        }
      },
    )
  } else {
    (quote! { None }, quote! {})
  };

  Ok(quote! {
    lazy_static! {
      #(#array_initializers)*
      static ref #params_static_id: shards::types::Parameters = vec![
        #((
          cstr!(#param_names),
          shccstr!(#param_descs),
          &#param_types[..]
        ).into()),*
      ];
    }

    impl shards::shard::ShardGenerated for #struct_id {
      fn register_name() -> &'static str {
        cstr!(#shard_name_expr)
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
        OptionalString(shccstr!(#shard_desc_expr))
      }

      fn parameters(&mut self) -> Option<&shards::types::Parameters> {
          Some(&#params_static_id)
      }

      fn set_param(&mut self, index: i32, value: &Var) -> std::result::Result<(), &str> {
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
          _ => Var::default(),
        }
      }

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


      fn cleanup_helper(&mut self) -> std::result::Result<(), &'static str> {
        #( #cleanups )*
        Ok(())
      }
    }
  }.into())
}

#[proc_macro_derive(
  shard,
  attributes(shard_info, shard_param, shard_required, shard_warmup)
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
