extern crate cbindgen;

use std::{env, path::PathBuf};
fn main() {
    let crate_dir = PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR env var is not defined"),
    );

    let package_name = env::var("CARGO_PKG_NAME").expect("CARGO_PKG_NAME env var is not defined");

    let build_dir = if let Ok(build_dir) = env::var("CBINDGEN_TARGET_DIR") {
        PathBuf::from(build_dir)
    } else {
        // get the PWD
        let pwd = crate_dir.clone();
        println!("cargo:warning=pwd: {}", pwd.display());
        pwd.join("target")
    };
    // println!("cargo:warning=build_dir: {}", build_dir.display());

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

        let target_dir = if let Ok(target_dir) = env::var("CBINDGEN_TARGET_DIR") {
            PathBuf::from(target_dir)
        } else {
            build_dir.clone().join("include").join(package_name.clone())
        };
        // println!("cargo:warning=target_dir: {}", target_dir.display());

        // ensure target_dir exists
        std::fs::create_dir_all(target_dir.clone()).expect("Failed to create target directory");
        writer.write_to_file(target_dir.join(format!("{}.h", package_name)));
    }
}
