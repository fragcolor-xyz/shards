extern crate proc_macro;
use std::boxed;

use itertools::Itertools;
use proc_macro::TokenStream;
use quote::quote;
use syn::{
  Ident,
  parse::{Parse, ParseStream},
  punctuated::Punctuated,
  DeriveInput, Expr,  Field, FieldsNamed,  LitStr, Meta,  LitInt, token::Struct, DataStruct, ExprPath, Path,
};

// Placeholder just to allow #[Param] custom attributes
#[proc_macro_derive(Shard, attributes(Param))]
pub fn derive_shard(_item: TokenStream) -> TokenStream {
  TokenStream::new()
}

struct GenerateShardImplInput {
  di: DeriveInput,
  shard_desc: ShardDesc,
}

impl Parse for GenerateShardImplInput {
  fn parse(input: ParseStream) -> Result<Self, syn::Error> {
    let name = input.parse::<LitStr>()?;
    let desc = input.parse::<LitStr>()?;
    let di = input.parse()?;
    Ok(Self {
      di,
      shard_desc: ShardDesc {
        name: name.value(),
        desc: desc.value(),
      },
    })
  }
}

struct Param {
  name: String,
  var_name: String,
  desc: String,
  types: syn::Expr,
  rust_type: syn::Type,
}

type Error = boxed::Box<dyn std::error::Error>;

fn get_struct_name(parsed: &DeriveInput) -> String {
  parsed.ident.to_string()
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
      rust_type: fld.ty.clone(),
    })
  } else {
    Err("Param attribute must have 3 arguments".into())
  }
}

#[derive(Default)]
struct ShardDesc {
  name: String,
  desc: String,
}

fn crc32(name: String) -> u32 {
  let crc = crc::Crc::<u32>::new(&crc::CRC_32_BZIP2);
  let checksum = crc.checksum(name.as_bytes());
  checksum
}

fn generate_shard_impl_functions(
  parsed: &DeriveInput,
  shard_desc: ShardDesc,
  params: Vec<Param>,
) -> Result<TokenStream, Error> {
  let name_lit = syn::LitStr::new(&shard_desc.name, proc_macro2::Span::call_site());
  let desc_lit = syn::LitStr::new(&shard_desc.desc, proc_macro2::Span::call_site());

  let params_idents: Vec<_> = params.iter().map(|x| Ident::new(&x.var_name, proc_macro2::Span::call_site())).collect();
  let params_indices: Vec<_> = (0..params.len()).map(|x| LitInt::new(&format!("{}", x) , proc_macro2::Span::call_site())).collect();

  let crc = crc32(format!("{}-rust-0x20200101", shard_desc.name));

  let struct_name_upper = parsed.ident.to_string().to_uppercase();
  let params_static_id: Ident = Ident::new(&format!("{}_PARAMETERS", struct_name_upper), proc_macro2::Span::call_site());

  Ok(quote::quote! {
    fn registerName() -> &'static str {
      cstr!(#name_lit)
    }

    fn name(&mut self) -> &str {
      #name_lit
    }

    fn hash() -> u32
    where
      Self: Sized,
    {
      #crc
    }

    fn help(&mut self) -> OptionalString {
      OptionalString(shccstr!(#desc_lit))
    }

    fn parameters(&mut self) -> Option<&Parameters> {
        Some(&#params_static_id)
    }

    fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
      match index {
        #(
          #params_indices => self.#params_idents.set_param(value),
        )*
        _ => Err("Invalid parameter index"),
      }
    }

    fn getParam(&mut self, index: i32) -> shards::types::Var {
      match index {
        #(
          #params_indices => (&self.#params_idents).into(),
        )*
        _ => Var::default(),
      }
    }
  }.into())
}

fn parse_params<'a>(fields: impl IntoIterator<Item = &'a Field>) -> Result<Vec<Param>, Error> {
  let mut params: Vec<Param> = Vec::new();
  for fld in fields {
    let name: String = get_field_name(&fld);

    for attr in &fld.attrs {
      if attr.path().is_ident("Param") {
        match parse_param_field(&fld, &attr) {
          Ok(param) => {
            params.push(param);
          }
          Err(e) => return Err(format!("Failed to parse param for field {}\n{}", name, e).into()),
        }
      }
    }
  }
  Ok(params)
}

