use crate::block::Block;
use crate::core::activate_blocking;
use crate::core::do_blocking;
use crate::core::log;
use crate::core::registerBlock;
use crate::core::BlockingBlock;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::Table;
use crate::types::Type;
use crate::CString;
use crate::Types;
use crate::Var;
use core::time::Duration;
use reqwest::blocking::Request;
use reqwest::blocking::RequestBuilder;
use reqwest::blocking::Response;
use reqwest::header::{HeaderMap, HeaderName, HeaderValue, CONTENT_TYPE, USER_AGENT};
use std::convert::TryInto;
use std::ffi::CStr;

const FULL_OUTPUT_KEYS: &[RawString] = &[cbstr!("status"), cbstr!("headers"), cbstr!("body")];

lazy_static! {
  static ref GET_INPUT_TYPES: Vec<Type> = vec![common_type::none, common_type::string_table];
  static ref POST_INPUT_TYPES: Vec<Type> = vec![
    common_type::none,
    common_type::string_table,
    common_type::bytes,
    common_type::string
  ];
  static ref STR_OUTPUT_TYPE: Vec<Type> = vec![common_type::string];
  static ref BYTES_OUTPUT_TYPE: Vec<Type> = vec![common_type::bytes];
  static ref _STR_FULL_OUTPUT_TYPES: Vec<Type> = vec![
    common_type::int,
    common_type::string_table,
    common_type::string
  ];
  static ref _BYTES_FULL_OUTPUT_TYPES: Vec<Type> = vec![
    common_type::int,
    common_type::string_table,
    common_type::bytes
  ];
  static ref STR_FULL_OUTPUT_TTYPE: Type = Type::table(FULL_OUTPUT_KEYS, &_STR_FULL_OUTPUT_TYPES);
  static ref BYTES_FULL_OUTPUT_TTYPE: Type =
    Type::table(FULL_OUTPUT_KEYS, &_BYTES_FULL_OUTPUT_TYPES);
  static ref STR_FULL_OUTPUT_TYPE: Vec<Type> = vec![*STR_FULL_OUTPUT_TTYPE];
  static ref BYTES_FULL_OUTPUT_TYPE: Vec<Type> = vec![*BYTES_FULL_OUTPUT_TTYPE];
  static ref GET_PARAMETERS: Parameters = vec![
    (
      cstr!("URL"),
      cbccstr!("The url to request to."),
      vec![common_type::string, common_type::string_var]
    )
      .into(),
    (
      cstr!("Headers"),
      cbccstr!("The headers to use for the request."),
      vec![
        common_type::none,
        common_type::string_table,
        common_type::string_table_var
      ]
    )
      .into(),
    (
      cstr!("Timeout"),
      cbccstr!("How many seconds to wait for the request to complete."),
      vec![common_type::int]
    )
      .into(),
    (
      cstr!("Bytes"),
      cbccstr!("If instead of a string the block should outout bytes."),
      vec![common_type::bool]
    )
      .into(),
    (
      cstr!("FullResponse"),
      cbccstr!(
        "If the output should be a table with the full response, including headers and status."
      ),
      vec![common_type::bool]
    )
      .into(),
  ];
}

type Client = reqwest::blocking::Client;

fn new_client() -> Client {
  reqwest::blocking::Client::new()
}

struct RequestBase {
  client: Client,
  url: ParamVar,
  headers: ParamVar,
  output_table: Table,
  timeout: u64,
  as_bytes: bool,
  full_response: bool,
}

impl Default for RequestBase {
  fn default() -> Self {
    let mut s = Self {
      client: new_client(),
      url: ParamVar::new(cstr!("").into()),
      headers: ParamVar::new(().into()),
      output_table: Table::new(),
      timeout: 10,
      as_bytes: false,
      full_response: false,
    };
    let table = Table::new();
    s.output_table
      .insert_fast_static(cstr!("headers"), table.as_ref().into());
    s
  }
}

impl RequestBase {
  fn _outputTypes(&mut self) -> &Types {
    if self.as_bytes {
      if self.full_response {
        &BYTES_FULL_OUTPUT_TYPE
      } else {
        &BYTES_OUTPUT_TYPE
      }
    } else if self.full_response {
      &STR_FULL_OUTPUT_TYPE
    } else {
      &STR_OUTPUT_TYPE
    }
  }

  fn _parameters(&mut self) -> Option<&Parameters> {
    Some(&GET_PARAMETERS)
  }

