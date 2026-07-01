use std::env;

use std::path::PathBuf;



fn repo_root() -> PathBuf {

    PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("../..")

}



fn mingw_lib_search() {

    if let Ok(path) = env::var("SIT_MINGW_LIB") {

        println!("cargo:rustc-link-search=native={path}");

    }

}



fn link_gcc_eh_runtime() {

    if let Ok(path) = env::var("SIT_MINGW_GCC_LIB") {

        let libgcc_eh = PathBuf::from(&path).join("libgcc_eh.a");

        if libgcc_eh.exists() {

            println!("cargo:rustc-link-arg={}", libgcc_eh.display());

        }

    }

}



fn link_static_windows_libs() {

    println!("cargo:rustc-link-arg=-Wl,--start-group");

    for lib in [

        "glfw3", "opengl32", "gdi32", "winmm", "user32", "shell32", "ole32", "iphlpapi",

        "setupapi", "dxgi", "propsys", "shlwapi", "uuid", "xinput", "ws2_32", "psapi",

        "kernel32", "advapi32", "m", "msvcrt", "mingwex", "gcc", "mingw32",

    ] {

        println!("cargo:rustc-link-arg=-l{lib}");

    }

    link_gcc_eh_runtime();

    println!("cargo:rustc-link-arg=-Wl,--end-group");

}



fn emit_static_situation_link(archive: &PathBuf) {

    println!("cargo:rustc-link-arg=-static-libgcc");

    println!("cargo:rustc-link-arg=-Wl,-Bstatic,--whole-archive");

    println!("cargo:rustc-link-arg=-lwinpthread");

    println!("cargo:rustc-link-arg=-Wl,--no-whole-archive");

    println!("cargo:rustc-link-arg={}", archive.display());

}



fn link_static_opengl(repo: &PathBuf) {

    let dll_dir = repo.join("build/dll");

    let glfw = repo.join("ext/glfw/build/src");

    let archive = dll_dir.join("situation_opengl.a");



    mingw_lib_search();

    println!("cargo:rustc-link-search=native={}", glfw.display());

    emit_static_situation_link(&archive);

    link_static_windows_libs();

}



fn link_static_vulkan(repo: &PathBuf) {

    let dll_dir = repo.join("build/dll");

    let glfw = repo.join("ext/glfw/build/src");

    let shaderc = repo.join("ext/shaderc/build/libshaderc");

    let archive = dll_dir.join("situation_vulkan.a");



    let vk_sdk = env::var("VULKAN_SDK").expect("VULKAN_SDK required for static-vulkan Rust builds");

    let vk_lib = PathBuf::from(&vk_sdk).join("Lib");



    mingw_lib_search();

    println!("cargo:rustc-link-search=native={}", glfw.display());

    println!("cargo:rustc-link-search=native={}", shaderc.display());

    println!("cargo:rustc-link-search=native={}", vk_lib.display());

    emit_static_situation_link(&archive);

    // Wrap everything in --start-group/--end-group so the linker can resolve
    // circular references between shaderc, vma_wrapper, libstdc++, and mingw CRT.
    println!("cargo:rustc-link-arg=-Wl,--start-group");

    for lib in [

        "glfw3", "vulkan-1", "shaderc_combined", "gdi32", "winmm", "user32", "shell32",

        "ole32", "iphlpapi", "setupapi", "dxgi", "propsys", "shlwapi", "uuid", "xinput",

        "ws2_32", "psapi", "kernel32", "advapi32",

    ] {

        println!("cargo:rustc-link-arg=-l{lib}");

    }

    // -static-libstdc++ is ignored under -nodefaultlibs (which Rust injects).
    // Instead inject libstdc++.a directly as an archive path, same as libgcc_eh.a.
    // libmingwex provides __mingw_fprintf, __sinl_internal, __cosl_internal, etc.
    // msvcrt provides sprintf_s/vsnprintf_s via dllimport.
    // mingw32 provides the MinGW CRT entry points.

    if let Ok(mingw_lib) = env::var("SIT_MINGW_LIB") {

        let stdcpp = PathBuf::from(&mingw_lib).join("libstdc++.a");

        if stdcpp.exists() {

            println!("cargo:rustc-link-arg={}", stdcpp.display());

        }

    }

    for lib in ["mingwex", "m", "msvcrt", "mingw32"] {

        println!("cargo:rustc-link-arg=-l{lib}");

    }

    link_gcc_eh_runtime();

    println!("cargo:rustc-link-arg=-Wl,--end-group");

}



fn main() {

    println!("cargo:rerun-if-env-changed=SITUATION_LINK");

    println!("cargo:rerun-if-env-changed=SIT_MINGW_LIB");

    println!("cargo:rerun-if-env-changed=SIT_MINGW_GCC_LIB");

    let repo = repo_root();

    let dll_dir = repo.join("build/dll");

    let link = env::var("SITUATION_LINK").unwrap_or_else(|_| "opengl".into());



    match link.as_str() {

        "opengl" => {

            // DLL-linked: the import library is situation_opengl.lib (MinGW dlltool format).
            // GCC won't find it via -lsituation_opengl (expects libsituation_opengl.dll.a),
            // so pass it as an explicit linker arg with -l: for exact-filename lookup.
            println!("cargo:rustc-link-search=native={}", dll_dir.display());

            println!("cargo:rustc-link-arg=-l:situation_opengl.lib");

        }

        "vulkan" => {

            println!("cargo:rustc-link-search=native={}", dll_dir.display());

            println!("cargo:rustc-link-arg=-l:situation_vulkan.lib");

        }

        "static-opengl" => link_static_opengl(&repo),

        "static-vulkan" => link_static_vulkan(&repo),

        other => panic!("Unknown SITUATION_LINK value: {other}"),

    }

}
