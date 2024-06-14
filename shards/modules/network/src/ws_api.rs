use crate::pollnet_context::PollnetContext;
use crate::pollnet_context::SocketHandle;
use crate::pollnet_context::SocketStatus;
use shards::SHStringWithLen;
use slotmap::Key;

use log::{error, info};
use std::ffi::CStr;
use std::os::raw::c_char;

const VERSION_STR: &str = concat!(env!("CARGO_PKG_VERSION"), "\0");

fn c_str_to_string(s: SHStringWithLen) -> &'static str {
  s.into()
}

#[no_mangle]
pub extern "C" fn pollnet_init() -> *mut PollnetContext {
  Box::into_raw(Box::new(PollnetContext::new()))
}

#[no_mangle]
pub extern "C" fn pollnet_handle_is_valid(handle: u64) -> bool {
  let handle: SocketHandle = handle.into();
  handle.is_null()
}

#[no_mangle]
pub extern "C" fn pollnet_invalid_handle() -> u64 {
  SocketHandle::null().into()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_shutdown(ctx: *mut PollnetContext) {
  info!("Requested ctx close!");
  let ctx = unsafe { &mut *ctx };
  ctx.shutdown();

  // take ownership and drop
  let b = unsafe { Box::from_raw(ctx) };
  drop(b);
  info!("Everything should be dead now!");
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_open_ws(ctx: *mut PollnetContext, url: SHStringWithLen) -> u64 {
  let ctx: &mut PollnetContext = unsafe { &mut *ctx };
  let url = c_str_to_string(url);
  ctx.open_ws(url).into()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_listen_ws(ctx: *mut PollnetContext, addr: SHStringWithLen) -> u64 {
  let ctx = unsafe { &mut *ctx };
  let addr = c_str_to_string(addr);
  ctx.listen_ws(addr).into()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_close(ctx: *mut PollnetContext, handle: u64) {
  let ctx = unsafe { &mut *ctx };
  ctx.close(handle.into())
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_close_all(ctx: *mut PollnetContext) {
  let ctx = unsafe { &mut *ctx };
  ctx.close_all()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_status(ctx: *mut PollnetContext, handle: u64) -> u32 {
  let ctx = unsafe { &*ctx };
  if let Some(socket) = ctx.sockets.get(handle.into()) {
    socket.status
  } else {
    SocketStatus::InvalidHandle
  }
  .into()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_send_binary(
  ctx: *mut PollnetContext,
  handle: u64,
  msg: *const u8,
  msgsize: u32,
) {
  let ctx = unsafe { &mut *ctx };
  let slice = unsafe { std::slice::from_raw_parts(msg, msgsize as usize) };
  ctx.send_binary(handle.into(), slice)
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_update(ctx: *mut PollnetContext, handle: u64) -> u32 {
  let ctx = unsafe { &mut *ctx };
  ctx.update(handle.into(), false).into()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_update_blocking(ctx: *mut PollnetContext, handle: u64) -> u32 {
  let ctx = unsafe { &mut *ctx };
  ctx.update(handle.into(), true).into()
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_get_data_size(ctx: *mut PollnetContext, handle: u64) -> u32 {
  let ctx = unsafe { &mut *ctx };
  let socket = match ctx.sockets.get(handle.into()) {
    Some(socket) => socket,
    None => return 0,
  };
  let msgsize = match &socket.data {
    Some(msg) => msg.len(),
    None => 0,
  };
  if msgsize > u32::MAX as usize {
    error!("Message size exceeds u32 max: {:}", msgsize);
    return 0;
  };
  msgsize as u32
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_get_data(
  ctx: *mut PollnetContext,
  handle: u64,
  dest: *mut u8,
  dest_size: u32,
) -> u32 {
  let ctx = unsafe { &mut *ctx };
  let socket = match ctx.sockets.get(handle.into()) {
    Some(socket) => socket,
    None => return 0,
  };

  let msgsize = match &socket.data {
    Some(msg) => msg.len(),
    None => 0,
  };

  if msgsize > u32::MAX as usize {
    error!("Message size exceeds u32 max: {:}", msgsize);
    return 0;
  }

  if msgsize > dest_size as usize {
    return msgsize as u32;
  }

  match &socket.data {
    Some(msg) => {
      let ncopy = msg.len().min(dest_size as usize);
      unsafe {
        std::ptr::copy_nonoverlapping(msg.as_ptr(), dest, ncopy);
      }
      ncopy as u32
    }
    None => 0,
  }
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_unsafe_get_data_ptr(
  ctx: *mut PollnetContext,
  handle: u64,
) -> *const u8 {
  let ctx = unsafe { &mut *ctx };
  let socket = match ctx.sockets.get_mut(handle.into()) {
    Some(socket) => socket,
    None => return std::ptr::null(),
  };
  match &socket.data {
    Some(msg) => msg.as_ptr(),
    None => std::ptr::null(),
  }
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_clear_data(ctx: *mut PollnetContext, handle: u64) {
  let ctx = unsafe { &mut *ctx };
  if let Some(socket) = ctx.sockets.get_mut(handle.into()) {
    socket.data.take();
  }
}

/// # Safety
///
/// ctx must be valid
#[no_mangle]
pub unsafe extern "C" fn pollnet_get_connected_client_handle(
  ctx: *mut PollnetContext,
  handle: u64,
) -> u64 {
  let ctx = unsafe { &mut *ctx };
  match ctx.sockets.get_mut(handle.into()) {
    Some(socket) => socket.last_client_handle,
    None => SocketHandle::null(),
  }
  .into()
}
