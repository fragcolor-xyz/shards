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
  static ref POST_INPUT_TYPES: Vec<Type> = vec![
    common_type::none,
    common_type::string_table,
    common_type::bytes,
    common_type::string
  ];
  static ref STR_OUTPUT_TYPE: Vec<Type> = vec![common_type::string];
  static ref BYTES_OUTPUT_TYPE: Vec<Type> = vec![common_type::bytes];
  static ref GET_PARAMETERS: Parameters = vec![
    (
      cstr!("URL"),
      cstr!("The url to request to."),
      vec![common_type::string, common_type::string_var]
    )
      .into(),
    (
      cstr!("Headers"),
      cstr!("The headers to use for the request."),
      vec![
        common_type::none,
        common_type::string_table,
        common_type::string_table_var
      ]
    )
      .into(),
    (
      cstr!("Timeout"),
      cstr!("How many seconds to wait for the request to complete."),
      vec![common_type::int]
    )
      .into(),
    (
      cstr!("Bytes"),
      cstr!("If instead of a string the block should outout bytes."),
      vec![common_type::bool]
    )
      .into()
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
  output: ClonedVar,
  timeout: u64,
  as_bytes: bool,
}

impl Default for RequestBase {
  fn default() -> Self {
    Self {
      client: new_client(),
      url: ParamVar::new(cstr!("").into()),
      headers: ParamVar::new(().into()),
      output: ().into(),
      timeout: 10,
      as_bytes: false,
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
        if self.rb.as_bytes {
          &BYTES_OUTPUT_TYPE
        } else {
          &STR_OUTPUT_TYPE
        }
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&GET_PARAMETERS)
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        match index {
          0 => self.rb.url.setParam(value),
          1 => self.rb.headers.setParam(value),
          2 => self.rb.timeout = value.try_into().unwrap(),
          3 => self.rb.as_bytes = value.try_into().unwrap(),
          _ => unreachable!(),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.rb.url.getParam(),
          1 => self.rb.headers.getParam(),
          2 => self
            .rb
            .timeout
            .try_into()
            .expect("A valid integer in range"),
          3 => self.rb.as_bytes.into(),
          _ => unreachable!(),
        }
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.rb.url.warmup(context);
        self.rb.headers.warmup(context);
        Ok(())
      }

      fn cleanup(&mut self) {
        self.rb.url.cleanup();
        self.rb.headers.cleanup();
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
        Ok(do_blocking(context, || {
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
          if self.rb.as_bytes {
            let bytes = request
              .send()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to send the request"
              })?
              .bytes()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to decode the response"
              })?;
            self.rb.output = bytes.as_ref().into();
            Ok(self.rb.output.0)
          } else {
            let str = request
              .send()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to send the request"
              })?
              .text()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to decode the response"
              })?;
            self.rb.output = str.as_str().into();
            Ok(self.rb.output.0)
          }
        }))
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
        if self.rb.as_bytes {
          &BYTES_OUTPUT_TYPE
        } else {
          &STR_OUTPUT_TYPE
        }
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&GET_PARAMETERS)
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        match index {
          0 => self.rb.url.setParam(value),
          1 => self.rb.headers.setParam(value),
          2 => self.rb.timeout = value.try_into().unwrap(),
          3 => self.rb.as_bytes = value.try_into().unwrap(),
          _ => unreachable!(),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.rb.url.getParam(),
          1 => self.rb.headers.getParam(),
          2 => self
            .rb
            .timeout
            .try_into()
            .expect("A valid integer in range"),
          3 => self.rb.as_bytes.into(),
          _ => unreachable!(),
        }
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.rb.url.warmup(context);
        self.rb.headers.warmup(context);
        Ok(())
      }

      fn cleanup(&mut self) {
        self.rb.url.cleanup();
        self.rb.headers.cleanup();
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
        Ok(do_blocking(context, || {
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
          if self.rb.as_bytes {
            let bytes = request
              .send()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to send the request"
              })?
              .bytes()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to decode the response"
              })?;
            self.rb.output = bytes.as_ref().into();
            Ok(self.rb.output.0)
          } else {
            let str = request
              .send()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to send the request"
              })?
              .text()
              .map_err(|e| {
                cblog!("Failure details: {}", e);
                "Failed to decode the response"
              })?;
            self.rb.output = str.as_str().into();
            Ok(self.rb.output.0)
          }
        }))
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