  fn _setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.url.setParam(value),
      1 => self.headers.setParam(value),
      2 => self.timeout = value.try_into().unwrap(),
      3 => self.as_bytes = value.try_into().unwrap(),
      4 => self.full_response = value.try_into().unwrap(),
      _ => unreachable!(),
    }
  }

  fn _getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.url.getParam(),
      1 => self.headers.getParam(),
      2 => self.timeout.try_into().expect("A valid integer in range"),
      3 => self.as_bytes.into(),
      4 => self.full_response.into(),
      _ => unreachable!(),
    }
  }

  fn _warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.url.warmup(context);
    self.headers.warmup(context);
    Ok(())
  }

  fn _cleanup(&mut self) {
    self.url.cleanup();
    self.headers.cleanup();
  }

  fn _finalize(&mut self, response: Response) -> Result<Var, &str> {
    if self.full_response {
      self
        .output_table
        .insert_fast_static(cstr!("status"), response.status().as_u16().try_into()?);

      let headers = self.output_table.get_mut_fast_static(cstr!("headers"));
      let mut headers: Table = headers.as_ref().try_into()?;
      for (key, value) in response.headers() {
        let key = CString::new(key.as_str()).map_err(|e| {
          cblog!("Failure details: {}", e);
          "Failed to convert header string"
        })?;
        let value = CString::new(value.as_bytes()).map_err(|e| {
          cblog!("Failure details: {}", e);
          "Failed to convert header value"
        })?;
        headers.insert_fast(&key, value.as_ref().into());
      }
    }

    if self.as_bytes {
      let bytes = response.bytes().map_err(|e| {
        cblog!("Failure details: {}", e);
        "Failed to decode the response"
      })?;
      self
        .output_table
        .insert_fast_static(cstr!("body"), bytes.as_ref().into());
    } else {
      let str = response.text().map_err(|e| {
        cblog!("Failure details: {}", e);
        "Failed to decode the response"
      })?;
      self
        .output_table
        .insert_fast_static(cstr!("body"), str.as_str().into());
    }

    if self.full_response {
      Ok(self.output_table.as_ref().into())
    } else {
      Ok(*self.output_table.get_fast_static(cstr!("body")))
    }
  }
}

macro_rules! get_like {
  ($block_name:ident, $call:ident, $name_str:literal, $hash:literal) => {
    #[derive(Default)]
    struct $block_name {
      rb: RequestBase,
    }

    impl Block for $block_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn inputTypes(&mut self) -> &Types {
        &GET_INPUT_TYPES
      }

      fn outputTypes(&mut self) -> &Types {
        self.rb._outputTypes()
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        self.rb._parameters()
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        self.rb._setParam(index, value);
      }

      fn getParam(&mut self, index: i32) -> Var {
        self.rb._getParam(index)
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.rb._warmup(context)
      }

      fn cleanup(&mut self) {
        self.rb._cleanup();
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
        Ok(activate_blocking(self, context, input))
      }
    }

    impl BlockingBlock for $block_name {
      fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
        let request = self.rb.url.get();
        let request_string: &str = request.as_ref().try_into()?;
        let mut request = self.rb.client.$call(request_string);
        request = request.timeout(Duration::from_secs(self.rb.timeout));
        let headers = self.rb.headers.get();
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

        let response = request.send().map_err(|e| {
          cblog!("Failure details: {}", e);
          "Failed to send the request"
        })?;

        self.rb._finalize(response)
      }
    }
  };
}

macro_rules! post_like {
  ($block_name:ident, $call:ident, $name_str:literal, $hash:literal) => {
    #[derive(Default)]
    struct $block_name {
      rb: RequestBase,
    }

    impl Block for $block_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn inputTypes(&mut self) -> &Types {
        &POST_INPUT_TYPES
      }

      fn outputTypes(&mut self) -> &Types {
        self.rb._outputTypes()
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        self.rb._parameters()
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        self.rb._setParam(index, value);
      }

      fn getParam(&mut self, index: i32) -> Var {
        self.rb._getParam(index)
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.rb._warmup(context)
      }

      fn cleanup(&mut self) {
        self.rb._cleanup();
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
        Ok(activate_blocking(self, context, input))
      }
    }

    impl BlockingBlock for $block_name {
      fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
        let request = self.rb.url.get();
        let request_string: &str = request.as_ref().try_into()?;
        let mut request = self.rb.client.$call(request_string);
        request = request.timeout(Duration::from_secs(self.rb.timeout));
        let headers = self.rb.headers.get();
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
          // .form ( kv table )
          let input_table: Result<Table, &str> = input.as_ref().try_into();
          if let Ok(input_table) = input_table {
            for (k, v) in input_table.iter() {
              let key: &str = k.into();
              let value: &str = v.as_ref().try_into()?;
              request = request.form(&[(key, value)]);
            }
          } else {
            // .body ( string )
            let input_string: Result<&str, &str> = input.as_ref().try_into();
            if let Ok(input_string) = input_string {
              request = request.body(input_string);
            } else {
              // .body ( bytes )
              let input_bytes: Result<&[u8], &str> = input.as_ref().try_into();
              if let Ok(input_bytes) = input_bytes {
                request = request.body(input_bytes);
              }
            }
          }
        }

        let response = request.send().map_err(|e| {
          cblog!("Failure details: {}", e);
          "Failed to send the request"
        })?;

        self.rb._finalize(response)
      }
    }
  };
}

get_like!(Get, get, "Http.Get", "Http.Get-rust-0x20200101");
get_like!(Head, head, "Http.Head", "Http.Head-rust-0x20200101");
post_like!(Post, post, "Http.Post", "Http.Post-rust-0x20200101");
post_like!(Put, put, "Http.Put", "Http.Put-rust-0x20200101");
post_like!(Patch, patch, "Http.Patch", "Http.Patch-rust-0x20200101");
post_like!(Delete, delete, "Http.Delete", "Http.Delete-rust-0x20200101");

pub fn registerBlocks() {
  registerBlock::<Get>();
  registerBlock::<Head>();
  registerBlock::<Post>();
  registerBlock::<Put>();
  registerBlock::<Patch>();
  registerBlock::<Delete>();
}
