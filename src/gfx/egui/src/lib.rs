mod renderer;
pub use renderer::*;

#[cfg(feature = "standalone_test")]
#[macro_use]
extern crate lazy_static;

#[cfg(feature = "standalone_test")]
mod color_test;

#[cfg(feature = "standalone_test")]
mod standalone_test;
