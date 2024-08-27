use std::io::{self, Write};

#[no_mangle]
pub extern "C" fn _initialize() {
  // Initialization code here
  let _ = writeln!(io::stderr(), "Reactor Initialized");
}

#[no_mangle]
pub extern "C" fn hello() -> i32 {
  let message = "Hello from WASI Reactor!\n";
  io::stdout()
    .write_all(message.as_bytes())
    .expect("Failed to write to stdout");
  0
}

#[no_mangle]
pub extern "C" fn goodbye() -> i32 {
  let message = "Goodbye from WASI Reactor!\n";
  io::stdout()
    .write_all(message.as_bytes())
    .expect("Failed to write to stdout");
  0
}
