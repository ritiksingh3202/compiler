//! Golden tests: interpreter vs examples/*.expected (and native on Linux).

use std::fs;
use std::path::PathBuf;
use tiny_interp::run_to_string;
use tiny_parser::parse;
use tiny_sema::check;

fn examples_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../examples")
}

fn normalize(s: &str) -> String {
    s.replace("\r\n", "\n").trim().to_string()
}

fn interpret_file(path: &std::path::Path) -> String {
    let src = fs::read_to_string(path).expect("read tiny");
    let program = parse(&src).expect("parse");
    check(&program, &src).expect("sema");
    run_to_string(&program, &src).expect("interp")
}

#[test]
fn golden_interpreter_matches_expected() {
    let dir = examples_dir();
    let mut count = 0;
    for entry in fs::read_dir(&dir).expect("examples dir") {
        let entry = entry.expect("entry");
        let path = entry.path();
        if path.extension().and_then(|e| e.to_str()) != Some("tiny") {
            continue;
        }
        let expected_path = path.with_extension("expected");
        if !expected_path.exists() {
            continue;
        }
        let expected = fs::read_to_string(&expected_path).expect("expected");
        let got = interpret_file(&path);
        assert_eq!(
            normalize(&got),
            normalize(&expected),
            "interpreter mismatch for {}",
            path.display()
        );
        count += 1;
    }
    assert!(count >= 3, "expected at least 3 golden examples, got {count}");
}

#[cfg(target_os = "linux")]
mod native {
    use super::*;
    use std::process::Command;

    #[test]
    fn golden_native_matches_interpreter() {
        let dir = examples_dir();
        let tiny = env!("CARGO_BIN_EXE_tiny");
        for entry in fs::read_dir(&dir).expect("examples") {
            let entry = entry.expect("entry");
            let path = entry.path();
            if path.extension().and_then(|e| e.to_str()) != Some("tiny") {
                continue;
            }
            if !path.with_extension("expected").exists() {
                continue;
            }

            let interp = interpret_file(&path);
            let stem = path
                .file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("out");
            let out_bin = std::env::temp_dir().join(format!("tiny_golden_{stem}"));
            let path_s = path.to_str().expect("utf8 path");
            let out_s = out_bin.to_str().expect("utf8 out");

            let status = Command::new(tiny)
                .args(["build", path_s, "-o", out_s])
                .status()
                .expect("spawn tiny build");
            assert!(status.success(), "build failed for {path_s}");

            let native = Command::new(&out_bin).output().expect("run native");
            assert!(
                native.status.success(),
                "native failed: {}",
                String::from_utf8_lossy(&native.stderr)
            );
            assert_eq!(
                normalize(&String::from_utf8_lossy(&native.stdout)),
                normalize(&interp),
                "native vs interp for {path_s}"
            );
            let _ = fs::remove_file(&out_bin);
            let _ = fs::remove_file(format!("{out_s}.s"));
        }
    }

    #[test]
    fn golden_native_with_opt_matches() {
        let path = examples_dir().join("fib.tiny");
        let tiny = env!("CARGO_BIN_EXE_tiny");
        let interp = interpret_file(&path);
        let out_bin = std::env::temp_dir().join("tiny_golden_fib_opt");
        let status = Command::new(tiny)
            .args([
                "build",
                path.to_str().unwrap(),
                "-o",
                out_bin.to_str().unwrap(),
                "--opt",
            ])
            .status()
            .expect("build");
        assert!(status.success());
        let native = Command::new(&out_bin).output().expect("run");
        assert_eq!(
            normalize(&String::from_utf8_lossy(&native.stdout)),
            normalize(&interp)
        );
    }
}
