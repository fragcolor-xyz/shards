use crate::{eval::eval, read::read};
use clap::{arg, Command};
use shards::core::Core;
use shards::types::Mesh;
use shards::{SHCore, SHARDS_CURRENT_ABI};
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
        .arg(arg!(<FILE> "The script to execute")),
    )
    // Add your arguments here
    .get_matches_from(args);

  match matches.subcommand() {
    Some(("new", matches)) => {
      // we need to do this here or old path will fail
      unsafe {
        shards::core::Core = shardsInterface(SHARDS_CURRENT_ABI as u32);
      }

      let file = matches
        .get_one::<String>("FILE")
        .expect("A file to execute");
      println!("Executing file: {}", file);

      let wire = {
        let ast = {
          let file_path = Path::new(&file);
          let mut file_content = std::fs::read_to_string(file).expect("File not found");
          // add new line at the end of the file to be able to parse it correctly
          file_content.push('\n');

          // get absolute parent path of the file
          let mut parent_path = file_path.parent().unwrap().to_str().unwrap();
          if parent_path == "" {
            parent_path = ".";
          }
          let parent_path = std::fs::canonicalize(parent_path).unwrap();
          // set it as root path
          unsafe {
            (*Core).setRootPath.unwrap()(parent_path.to_str().unwrap().as_ptr() as *const c_char)
          };

          read(&file_content, file_path.parent().unwrap().to_str().unwrap())
            .expect("Failed to parse file")
        };
        eval(&ast, "root").expect("Failed to evaluate file")
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

// fn main() {
//   let matches = Command::new("pacman")
//     .about("package manager utility")
//     .version("5.2.1")
//     .subcommand_required(true)
//     .arg_required_else_help(true)
//     .author("Pacman Development Team")
//     // Query subcommand
//     //
//     // Only a few of its arguments are implemented below.
//     .subcommand(
//       Command::new("query")
//         .short_flag('Q')
//         .long_flag("query")
//         .about("Query the package database.")
//         .arg(
//           Arg::new("search")
//             .short('s')
//             .long("search")
//             .help("search locally installed packages for matching strings")
//             .conflicts_with("info")
//             .action(ArgAction::Set)
//             .num_args(1..),
//         )
//         .arg(
//           Arg::new("info")
//             .long("info")
//             .short('i')
//             .conflicts_with("search")
//             .help("view package information")
//             .action(ArgAction::Set)
//             .num_args(1..),
//         ),
//     )
//     // Sync subcommand
//     //
//     // Only a few of its arguments are implemented below.
//     .subcommand(
//       Command::new("sync")
//         .short_flag('S')
//         .long_flag("sync")
//         .about("Synchronize packages.")
//         .arg(
//           Arg::new("search")
//             .short('s')
//             .long("search")
//             .conflicts_with("info")
//             .action(ArgAction::Set)
//             .num_args(1..)
//             .help("search remote repositories for matching strings"),
//         )
//         .arg(
//           Arg::new("info")
//             .long("info")
//             .conflicts_with("search")
//             .short('i')
//             .action(ArgAction::SetTrue)
//             .help("view package information"),
//         )
//         .arg(
//           Arg::new("package")
//             .help("packages")
//             .required_unless_present("search")
//             .action(ArgAction::Set)
//             .num_args(1..),
//         ),
//     )
//     .get_matches();

//   match matches.subcommand() {
//     Some(("sync", sync_matches)) => {
//       if sync_matches.contains_id("search") {
//         let packages: Vec<_> = sync_matches
//           .get_many::<String>("search")
//           .expect("contains_id")
//           .map(|s| s.as_str())
//           .collect();
//         let values = packages.join(", ");
//         println!("Searching for {values}...");
//         return;
//       }

//       let packages: Vec<_> = sync_matches
//         .get_many::<String>("package")
//         .expect("is present")
//         .map(|s| s.as_str())
//         .collect();
//       let values = packages.join(", ");

//       if sync_matches.get_flag("info") {
//         println!("Retrieving info for {values}...");
//       } else {
//         println!("Installing {values}...");
//       }
//     }
//     Some(("query", query_matches)) => {
//       if let Some(packages) = query_matches.get_many::<String>("info") {
//         let comma_sep = packages.map(|s| s.as_str()).collect::<Vec<_>>().join(", ");
//         println!("Retrieving info for {comma_sep}...");
//       } else if let Some(queries) = query_matches.get_many::<String>("search") {
//         let comma_sep = queries.map(|s| s.as_str()).collect::<Vec<_>>().join(", ");
//         println!("Searching Locally for {comma_sep}...");
//       } else {
//         println!("Displaying all locally installed packages...");
//       }
//     }
//     _ => unreachable!(), // If all subcommands are defined above, anything else is unreachable
//   }
// }
