use ast_visitor::{process, Env};
use clap::Parser;
use std::fs;

mod ast_visitor;
mod error;
mod formatter;

#[derive(Debug, clap::Subcommand)]
enum Commands {
  /// Formats a shards file
  Format {
    /// Run the formatter on the file directly
    /// by default the output will go to stdout
    #[arg(long, short = 'i', action)]
    inline: bool,
    #[arg(long, short = 'o')]
    output: Option<String>,
    #[arg(last = true)]
    file: String,
  },
}

#[derive(Debug, clap::Parser)]
#[command(name = "shlang")]
#[command(about = "Shards language CLI")]
struct Cli {
  #[command(subcommand)]
  command: Commands,
}

fn main() {
  let args = Cli::parse();
  match args.command {
    Commands::Format {
      file,
      output,
      inline,
    } => {
      if output.is_some() && inline {
        panic!("Cannot use both -i and -o");
      }

      let mut out_stream: Box<dyn std::io::Write> = if let Some(out) = output {
        Box::new(fs::File::create(out).unwrap())
      } else if !inline {
        Box::new(std::io::stdout())
      } else {
        Box::new(fs::File::create(file.clone()).unwrap())
      };

      let in_str = if file == "-" {
        std::io::read_to_string(std::io::stdin()).unwrap()
      } else {
        fs::read_to_string(file).unwrap()
      };
      let mut v = formatter::FormatterVisitor::new(out_stream.as_mut(), &in_str);

      let mut env = Env::default();
      process(&in_str, &mut v, &mut env).unwrap();
    }
  }

  // let file = fs::File::create("out.shs").unwrap();

  // process(&str, &mut env).unwrap();
}
