use crate::block::Block;
use crate::core::do_blocking;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Table;
use crate::types::Type;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE, USER_AGENT};
use std::convert::TryInto;

lazy_static! {
  static ref GET_INPUT_TYPES: Vec<Type> = vec![common_type::none, common_type::string_table];
  static ref STR_OUTPUT_TYPE: Vec<Type> = vec![common_type::string];
  static ref PARAMETERS: Parameters = vec![
    (
      "URL",
      "The url to request to.",
      vec![common_type::string, common_type::string_var]
    )
      .into(),
    (
      "Headers",
      "The headers to use for the request.",
      vec![common_type::string_table, common_type::string_table_var]
    )
      .into()
  ];
}

struct Get {
  client: reqwest::blocking::Client,
  url: ParamVar,
  headers: ParamVar,
  headers_map: HeaderMap,
}

impl Default for Get {
  fn default() -> Self {
    Self {
      client: reqwest::blocking::Client::builder()
        .timeout(Duration::from_secs(30))
        .build()
        .unwrap(),
      url: ParamVar::new(cstr!("").into()),
      headers: ParamVar::new(cstr!("").into()),
      headers_map: HeaderMap::new(),
    }
  }
}

impl Block for Get {
  fn registerName() -> &'static str {
    cstr!("Http.Get")
  }
  fn name(&mut self) -> &str {
    "Http.Get"
  }

  fn inputTypes(&mut self) -> &Types {
    &GET_INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &Types {
    &STR_OUTPUT_TYPE
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.url.setParam(value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.url.getParam(),
      _ => unreachable!(),
    }
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.url.warmup(context);
    self.headers.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) {
    self.url.cleanup();
    self.headers.cleanup();
    self.headers_map.clear();
  }

  fn activate(&mut self, context: &Context, _: &Var) -> Result<Var, &str> {
    let request = self.url.get();
    let headers = self.headers.get();
    Ok(do_blocking(context, || {
      self.headers_map.clear();
      let headers_table: Table = headers.as_ref().try_into()?;
      // for (k, v) in headers_table.iter() {
      //   self.headers_map.insert(
      //     k,
      //     HeaderValue::from_str(v.as_ref().try_into()?)
      //       .or_else(|_| Err("Could not convert into HeaderValue"))?,
      //   );
      // }
      let request_string: &str = request.as_ref().try_into()?;
      let response = self.client.get(request_string);
      Ok(().into())
    }))
  }
}

#[cfg(test)]
mod tests {
  use std::collections::HashMap;
  #[test]
  fn it_works() {
    let resp = reqwest::blocking::get("https://httpbin.org/ip")
      .unwrap()
      .json::<HashMap<String, String>>()
      .unwrap();
    println!("{:#?}", resp);
  }
}
