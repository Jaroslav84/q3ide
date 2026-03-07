use std::env;
use std::path::PathBuf;

fn main() {
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let out_dir = PathBuf::from(&crate_dir).join("..");

    // Link Swift runtime (needed by screencapturekit crate)
    let xcode_path = std::process::Command::new("xcode-select")
        .arg("-p")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string())
        .unwrap_or_else(|_| "/Applications/Xcode.app/Contents/Developer".to_string());

    let swift_lib = format!(
        "{}/Toolchains/XcodeDefault.xctoolchain/usr/lib/swift/macosx",
        xcode_path
    );
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", swift_lib);

    // Also try /usr/lib/swift for command line tools installs
    println!("cargo:rustc-link-arg=-Wl,-rpath,/usr/lib/swift");

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(cbindgen::Config::from_file("cbindgen.toml").unwrap())
        .generate()
        .expect("Unable to generate C bindings")
        .write_to_file(out_dir.join("q3ide_capture.h"));
}
