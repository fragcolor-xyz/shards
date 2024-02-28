use std::ffi::c_void;

use dlopen::symbor::{Library, PtrOrNull};
use libffi::middle::arg as ffi_arg;
use libffi::middle::Type as FFIType;
use libffi::middle::*;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::SeqVar;
use shards::types::{common_type, ClonedVar, ANYS_TYPES, ANY_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

/*
ffi_type_uint8 -> u8
ffi_type_sint8 -> i8
ffi_type_uint16 -> u16
ffi_type_sint16 -> i16
ffi_type_uint32 -> u32
ffi_type_sint32 -> i32
ffi_type_uint64 -> u64
ffi_type_sint64 -> i64
ffi_type_float -> f32
ffi_type_double -> f64
ffi_type_pointer -> ptr
ffi_type_void -> void
*/

const FFI_TYPES: [&str; 12] = [
  "u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f32", "f64", "ptr", "void",
];

#[derive(shards::shard)]
#[shard_info("FFI.Invoke", "Invokes a FFI function.")]
struct FFIInvokeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Library", "The path to the library to load, can be None, to load self.", [common_type::none, common_type::string])]
  library: ClonedVar,

  #[shard_param("Symbol", "The symbol to load or a function pointer (Int).", [common_type::string, common_type::int])]
  symbol: ClonedVar,

  #[shard_param("Arguments", "The description of argument types, e.g. \"i32 f32 b i8[256]\".", [common_type::string])]
  arguments: ClonedVar,

  #[shard_param("Return", "The description of return type, e.g. \"int\", if None, void is assumed.", [common_type::string])]
  return_type: ClonedVar,

  actual_library: Option<Library>,
  actual_symbol: Option<*mut u8>,
  actual_return: Option<usize>,
  cif: Option<Cif>,
  args_hint: Vec<usize>,
  args_storage: Vec<u64>, // so far we need max 8 bytes
  args: Vec<Arg>,
}

