use std::env;
use std::fs;
use std::io::Write;
use std::path::PathBuf;

struct Options {
    in_path: PathBuf,
    out_path: PathBuf,
    var_name: String,
}

fn parse_args() -> Result<Options, String> {
    let args: Vec<String> = env::args().collect();

    if args.len() < 3 {
        return Err(format!(
            "Syntax: {} -in <inputfile> -out <outputfile> -varName <symbolname>",
            args[0]
        ));
    }

    let mut in_path = None;
    let mut out_path = None;
    let mut var_name = None;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "-in" => {
                i += 1;
                if i >= args.len() {
                    return Err("option -in requires another argument".to_string());
                }
                in_path = Some(PathBuf::from(&args[i]));
            }
            "-out" => {
                i += 1;
                if i >= args.len() {
                    return Err("option -out requires another argument".to_string());
                }
                out_path = Some(PathBuf::from(&args[i]));
            }
            "-varName" => {
                i += 1;
                if i >= args.len() {
                    return Err("option -varName requires another argument".to_string());
                }
                var_name = Some(args[i].clone());
            }
            arg => {
                return Err(format!("Unknown argument: '{}'", arg));
            }
        }
        i += 1;
    }

    Ok(Options {
        in_path: in_path.ok_or("No input file specified")?,
        out_path: out_path.ok_or("No output file specified")?,
        var_name: var_name.ok_or("No variable name specified")?,
    })
}

fn run_bin2c(options: &Options) -> Result<(), String> {
    // Read input file
    let mut data = fs::read(&options.in_path)
        .map_err(|e| format!("Failed to open input file {:?}: {}", options.in_path, e))?;

    let data_length = data.len();

    // Pad to 8-byte alignment
    let word_size = 8usize;
    let num_words = (data.len() + word_size - 1) / word_size;
    let aligned_length = num_words * word_size;
    data.resize(aligned_length, 0);

    // Create output directory if needed
    if let Some(parent) = options.out_path.parent() {
        fs::create_dir_all(parent)
            .map_err(|e| format!("Failed to create output directory: {}", e))?;
    }

    // Write .c file
    let c_path = format!("{}.c", options.out_path.display());
    let mut c_file = fs::File::create(&c_path)
        .map_err(|e| format!("Failed to create C file {}: {}", c_path, e))?;

    writeln!(c_file, "#include <stdint.h>").unwrap();
    write!(
        c_file,
        "static struct File {{ uint64_t length; uint64_t flags; uint64_t data[{}]; }} _{} = ",
        num_words, options.var_name
    ).unwrap();
    write!(c_file, "{{ {}, {}, {{", data_length, 0u64).unwrap(); // flags = 0

    // Write data as uint64_t values
    for (i, chunk) in data.chunks(word_size).enumerate() {
        let mut bytes = [0u8; 8];
        bytes[..chunk.len()].copy_from_slice(chunk);
        let value = u64::from_le_bytes(bytes);

        if i % 12 == 0 {
            write!(c_file, "\n0x{:x},", value).unwrap();
        } else {
            write!(c_file, "0x{:x},", value).unwrap();
        }
    }

    writeln!(c_file, "}} }};").unwrap();
    writeln!(c_file, "void* {0} = &_{0};", options.var_name).unwrap();

    // Write .h file
    let mut h_file = fs::File::create(&options.out_path)
        .map_err(|e| format!("Failed to create header file {:?}: {}", options.out_path, e))?;

    writeln!(h_file, "#pragma once").unwrap();
    writeln!(h_file, "#include <stdint.h>").unwrap();
    writeln!(h_file, "#include <stddef.h>").unwrap();
    writeln!(h_file, "#ifdef __cplusplus").unwrap();
    writeln!(h_file, "extern \"C\" {{").unwrap();
    writeln!(h_file, "#endif").unwrap();
    writeln!(h_file, "extern void* {};", options.var_name).unwrap();
    writeln!(h_file, "#ifdef __cplusplus").unwrap();
    writeln!(h_file, "}}").unwrap();
    writeln!(h_file, "#endif").unwrap();

    // Helper functions - offsets match the C++ bin2c::File struct
    // struct File { uint64_t length; uint64_t flags; uint8_t data[1]; }
    // length is at offset 0, data is at offset 16 (after length and flags)
    writeln!(
        h_file,
        "inline size_t {0}_getLength(){{ size_t* sizePtr = (size_t*)((uint8_t*){0} + {1}); return *sizePtr; }}",
        options.var_name, 0 // offset of length
    ).unwrap();
    writeln!(
        h_file,
        "inline const uint8_t* {0}_getData(){{ const uint8_t* dataPtr = ((uint8_t*){0} + {1}); return dataPtr; }}",
        options.var_name, 16 // offset of data (after length and flags, each 8 bytes)
    ).unwrap();

    Ok(())
}

fn main() {
    let options = match parse_args() {
        Ok(opts) => opts,
        Err(e) => {
            eprintln!("Error: {}", e);
            std::process::exit(1);
        }
    };

    if let Err(e) = run_bin2c(&options) {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
