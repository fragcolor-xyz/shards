/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use shards::core::register_legacy_shard;
use shards::core::run_blocking;
use shards::core::BlockingShard;
use shards::shard::LegacyShard;
use shards::types::common_type;

use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::RawString;
use shards::types::Table;
use shards::types::Type;
use shards::types::Types;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::INT_TYPES_SLICE;

use core::time::Duration;
use shards::types::Var;

use reqwest::blocking::Response;
use reqwest::header::{HeaderName, HeaderValue};
use std::convert::TryInto;

use std::collections::HashMap;

static URL_TYPES: &[Type] = &[common_type::string, common_type::string_var];
static HEADERS_TYPES: &[Type] = &[
  common_type::none,
  common_type::string_table,
  common_type::string_table_var,
];

lazy_static! {
  static ref GET_INPUT_TYPES: Vec<Type> = vec![common_type::none, common_type::string_table];
  static ref POST_INPUT_TYPES: Vec<Type> = vec![
    common_type::none,
    common_type::string_table,
    common_type::bytes,
    common_type::string
  ];
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
  static ref FULL_OUTPUT_KEYS: Vec<Var> = vec![
    shstr!("status").into(),
    shstr!("headers").into(),
    shstr!("body").into()
  ];
  static ref STR_FULL_OUTPUT_TTYPE: Type = Type::table(&FULL_OUTPUT_KEYS, &_STR_FULL_OUTPUT_TYPES);
  static ref BYTES_FULL_OUTPUT_TTYPE: Type =
    Type::table(&FULL_OUTPUT_KEYS, &_BYTES_FULL_OUTPUT_TYPES);
  static ref ALL_OUTPUT_TYPES: Vec<Type> = vec![
    *BYTES_FULL_OUTPUT_TTYPE,
    common_type::bytes,
    *STR_FULL_OUTPUT_TTYPE,
    common_type::string
  ];
  static ref GET_PARAMETERS: Parameters = vec![
    (cstr!("URL"), shccstr!("The url to request to."), URL_TYPES).into(),
    (
      cstr!("Headers"),
      shccstr!("The headers to use for the request."),
      HEADERS_TYPES
    )
      .into(),
    (
      cstr!("Timeout"),
      shccstr!("How many seconds to wait for the request to complete."),
      INT_TYPES_SLICE
    )
      .into(),
    (
      cstr!("Bytes"),
      shccstr!("If instead of a string the shard should output bytes."),
      BOOL_TYPES_SLICE
    )
      .into(),
    (
      cstr!("FullResponse"),
      shccstr!(
        "If the output should be a table with the full response, including headers and status."
      ),
      BOOL_TYPES_SLICE
    )
      .into(),
    (
      cstr!("AcceptInvalidCerts"),
      shccstr!(
        "If we should ignore invalid certificates. This is useful for testing but should not be used in production."
      ),
      BOOL_TYPES_SLICE
    )
      .into(),
    (
      cstr!("Retry"),
      shccstr!("How many times to retry the request if it fails."),
      INT_TYPES_SLICE
    )
      .into(),
    (
      cstr!("KeepAlive"),
      shccstr!("If the client instance should be kept alive, allowing connection reuse for multiple requests. The client won't be closed until this shard cleans up (including its worker thread)."),
      BOOL_TYPES_SLICE
    )
      .into(),
  ];
}

type Client = reqwest::blocking::Client;

struct RequestBase {
  client: Option<Client>,
  url: ParamVar,
  headers: ParamVar,
  output_table: Table,
  timeout: u64,
  retry: u64,
  as_bytes: bool,
  full_response: bool,
  invalid_certs: bool,
  keep_alive: bool,
  required: ExposedTypes,
}

impl Default for RequestBase {
  fn default() -> Self {
    let mut s = Self {
      client: None,
      url: ParamVar::new(Var::ephemeral_string("")),
      headers: ParamVar::new(().into()),
      output_table: Table::new(),
      timeout: 10,
      retry: 0,
      as_bytes: false,
      full_response: false,
      invalid_certs: false,
      keep_alive: false,
      required: Vec::new(),
    };
    let table = Table::new();
    s.output_table
      .insert_fast_static("headers", &table.as_ref().into());
    s
  }
}

impl RequestBase {
  fn _outputTypes(&mut self) -> &Types {
    &ALL_OUTPUT_TYPES
  }

