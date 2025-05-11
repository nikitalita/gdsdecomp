extern crate cbindgen;

use std::{env, path::PathBuf};

fn main() {
    let crate_dir = PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR env var is not defined"),
    );

    let package_name = env::var("CARGO_PKG_NAME").expect("CARGO_PKG_NAME env var is not defined");

    // let build_dir = PathBuf::from(
    //     env::var("CARGO_TARGET_DIR").expect("CARGO_TARGET_DIR env var is not defined"),
    // );
    // // println!("cargo:warning=build_dir: {}", build_dir.display());


    // // print out the entire environment
    // for (key, value) in env::vars() {
    //     println!("cargo:warning={}={}", key, value);
    // }


    let config = cbindgen::Config::from_file("cbindgen.toml")
        .expect("Unable to find cbindgen.toml configuration file");

    if let Ok(writer) = cbindgen::generate_with_config(crate_dir.clone(), config) {
        // \note CMake sets this environment variable before invoking Cargo so
        //       that it can direct the generated header file into its
        //       out-of-source build directory for post-processing.

        let header_file = format!("{}.h", package_name);
        let header_path = if let Ok(target_dir) = env::var("CBINDGEN_TARGET_DIR") {
            std::fs::create_dir_all(target_dir.clone()).expect("Failed to create target directory");
            PathBuf::from(target_dir).join(header_file.clone())
        } else {
            PathBuf::from("include").join("vtracer").join(header_file.clone())
        };
        println!("cargo:warning=header_path: {}", header_path.display());

        // ensure target_dir exists
        writer.write_to_file(header_path);
    }
}
