fn main() {
    println!("cargo:rustc-link-search=native=../../build/dll");
    println!("cargo:rustc-link-lib=situation_opengl");
}
