//! Tiny compiler CLI.

use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{self, Command};
use tiny_ast::Program;
use tiny_codegen::emit_asm;
use tiny_interp::run;
use tiny_ir::{lower, optimize};
use tiny_parser::parse;
use tiny_sema::check;

fn main() {
    if let Err(e) = try_main() {
        eprintln!("{e}");
        process::exit(1);
    }
}

fn try_main() -> Result<(), String> {
    let mut args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() || args.iter().any(|a| a == "-h" || a == "--help") {
        print_help();
        return Ok(());
    }

    let cmd = args.remove(0);
    match cmd.as_str() {
        "run" => {
            let file = args.first().ok_or("usage: tiny run <file.tiny>")?;
            let source = read_file(file)?;
            let program = compile_front(&source)?;
            run(&program, &source, &mut std::io::stdout()).map_err(|e| e.formatted)?;
            Ok(())
        }
        "build" => {
            let mut input: Option<String> = None;
            let mut output: Option<String> = None;
            let mut emit_ast = false;
            let mut emit_ir = false;
            let mut opt = false;
            let mut i = 0;
            while i < args.len() {
                match args[i].as_str() {
                    "-o" => {
                        i += 1;
                        output = args.get(i).cloned();
                    }
                    "--emit-ast" => emit_ast = true,
                    "--emit-ir" => emit_ir = true,
                    "--opt" => opt = true,
                    s if !s.starts_with('-') => input = Some(s.to_string()),
                    other => return Err(format!("unknown option: {other}")),
                }
                i += 1;
            }
            let file = input.ok_or("usage: tiny build <file.tiny> [-o out] [--emit-ast] [--emit-ir] [--opt]")?;
            let source = read_file(&file)?;
            let program = compile_front(&source)?;

            if emit_ast {
                println!("{program:#?}");
                return Ok(());
            }

            let mut module = lower(&program);
            if opt {
                optimize(&mut module);
            }

            if emit_ir {
                print!("{}", module.display());
                return Ok(());
            }

            let asm = emit_asm(&module);
            let out_path = output.unwrap_or_else(|| default_output_name(&file));
            assemble_and_link(&asm, &out_path)?;
            Ok(())
        }
        other => Err(format!(
            "unknown command `{other}`\n\n{}",
            HELP.trim_start()
        )),
    }
}

fn compile_front(source: &str) -> Result<Program, String> {
    let program = parse(source).map_err(|e| e.formatted)?;
    check(&program, source).map_err(|e| e.to_string())?;
    Ok(program)
}

fn read_file(path: &str) -> Result<String, String> {
    fs::read_to_string(path).map_err(|e| format!("cannot read {path}: {e}"))
}

fn default_output_name(input: &str) -> String {
    Path::new(input)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("a.out")
        .to_string()
}

fn assemble_and_link(asm: &str, output: &str) -> Result<(), String> {
    let asm_path = PathBuf::from(format!("{output}.s"));
    fs::write(&asm_path, asm).map_err(|e| format!("write asm: {e}"))?;

    let status = Command::new("cc")
        .args([
            asm_path.to_str().unwrap_or("out.s"),
            "-o",
            output,
            "-no-pie",
        ])
        .status()
        .or_else(|_| {
            Command::new("gcc")
                .args([
                    asm_path.to_str().unwrap_or("out.s"),
                    "-o",
                    output,
                    "-no-pie",
                ])
                .status()
        })
        .map_err(|e| {
            format!(
                "failed to invoke cc/gcc (need a C toolchain for linking): {e}"
            )
        })?;

    if !status.success() {
        return Err(format!(
            "assembler/linker failed with status {status} (see {})",
            asm_path.display()
        ));
    }
    Ok(())
}

const HELP: &str = r#"
Tiny compiler — interpret or compile .tiny programs

USAGE:
  tiny run <file.tiny>
  tiny build <file.tiny> [-o <output>] [--opt] [--emit-ast] [--emit-ir]
  tiny --help

COMMANDS:
  run      Lex, parse, type-check, and interpret
  build    Compile to a native executable (Linux x86-64 via cc/gcc)

OPTIONS:
  -o <path>    Output binary path (default: input stem)
  --opt        Enable IR constant folding and DCE
  --emit-ast   Print the AST and exit
  --emit-ir    Print three-address IR and exit
"#;

fn print_help() {
    print!("{HELP}");
}
