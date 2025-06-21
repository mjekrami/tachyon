fn main() {
    // Tell cargo to look for the library in the C++ project's build directory
    // Adjust the path as needed!
    println!("cargo:rustc-link-search=native=../tachyon-tick/build");

    // Tell cargo to link against our `tachyon_lib` library
    println!("cargo:rustc-link-lib=dylib=tachyon_lib");
}