  fn _parameters(&mut self) -> Option<&Parameters> {
    Some(&GET_PARAMETERS)
  }

  fn _setParam(&mut self, index: i32, value: &Var) -> Result<(), &'static str> {
    match index {
      0 => self.url.set_param(value),
      1 => self.headers.set_param(value),
      2 => Ok(self.timeout = value.try_into().map_err(|_x| "Failed to set timeout")?),
      3 => Ok(self.as_bytes = value.try_into().map_err(|_x| "Failed to set as_bytes")?),
      4 => Ok(
        self.full_response = value
          .try_into()
          .map_err(|_x| "Failed to set full_response")?,
      ),
      5 => Ok(
        self.invalid_certs = value
          .try_into()
          .map_err(|_x| "Failed to set invalid_certs")?,
      ),
      6 => Ok(self.retry = value.try_into().map_err(|_x| "Failed to set retry")?),
      7 => Ok(self.keep_alive = value.try_into().map_err(|_x| "Failed to set keep_alive")?),
      _ => unreachable!(),
    }
  }

  fn _getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.url.get_param(),
      1 => self.headers.get_param(),
      2 => self.timeout.try_into().expect("A valid integer in range"),
      3 => self.as_bytes.into(),
      4 => self.full_response.into(),
      5 => self.invalid_certs.into(),
      6 => self.retry.try_into().expect("A valid integer in range"),
      7 => self.keep_alive.into(),
      _ => unreachable!(),
    }
  }

  fn _requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.required.clear();

    if self.headers.is_variable() {
      let exp_info = ExposedInfo {
        exposedType: common_type::string_table,
        name: self.headers.get_name(),
        help: shccstr!("The headers associated with the request."),
        ..ExposedInfo::default()
      };
      self.required.push(exp_info);
    }
    if self.url.is_variable() {
      let exp_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.url.get_name(),
        help: shccstr!("The url to request to."),
        ..ExposedInfo::default()
      };
      self.required.push(exp_info);
    }

    Some(&self.required)
  }

  fn _open_client(&mut self) -> Result<(), &'static str> {
    if self.client.is_none() {
      self.client = Some(
        reqwest::blocking::Client::builder()
          .danger_accept_invalid_certs(self.invalid_certs)
          .timeout(Duration::from_secs(self.timeout))
          .build()
          .map_err(|e| {
            shlog!("Failure details: {}", e);
            "Failed to create client"
          })?,
      );
    }
    Ok(())
  }

  fn _close_client(&mut self) {
    self.client = None;
  }

  fn _warmup(&mut self, context: &Context) -> Result<(), &'static str> {
    if self.keep_alive {
      // open client early in this case
      self._open_client()?;
    }
    self.url.warmup(context);
    self.headers.warmup(context);
    Ok(())
  }

  fn _cleanup(&mut self, ctx: Option<&Context>) {
    self.url.cleanup(ctx);
    self.headers.cleanup(ctx);
    self._close_client();
  }

  fn _compose(&mut self, _data: &InstanceData) -> Result<Type, &'static str> {
    let output_type = if self.as_bytes {
      if self.full_response {
        *BYTES_FULL_OUTPUT_TTYPE
      } else {
        common_type::bytes
      }
    } else if self.full_response {
      *STR_FULL_OUTPUT_TTYPE
    } else {
      common_type::string
    };
    Ok(output_type)
  }

  fn _finalize(&mut self, response: Response) -> Result<Var, &'static str> {
    if self.full_response {
      self
        .output_table
        .insert_fast_static("status", &response.status().as_u16().try_into()?);

      let headers = self
        .output_table
        .get_mut_fast_static("headers")
        .as_mut_table()
        .unwrap(); // we know it's a table because we inserted at construction time
      for (key, value) in response.headers() {
        let key = Var::ephemeral_string(key.as_str());
        let value = Var::ephemeral_string(value.to_str().map_err(|e| {
          shlog!("Failure details: {}", e);
          "Failed to decode the response"
        })?);
        headers.insert_fast(key, &value);
      }
    }

    if self.as_bytes {
      let bytes = response.bytes().map_err(|e| {
        shlog!("Failure details: {}", e);
        "Failed to decode the response"
      })?;
      self
        .output_table
        .insert_fast_static("body", &bytes.as_ref().into());
    } else {
      let str = response.text().map_err(|e| {
        shlog!("Failure details: {}", e);
        "Failed to decode the response"
      })?;
      self
        .output_table
        .insert_fast_static("body", &Var::ephemeral_string(str.as_str()));
      // will clone
    }

    if self.full_response {
      Ok(self.output_table.as_ref().into())
    } else {
      Ok(*self.output_table.get_fast_static("body"))
    }
  }
}