fn process_shard_impl(parsed: GenerateShardImplInput) -> Result<TokenStream, Error> {
  let struct_name = get_struct_name(&parsed.di);

  let params = if let syn::Data::Struct(sd) = &parsed.di.data {
    parse_params(&sd.fields)
  } else {
    Err(format!("Expected a struct for type {}", struct_name).into())
  }?;

  generate_shard_impl_functions(&parsed.di, parsed.shard_desc, params)
}

#[proc_macro]
pub fn generate_shard_impl(struct_def: TokenStream) -> TokenStream {
  let parsed: GenerateShardImplInput =
    syn::parse_macro_input!(struct_def as GenerateShardImplInput);

  match process_shard_impl(parsed) {
    Ok(result) => result,
    Err(err) => panic!("Failed generate shards implementation {}", err),
  }
}

fn process_shard_helper_impl(struct_: syn::ItemStruct) -> Result<TokenStream, Error> {
  let struct_id = struct_.ident;
  let struct_name_upper = struct_id.to_string().to_uppercase();

  let params = parse_params(&struct_.fields)?;
  let param_names: Vec<_> = params.iter().map(|x|LitStr::new(&x.name, proc_macro2::Span::call_site())).collect();
  let param_descs: Vec<_> = params.iter().map(|x|LitStr::new(&x.desc, proc_macro2::Span::call_site())).collect();
  let mut array_initializers = Vec::new();
  let param_types: Vec<_> = params.iter().map(|x| {
    if let Expr::Array(arr) = &x.types {
      let tmp_id: Ident = Ident::new(&format!("{}_{}_TYPES", struct_name_upper, x.name.to_uppercase()), proc_macro2::Span::call_site());
      array_initializers.push(quote!{ static ref #tmp_id: shards::types::Types = vec!#arr; });
      syn::parse_quote!{ #tmp_id }
    } else {
      x.types.clone()
    }
  }).collect();
  let param_idents: Vec<_> = params.iter().map(|x|Ident::new(&x.var_name, proc_macro2::Span::call_site())).collect();

  let params_static_id: Ident = Ident::new(&format!("{}_PARAMETERS", struct_name_upper), proc_macro2::Span::call_site());

  // Generate warmup/cleanup calls for supported types
  let mut warmups = Vec::new();
  let mut cleanups_rev = Vec::new();
  for x in &params {
      if let syn::Type::Path(p) = &x.rust_type {
        let param_id = Ident::new(&x.var_name, proc_macro2::Span::call_site());
        let last_type_id = &p.path.segments.last().expect("Empty path").ident;
        if last_type_id == "ParamVar" {
          warmups.push(quote!{self.#param_id.warmup(context);});
          cleanups_rev.push(quote!{self.#param_id.cleanup();});
        } else if last_type_id == "ShardsVar" {
          warmups.push(quote!{self.#param_id.warmup(context)?;});
          cleanups_rev.push(quote!{self.#param_id.cleanup();});
        }
      }
  }
  let cleanups = cleanups_rev.iter().rev();

  Ok({
    let ts = quote! {
      lazy_static! {
        #(#array_initializers)*
        static ref #params_static_id: Parameters = vec![
          #((
            cstr!(#param_names),
            shccstr!(#param_descs),
            &#param_types[..]
          ).into()),*
        ];
      }
      impl #struct_id {
        fn compose_helper(&mut self, data: &shards::types::InstanceData) -> std::result::Result<(), &'static str> {
          self.required.clear();
          #(
            shards::types::collect_required_variables(&data.shared, &mut self.required, (&self.#param_idents).into())?;
          )*
          Ok(())
        }

        fn warmup_helper(&mut self, context: &shards::types::Context) -> std::result::Result<(), &'static str> {
          #( #warmups )*
          Ok(())
        }


        fn cleanup_helper(&mut self) -> std::result::Result<(), &'static str> {
          #( #cleanups )*
          Ok(())
        }
      }
    }.into();
    ts
  })
}

#[proc_macro]
pub fn generate_shard_helper_impl(struct_def: TokenStream) -> TokenStream {
  let struct_: syn::ItemStruct = syn::parse_macro_input!(struct_def as syn::ItemStruct);

  match process_shard_helper_impl(struct_) {
    Ok(result) => result,
    Err(err) => panic!("Failed generate shards helper implementation {}", err),
  }
}
