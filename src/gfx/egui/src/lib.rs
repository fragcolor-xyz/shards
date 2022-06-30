#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

mod renderer;

#[cfg(feature = "standalone_test")]
#[macro_use]
extern crate lazy_static;

#[cfg(feature = "standalone_test")]
mod color_test;

#[cfg(feature = "standalone_test")]
mod standalone_test;

pub use renderer::*;
