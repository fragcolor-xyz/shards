use crate::ast::Sequence;
use crate::{eval::eval, read::read};
use clap::{arg, Arg, ArgMatches, Command};
use shards::core::Core;
use shards::types::Mesh;
use shards::{fourCharacterCode, shlog, SHCore, SHARDS_CURRENT_ABI};
use std::collections::HashMap;
use std::ffi::CStr;
use std::io::Write;
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
      Command::new("new")
        .about("Reads and executes a Shards file.")
        .arg(arg!(<FILE> "The script to execute"))
        .arg(
          Arg::new("decompress-strings")
            .long("decompress-strings")
            .help("Decompress help strings before running the script")
            .default_value("false"),
        )
        .arg(Arg::new("args").num_args(0..)),
    )
    .subcommand(
      Command::new("build")
        .about("Reads and executes a Shards file.")
        .arg(arg!(<FILE> "The script to execute"))
        .arg(
          Arg::new("output")
            .long("output")
            .short('o')
            .help("The output file to write to")
            .default_value("out.sho"),
        ),
    )
    .subcommand(
      Command::new("load")
        .about("Loads and executes a binary Shards file.")
        .arg(arg!(<FILE> "The binary Shards file to execute"))
        .arg(
          Arg::new("decompress-strings")
            .long("decompress-strings")
            .help("Decompress help strings before running the script")
            .default_value("false"),
        )
        .arg(Arg::new("args").num_args(0..)),
    )
    // Add your arguments here
    .get_matches_from(args);

  // we need to do this here or old path will fail
  unsafe {
    shards::core::Core = shardsInterface(SHARDS_CURRENT_ABI as u32);
  }

  match matches.subcommand() {
    Some(("new", matches)) => execute(matches),
    Some(("build", matches)) => build(matches),
    Some(("load", matches)) => load(matches),
    Some((_external, _matches)) => 99,
    _ => 0,
  }
}

fn load(matches: &ArgMatches) -> i32 {
  shlog!("Loading file");
  let file = matches
    .get_one::<String>("FILE")
    .expect("A binary Shards file to parse");
  shlog!("Parsing binary file: {}", file);

  let ast = {
    // deserialize from bincode, skipping the first 8 bytes
    let mut file_content = std::fs::read(file).expect("File not found");
    let magic: i32 = i32::from_be_bytes([
      file_content[0],
      file_content[1],
      file_content[2],
      file_content[3],
    ]);
    assert_eq!(magic, fourCharacterCode(*b"SHRD"));
    let version = u32::from_le_bytes([
      file_content[4],
      file_content[5],
      file_content[6],
      file_content[7],
    ]);
    assert_eq!(version, SHARDS_CURRENT_ABI); // todo backwards compatibility
    file_content.drain(0..8);
    bincode::deserialize(&file_content).unwrap()
  };

  execute_seq(matches, ast)
}

fn execute_seq(matches: &ArgMatches, ast: Sequence) -> i32 {
  let mut defines = HashMap::new();
  let args = matches.get_many::<String>("args");
  if let Some(args) = args {
    for arg in args {
      shlog!("arg: {}", arg);
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

  let wire = { eval(&ast, "root", defines).expect("Failed to evaluate file") };

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
    shlog!("Failed: {}", msg);
    -1
  } else {
    0
  }
}

fn build(matches: &ArgMatches) -> i32 {
  let file = matches.get_one::<String>("FILE").expect("A file to parse");
  shlog!("Parsing file: {}", file);

  let ast = {
    let file_path = Path::new(&file);
    let file_path = std::fs::canonicalize(file_path).unwrap();
    let mut file_content = std::fs::read_to_string(file).expect("File not found");
    // add new line at the end of the file to be able to parse it correctly
    file_content.push('\n');

    // get absolute parent path of the file
    let parent_path = file_path.parent().unwrap().to_str().unwrap();

    read(&file_content, parent_path).expect("Failed to parse file")
  };

  let output = matches
    .get_one::<String>("output")
    .expect("An output file to write to");

  // write sequence to file
  let mut file = std::fs::File::create(output).unwrap();
  let mut writer = std::io::BufWriter::new(&mut file);

  // Serialize using bincode
  let encoded_bin: Vec<u8> = bincode::serialize(&ast).unwrap();
  writer
    .write(fourCharacterCode(*b"SHRD").to_be_bytes().as_ref())
    .unwrap();
  writer
    .write(SHARDS_CURRENT_ABI.to_le_bytes().as_ref())
    .unwrap();
  writer.write_all(&encoded_bin).unwrap();

  0
}

fn execute(matches: &clap::ArgMatches) -> i32 {
  let decompress = matches.get_one::<String>("decompress-strings").unwrap();
  if decompress == "true" {
    shlog!("Decompressing strings");
    unsafe {
      (*Core).decompressStrings.unwrap()();
    }
  }

  let file = matches
    .get_one::<String>("FILE")
    .expect("A file to evaluate");
  shlog!("Evaluating file: {}", file);

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

  execute_seq(matches, ast)
}
