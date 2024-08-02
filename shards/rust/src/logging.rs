use crate::SHStringWithLen;
use std::os::raw::{c_char, c_int};

extern "C" {
  fn shards_log(
    level: c_int,
    msg: SHStringWithLen,
    file: *const c_char,
    function: *const c_char,
    line: c_int,
  );
}

#[inline(always)]
pub fn log(s: &str, file: &str, function: &str, line: u32) {
  unsafe {
    let msg = SHStringWithLen {
      string: s.as_ptr() as *const c_char,
      len: s.len() as u64,
    };
    shards_log(
      1,
      msg,
      file.as_ptr() as *const c_char,
      function.as_ptr() as *const c_char,
      line as c_int,
    );
  }
}

#[inline(always)]
pub fn log_level(level: i32, s: &str, file: &str, function: &str, line: u32) {
  unsafe {
    let msg = SHStringWithLen {
      string: s.as_ptr() as *const c_char,
      len: s.len() as u64,
    };
    shards_log(
      level,
      msg,
      file.as_ptr() as *const c_char,
      function.as_ptr() as *const c_char,
      line as c_int,
    );
  }
}

#[macro_export]
#[cfg(debug_assertions)]
macro_rules! shlog_trace {
    ($text:expr, $($arg:expr),*) => {
        {
            use std::io::Write as __stdWrite;
            let mut buf = vec![];
            ::std::write!(&mut buf, $text, $($arg),*).unwrap();
            $crate::logging::log_level(0, ::std::str::from_utf8(&buf).unwrap(),
                              concat!(file!(), "\0"),
                              concat!(module_path!(), "\0"),
                              line!());
        }
    };
    ($text:expr) => {
        $crate::logging::log_level(0, $text,
                          concat!(file!(), "\0"),
                          concat!(module_path!(), "\0"),
                          line!());
    };
}

#[macro_export]
#[cfg(not(debug_assertions))]
macro_rules! shlog_trace {
  ($text:expr, $($arg:expr),*) => {};
  ($text:expr) => {};
}

#[macro_export]
#[cfg(debug_assertions)]
macro_rules! shlog_debug {
    ($text:expr, $($arg:expr),*) => {
        {
            use std::io::Write as __stdWrite;
            let mut buf = vec![];
            ::std::write!(&mut buf, $text, $($arg),*).unwrap();
            $crate::logging::log_level(1, ::std::str::from_utf8(&buf).unwrap(),
                              concat!(file!(), "\0"),
                              concat!(module_path!(), "\0"),
                              line!());
        }
    };
    ($text:expr) => {
        $crate::logging::log_level(1, $text,
                          concat!(file!(), "\0"),
                          concat!(module_path!(), "\0"),
                          line!());
    };
}

#[macro_export]
#[cfg(not(debug_assertions))]
macro_rules! shlog_debug {
  ($text:expr, $($arg:expr),*) => {};
  ($text:expr) => {};
}

#[macro_export]
macro_rules! shlog {
    ($text:expr, $($arg:expr),*) => {
        {
            use std::io::Write as __stdWrite;
            let mut buf = vec![];
            ::std::write!(&mut buf, $text, $($arg),*).unwrap();
            $crate::logging::log(::std::str::from_utf8(&buf).unwrap(),
                        concat!(file!(), "\0"),
                        concat!(module_path!(), "\0"),
                        line!());
        }
    };
    ($text:expr) => {
        $crate::logging::log($text,
                    concat!(file!(), "\0"),
                    concat!(module_path!(), "\0"),
                    line!());
    };
}

#[macro_export]
macro_rules! shlog_warn {
    ($text:expr, $($arg:expr),*) => {
        {
            use std::io::Write as __stdWrite;
            let mut buf = vec![];
            ::std::write!(&mut buf, $text, $($arg),*).unwrap();
            $crate::logging::log_level(3, ::std::str::from_utf8(&buf).unwrap(),
                              concat!(file!(), "\0"),
                              concat!(module_path!(), "\0"),
                              line!());
        }
    };
    ($text:expr) => {
        $crate::logging::log_level(3, $text,
                          concat!(file!(), "\0"),
                          concat!(module_path!(), "\0"),
                          line!());
    };
}

#[macro_export]
macro_rules! shlog_error {
    ($text:expr, $($arg:expr),*) => {
        {
            use std::io::Write as __stdWrite;
            let mut buf = vec![];
            ::std::write!(&mut buf, $text, $($arg),*).unwrap();
            $crate::logging::log_level(4, ::std::str::from_utf8(&buf).unwrap(),
                              concat!(file!(), "\0"),
                              concat!(module_path!(), "\0"),
                              line!());
        }
    };
    ($text:expr) => {
        $crate::logging::log_level(4, $text,
                          concat!(file!(), "\0"),
                          concat!(module_path!(), "\0"),
                          line!());
    };
}