macro_rules! get_like {
  ($shard_name:ident, $call:ident, $name_str:literal, $hash:literal) => {
    #[derive(Default)]
    struct $shard_name {
      rb: RequestBase,
      on_worker_thread: bool,
    }

    impl LegacyShard for $shard_name {
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

      fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
        self.rb._requiredVariables()
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &'static str> {
        self.rb._setParam(index, value)
      }

      fn getParam(&mut self, index: i32) -> Var {
        self.rb._getParam(index)
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &'static str> {
        self.rb._warmup(context)
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &'static str> {
        self.rb._cleanup(ctx);
        Ok(())
      }

      fn hasCompose() -> bool {
        true
      }

      fn compose(&mut self, data: &InstanceData) -> Result<Type, &'static str> {
        self.on_worker_thread = data.onWorkerThread;
        self.rb._compose(data)
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &'static str> {
        if !self.rb.keep_alive {
          self.rb._open_client()?;
        }
        let res = if self.on_worker_thread {
          self.activate_blocking(context, input)
        } else {
          Ok(run_blocking(self, context, input))
        };
        if !self.rb.keep_alive {
          self.rb._close_client();
        }
        res
      }
    }

    impl BlockingShard for $shard_name {
      fn activate_blocking(
        &mut self,
        _context: &Context,
        input: &Var,
      ) -> Result<Var, &'static str> {
        let request = self.rb.url.get();
        let request_string: &str = request.try_into()?;
        let mut request = self.rb.client.as_ref().unwrap().$call(request_string);
        let headers = self.rb.headers.get();
        if !headers.is_none() {
          let headers_table: Table = headers.try_into()?;
          for (k, v) in headers_table.iter() {
            let key: &str = k.as_ref().try_into()?;
            let hname: HeaderName = key
              .try_into()
              .map_err(|_| "Could not convert into HeaderName")?;
            let hvalue = HeaderValue::from_str(v.as_ref().try_into()?)
              .map_err(|_| "Could not convert into HeaderValue")?;
            request = request.header(hname, hvalue);
          }
        }

        if !input.is_none() {
          let input_table: Table = input.try_into()?;
          for (k, v) in input_table.iter() {
            let key: &str = k.as_ref().try_into()?;
            let value: &str = v.as_ref().try_into()?;
            request = request.query(&[(key, value)]);
          }
        }

        // already issue is if we want to retry we need to clone the request
        // if not we can just directly run it
        if self.rb.retry == 0 {
          let response = request.send().map_err(|e| {
            shlog_error!("Failure details: {}", e);
            "Failed to send the request"
          })?;

          if self.rb.full_response || response.status().is_success() {
            return self.rb._finalize(response);
          }

          shlog_error!("Request failed with status {}", response.status());
          shlog_error!("Request failed with body {}", response.text().unwrap());
          return Err("Request failed");
        } else {
          for tries in (0..self.rb.retry).rev() {
            let response = request.try_clone().unwrap().send();

            if let Ok(response) = response {
              if self.rb.full_response || response.status().is_success() {
                return self.rb._finalize(response);
              }

              shlog_error!("Request failed with status {}", response.status());
              shlog_error!("Request failed with body {}", response.text().unwrap());
              return Err("Request failed");
            }

            if tries == 0 {
              shlog_error!("Request failed with error: {:?}", response.err());
              return Err("Request failed");
            } else {
              shlog_debug!("Retrying request");
            }
          }
        }
        unreachable!()
      }
    }
  };
}

