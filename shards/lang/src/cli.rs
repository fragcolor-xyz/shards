use crate::{eval::eval, read::read};
use clap::{arg, Arg, Command};
use shards::core::Core;
use shards::types::Mesh;
use shards::{SHCore, SHARDS_CURRENT_ABI};
use std::collections::HashMap;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::path::Path;

extern "C" {
  fn shardsInterface(version: u32) -> *mut SHCore;
}

#[no_mangle]
pub extern "C" fn shards_process_args(argc: i32, argv: *const *const c_char) -> i32 {
  let args: Vec<String> = unsafe {
    std::slice::from_raw_parts(argv, argc as usize)
      .iter()
      .map(|&arg| {
        let c_str = CStr::from_ptr(arg);
        c_str.to_str().unwrap().to_owned()
      })
      .collect()
  };

  let matches = Command::new("Shards")
    .version("0.1")
    .about("Shards command line tools and executor.")
    .author("Fragcolor Team")
    .allow_external_subcommands(true)
    .subcommand(
      Command::new("build")
        .about("Reads a Shards file and builds a binary version of it, ready to be executed."),
    )
    .subcommand(
      Command::new("new")
        .about("Reads and executes a Shards file.")
        .arg(arg!(<FILE> "The script to execute"))
        .arg(Arg::new("args").num_args(0..)),
    )
    // Add your arguments here
    .get_matches_from(args);

  match matches.subcommand() {
    Some(("new", matches)) => {
      // we need to do this here or old path will fail
      unsafe {
        shards::core::Core = shardsInterface(SHARDS_CURRENT_ABI as u32);
      }

      let mut defines = HashMap::new();
      let args = matches.get_many::<String>("args");
      if let Some(args) = args {
        for arg in args {
          println!("arg: {}", arg);
          // find the first column and split it, the rest is the value
          let mut split = arg.split(':');
          let key = split.next().unwrap();
          // value should be all the rest, could contain ':' even
          let value = split.collect::<Vec<&str>>().join(":");
          // finally unescape the value if needed
          let value = value.replace("\\:", ":");
          // and remove quotes if quoted
          let value = value.trim_matches('"');
          defines.insert(key.to_owned(), value.to_owned());
        }
      }

      let file = matches
        .get_one::<String>("FILE")
        .expect("A file to execute");
      println!("Executing file: {}", file);

      let wire = {
        let ast = {
          let file_path = Path::new(&file);
          let file_path = std::fs::canonicalize(file_path).unwrap();
          let mut file_content = std::fs::read_to_string(file).expect("File not found");
          // add new line at the end of the file to be able to parse it correctly
          file_content.push('\n');

          // get absolute parent path of the file
          let parent_path = file_path.parent().unwrap().to_str().unwrap();
          let c_parent_path = std::ffi::CString::new(parent_path).unwrap();
          // set it as root path
          unsafe { (*Core).setRootPath.unwrap()(c_parent_path.as_ptr() as *const c_char) };

          read(&file_content, parent_path).expect("Failed to parse file")
        };
        eval(&ast, "root", defines).expect("Failed to evaluate file")
      };

      let mut mesh = Mesh::default();
      mesh.schedule(wire.0);
      loop {
        if !mesh.tick() {
          break;
        }
      }
      let info = wire.get_info();
      if info.failed {
        let msg = std::str::from_utf8(unsafe {
          std::slice::from_raw_parts(
            info.failureMessage.string as *const u8,
            info.failureMessage.len,
          )
        })
        .unwrap();
        println!("Failed: {}", msg);
        -1
      } else {
        0
      }
    }
    Some((_external, _matches)) => 99,
    _ => 0,
  }
}