impl Default for FFIInvokeShard {
  fn default() -> Self {
    let empty = Var::ephemeral_string("");
    let void_ret = Var::ephemeral_string("void");
    Self {
      required: ExposedTypes::new(),
      library: None.into(),
      symbol: empty.into(),
      arguments: empty.into(),
      return_type: void_ret.into(),
      actual_library: None,
      actual_symbol: None,
      actual_return: None,
      cif: None,
      args_hint: Vec::new(),
      args_storage: Vec::new(),
      args: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for FFIInvokeShard {
  fn input_types(&mut self) -> &Types {
    &ANYS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    let lib = if self.library.0.is_none() {
      Library::open_self()
    } else {
      let name: &str = (&self.library.0).try_into()?;
      Library::open(name)
    }
    .map_err(|e| {
      shlog_error!("Failed to open library: {}", e);
      "Failed to open library."
    })?;
    self.actual_library = Some(lib);

    let actual_symbol = if self.symbol.0.is_string() {
      let name: &str = (&self.symbol.0).try_into()?;
      let symbol = unsafe {
        self
          .actual_library
          .as_ref()
          .unwrap()
          .symbol::<PtrOrNull<u8>>(name) // we don't care about the type, we just want the pointer
      };
      let symbol = symbol.map_err(|e| {
        shlog_error!("Failed to load symbol: {}", e);
        "Failed to load symbol."
      })?;
      *(*symbol) as *mut u8
    } else {
      let addr: i64 = (&self.symbol.0).try_into()?;
      addr as *mut u8
    };
    self.actual_symbol = Some(actual_symbol);

    // parse self.arguments string into ffi_type using FFI_TYPES
    let args_desc: &str = (&self.arguments.0).try_into()?;
    // trim
    let args_desc = args_desc.trim();
    // split
    let args_desc = args_desc.split_whitespace();
    // map to ffi_type
    self.args_hint.clear();
    let args_desc = args_desc.map(|s| {
      let idx = FFI_TYPES.iter().position(|&r| r == s);
      self.args_hint.push(idx.unwrap_or(0)); // we don't care of default cos warmup will fail
      match idx {
        Some(0) => Ok(FFIType::u8()),
        Some(1) => Ok(FFIType::i8()),
        Some(2) => Ok(FFIType::u16()),
        Some(3) => Ok(FFIType::i16()),
        Some(4) => Ok(FFIType::u32()),
        Some(5) => Ok(FFIType::i32()),
        Some(6) => Ok(FFIType::u64()),
        Some(7) => Ok(FFIType::i64()),
        Some(8) => Ok(FFIType::f32()),
        Some(9) => Ok(FFIType::f64()),
        Some(10) => Ok(FFIType::pointer()),
        Some(11) => Ok(FFIType::void()),
        _ => Err("Invalid type."),
      }
    });

    let mut closure = Builder::new();
    for arg in args_desc {
      let arg = arg?;
      closure = closure.arg(arg);
    }

    // now parse self.return_type string into ffi_type using FFI_TYPES
    let return_desc: &str = (&self.return_type.0).try_into()?;
    let return_desc = return_desc.trim();
    let return_desc = match return_desc {
      "" => FFIType::void(),
      _ => {
        let idx = FFI_TYPES.iter().position(|&r| r == return_desc);
        self.actual_return = idx;
        match idx {
          Some(0) => FFIType::u8(),
          Some(1) => FFIType::i8(),
          Some(2) => FFIType::u16(),
          Some(3) => FFIType::i16(),
          Some(4) => FFIType::u32(),
          Some(5) => FFIType::i32(),
          Some(6) => FFIType::u64(),
          Some(7) => FFIType::i64(),
          Some(8) => FFIType::f32(),
          Some(9) => FFIType::f64(),
          Some(10) => FFIType::pointer(),
          Some(11) => FFIType::void(),
          _ => return Err("Invalid type."),
        }
      }
    };

    let cif = closure.res(return_desc).into_cif();
    self.cif = Some(cif);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let cif = self.cif.as_ref().unwrap(); // qed, warmup should have failed
    let symbol = self.actual_symbol.unwrap(); // qed, warmup should have failed
    let return_type = self.actual_return.unwrap(); // qed, warmup should have failed

    // build args from input
    self.args.clear();
    self.args_storage.clear();
    let args: SeqVar = input.try_into()?;
    if args.len() != self.args_hint.len() {
      return Err("Invalid number of arguments.");
    }
    // translate input args using hints
    for (i, arg) in args.iter().enumerate() {
      let hint = self.args_hint[i];
      match hint {
        0 | 1 => {
          let arg: i64 = arg.as_ref().try_into()?;
          let arg = arg as u8;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const u8,
              &mut storage as *mut u64 as *mut u8,
              1,
            );
          }
          self.args_storage.push(storage);
        }
        2 | 3 => {
          let arg: i64 = arg.as_ref().try_into()?;
          let arg = arg as u16;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const u16,
              &mut storage as *mut u64 as *mut u16,
              2,
            );
          }
          self.args_storage.push(storage);
        }
        4 | 5 => {
          let arg: i64 = arg.as_ref().try_into()?;
          let arg = arg as u32;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const u32,
              &mut storage as *mut u64 as *mut u32,
              4,
            );
          }
          self.args_storage.push(storage);
        }
        6 | 7 => {
          let arg: i64 = arg.as_ref().try_into()?;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const i64,
              &mut storage as *mut u64 as *mut i64,
              8,
            );
          }
          self.args_storage.push(storage);
        }
        8 => {
          let arg: f64 = arg.as_ref().try_into()?;
          let arg = arg as f32;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const f32,
              &mut storage as *mut u64 as *mut f32,
              4,
            );
          }
          self.args_storage.push(storage);
        }
        9 => {
          let arg: f64 = arg.as_ref().try_into()?;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const f64,
              &mut storage as *mut u64 as *mut f64,
              8,
            );
          }
          self.args_storage.push(storage);
        }
        10 => {
          let arg: i64 = arg.as_ref().try_into()?;
          let mut storage = 0u64;
          unsafe {
            std::ptr::copy_nonoverlapping(
              &arg as *const i64,
              &mut storage as *mut u64 as *mut i64,
              8,
            );
          }
          self.args_storage.push(storage);
        }
        _ => {
          return Err("Invalid argument type.");
        }
      };
      self.args.push(ffi_arg(&self.args_storage[i]))
    }

    if self.args_storage.len() != self.args_hint.len() {
      return Err("Inconsistent number of arguments.");
    }

    let output = match return_type {
      0 => {
        let res =
          unsafe { cif.call::<i8>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as i64;
        res.into()
      }
      1 => {
        let res =
          unsafe { cif.call::<u8>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as i64;
        res.into()
      }
      2 => {
        let res =
          unsafe { cif.call::<i16>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as i64;
        res.into()
      }
      3 => {
        let res =
          unsafe { cif.call::<u16>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as i64;
        res.into()
      }
      4 => {
        let res =
          unsafe { cif.call::<i32>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as i64;
        res.into()
      }
      5 => {
        let res =
          unsafe { cif.call::<u32>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as i64;
        res.into()
      }
      6 => {
        let res =
          unsafe { cif.call::<i64>(CodePtr::from_ptr(symbol as *const c_void), &self.args) };
        res.into()
      }
      7 => {
        let res =
          unsafe { cif.call::<u64>(CodePtr::from_ptr(symbol as *const c_void), &self.args) };
        res.into()
      }
      8 => {
        let res =
          unsafe { cif.call::<f32>(CodePtr::from_ptr(symbol as *const c_void), &self.args) } as f64;
        res.into()
      }
      9 => {
        let res =
          unsafe { cif.call::<f64>(CodePtr::from_ptr(symbol as *const c_void), &self.args) };
        res.into()
      }
      10 => {
        let res = unsafe {
          cif.call::<*mut c_void>(CodePtr::from_ptr(symbol as *const c_void), &self.args)
        };
        // reinterpret to i64
        let res = res as i64;
        res.into()
      }
      11 => {
        unsafe { cif.call::<()>(CodePtr::from_ptr(symbol as *const c_void), &self.args) };
        None.into()
      }
      _ => {
        unreachable!()
      }
    };
    Ok(output)
  }
}

pub fn register_shards() {
  register_shard::<FFIInvokeShard>();
}
