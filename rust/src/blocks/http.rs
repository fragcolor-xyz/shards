use crate::block::Block;
use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Table;
use crate::types::Type;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use reqwest::header::{HeaderMap, HeaderName, HeaderValue, CONTENT_TYPE, USER_AGENT};
use std::convert::TryInto;
use std::ffi::CStr;

lazy_static! {
  static ref GET_INPUT_TYPES: Vec<Type> = vec![common_type::none, common_type::string_table];
  static ref STR_OUTPUT_TYPE: Vec<Type> = vec![common_type::string];
  static ref GET_PARAMETERS: Parameters = vec![
    (
      "URL",
      "The url to request to.",
      vec![common_type::string, common_type::string_var]
    )
      .into(),
    (
      "Headers",
      "The headers to use for the request.",
      vec![
        common_type::none,
        common_type::string_table,
        common_type::string_table_var
      ]
    )
      .into(),
    (
      "Timeout",
      "How many seconds to wait for the request to complete.",
      vec![common_type::int]
    )
      .into()
  ];
}

#[cfg(not(target_arch = "wasm32"))]
type Client = reqwest::blocking::Client;
#[cfg(target_arch = "wasm32")]
type Client = reqwest::Client;

#[cfg(not(target_arch = "wasm32"))]
fn new_client() -> Client {
  reqwest::blocking::Client::new()
}

#[cfg(target_arch = "wasm32")]
fn new_client() -> Client {
  reqwest::Client::new()
}

struct Get {
  client: Client,
  url: ParamVar,
  headers: ParamVar,
  headers_map: HeaderMap,
  output: ClonedVar,
  timeout: u64,
}

impl Default for Get {
  fn default() -> Self {
    Self {
      client: new_client(),
      url: ParamVar::new(cstr!("").into()),
      headers: ParamVar::new(().into()),
      headers_map: HeaderMap::new(),
      output: ().into(),
      timeout: 10,
    }
  }
}

impl Block for Get {
  fn registerName() -> &'static str {
    cstr!("Http.Get2")
  }
  fn name(&mut self) -> &str {
    "Http.Get2"
  }

  fn inputTypes(&mut self) -> &Types {
    &GET_INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &Types {
    &STR_OUTPUT_TYPE
  }
  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&GET_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.url.setParam(value),
      1 => self.headers.setParam(value),
      2 => self.timeout = value.try_into().expect("Integer timeout value"),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.url.getParam(),
      1 => self.headers.getParam(),
      2 => self.timeout.try_into().expect("A valid integer in range"),
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
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(do_blocking(context, || {
      let request = self.url.get();
      let request_string: &str = request.as_ref().try_into()?;
      let mut request = self.client.get(request_string);
      // request = request.timeout(Duration::from_secs(self.timeout));
      let headers = self.headers.get();
      if !headers.is_none() {
        let headers_table: Table = headers.as_ref().try_into()?;
        for (k, v) in headers_table.iter() {
          let key: &str = k.into();
          let hname: HeaderName = key
            .try_into()
            .map_err(|_| "Could not convert into HeaderName")?;
          let hvalue = HeaderValue::from_str(v.as_ref().try_into()?)
            .map_err(|_| "Could not convert into HeaderValue")?;
          request = request.header(hname, hvalue);
        }
      }
      if !input.is_none() {
        let input_table: Table = input.as_ref().try_into()?;
        for (k, v) in input_table.iter() {
          let key: &str = k.into();
          let value: &str = v.as_ref().try_into()?;
          request = request.query(&[(key, value)]);
        }
      }
      let response = exec_request_string(request)?;
      self.output = response.as_str().into();
      Ok(self.output.0)
    }))
  }
}

#[cfg(not(target_arch = "wasm32"))]
fn exec_request_string(
  request: reqwest::blocking::RequestBuilder,
) -> Result<std::string::String, &'static str> {
  request
    .send()
    .map_err(|e| {
      cblog!("Failure details: {}", e);
      "Failed to send the request"
    })?
    .text()
    .map_err(|e| {
      cblog!("Failure details: {}", e);
      "Failed to decode the response"
    })
}

#[cfg(target_arch = "wasm32")]
async fn exec_request_string_async(
  request: reqwest::RequestBuilder,
) -> Result<std::string::String, &'static str> {
  request
    .send()
    .await
    .map_err(|e| {
      cblog!("Failure details: {}", e);
      "Failed to send the request"
    })?
    .text()
    .await
    .map_err(|e| {
      cblog!("Failure details: {}", e);
      "Failed to decode the response"
    })
}

#[cfg(target_arch = "wasm32")]
fn exec_request_string(
  request: reqwest::RequestBuilder,
) -> Result<std::string::String, &'static str> {
  wasm_bindgen_futures::spawn_local(async {
    let _ = exec_request_string_async(request).await;
  });
  Err("Not yet implemented properly")
}

pub fn registerBlocks() {
  registerBlock::<Get>();
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