macro_rules! post_like {
  ($shard_name:ident, $call:ident, $name_str:literal, $hash:literal) => {
    #[derive(Default)]
    struct $shard_name {
      rb: RequestBase,
      on_worker_thread: bool,
    }

    impl LegacyShard for $shard_name {
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

      fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
        self.rb._requiredVariables()
      }

      fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &'static str> {
        self.rb._setParam(index, value)
      }

      fn getParam(&mut self, index: i32) -> Var {
        self.rb._getParam(index)
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &'static str> {
        self.rb._warmup(context)
      }

      fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &'static str> {
        self.rb._cleanup(ctx);
        Ok(())
      }

      fn hasCompose() -> bool {
        true
      }

      fn compose(&mut self, data: &InstanceData) -> Result<Type, &'static str> {
        self.on_worker_thread = data.onWorkerThread;
        self.rb._compose(data)
      }

      fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &'static str> {
        if !self.rb.keep_alive {
          self.rb._open_client()?;
        }
        let res = if self.on_worker_thread {
          self.activate_blocking(context, input)
        } else {
          Ok(run_blocking(self, context, input))
        };
        if !self.rb.keep_alive {
          self.rb._close_client();
        }
        res
      }
    }

    impl BlockingShard for $shard_name {
      fn activate_blocking(
        &mut self,
        _context: &Context,
        input: &Var,
      ) -> Result<Var, &'static str> {
        let request = self.rb.url.get();
        let request_string: &str = request.try_into()?;
        let mut request = self.rb.client.as_ref().unwrap().$call(request_string);
        let headers = self.rb.headers.get();
        let has_content_type = if !headers.is_none() {
          let headers_table = headers.as_table()?;
          let content_type_str = Var::ephemeral_string("content-type");
          let content_type = headers_table.get(content_type_str);
          content_type.is_some()
        } else {
          false
        };
        if !input.is_none() {
          // .form ( kv table )
          let input_table: Result<Table, &'static str> = input.try_into();
          if let Ok(input_table) = input_table {
            // default to this in this case but users can edit under
            if !has_content_type {
              request = request.header("content-type", "application/x-www-form-urlencoded");
            }

            let mut params = HashMap::new();
            for (k, v) in input_table.iter() {
              let key: &str = k.as_ref().try_into()?;
              let value: &str = v.as_ref().try_into()?;
              params.insert(key, value);
            }
            request = request.form(&params);
          } else {
            // .body ( string )
            let input_string: Result<&str, &'static str> = input.try_into();
            if let Ok(input_string) = input_string {
              // default to this in this case but users can edit under
              if !has_content_type {
                request = request.header("content-type", "application/json");
              }

              request = request.body(input_string);
            } else {
              // .body ( bytes )
              let input_bytes: Result<&[u8], &'static str> = input.try_into();
              if let Ok(input_bytes) = input_bytes {
                // default to this in this case but users can edit under
                if !has_content_type {
                  request = request.header("content-type", "application/octet-stream");
                }

                request = request.body(input_bytes);
              } else {
                return Err("Invalid input type");
              }
            }
          }
        }

        if !headers.is_none() {
          let headers_table: Table = headers.as_ref().try_into()?;
          for (k, v) in headers_table.iter() {
            let key: &str = k.as_ref().try_into()?;
            let hname: HeaderName = key
              .try_into()
              .map_err(|_| "Could not convert into HeaderName")?;
            let hvalue = HeaderValue::from_str(v.as_ref().try_into()?)
              .map_err(|_| "Could not convert into HeaderValue")?;
            request = request.header(hname, hvalue);
          }
        }

        if self.rb.retry == 0 {
          let response = request.send().map_err(|e| {
            shlog_error!("Failure details: {}", e);
            "Failed to send the request"
          })?;

          if self.rb.full_response || response.status().is_success() {
            return self.rb._finalize(response);
          }

          shlog_error!("Request failed with status {}", response.status());
          shlog_error!("Request failed with body {}", response.text().unwrap());
          return Err("Request failed");
        } else {
          for tries in (0..self.rb.retry).rev() {
            let response = request.try_clone().unwrap().send();

            if let Ok(response) = response {
              if self.rb.full_response || response.status().is_success() {
                return self.rb._finalize(response);
              }

              shlog_error!("Request failed with status {}", response.status());
              shlog_error!("Request failed with body {}", response.text().unwrap());
              return Err("Request failed");
            }

            if tries == 0 {
              shlog_error!("Request failed with error: {:?}", response.err());
              return Err("Request failed");
            } else {
              shlog_debug!("Retrying request");
            }
          }
        }
        unreachable!()
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

#[no_mangle]
pub extern "C" fn shardsRegister_http_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_legacy_shard::<Get>();
  register_legacy_shard::<Head>();
  register_legacy_shard::<Post>();
  register_legacy_shard::<Put>();
  register_legacy_shard::<Patch>();
  register_legacy_shard::<Delete>();
}
