const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // -Dexample=<name>  — which example subfolder to build (default: hello_situation)
    const example_name = b.option([]const u8, "example", "Example to build") orelse "hello_situation";

    // -Dlink=<opengl|vulkan|static-opengl|static-vulkan>  — backend/link mode
    const link_mode = b.option([]const u8, "link", "Link mode: opengl, vulkan, static-opengl, static-vulkan") orelse "opengl";

    // -Dmingw_lib=<path>  — MinGW lib dir (for static builds)
    const mingw_lib = b.option([]const u8, "mingw_lib", "Path to MinGW lib directory");

    // -Dmingw_gcc_lib=<path>  — MinGW GCC lib dir (for static builds)
    const mingw_gcc_lib = b.option([]const u8, "mingw_gcc_lib", "Path to MinGW GCC lib directory");

    // -Dvk_sdk=<path>  — Vulkan SDK root (for static Vulkan builds)
    const vk_sdk = b.option([]const u8, "vk_sdk", "Path to Vulkan SDK root");

    const is_vulkan = std.mem.startsWith(u8, link_mode, "vulkan") or
                      std.mem.eql(u8, link_mode, "static-vulkan");
    const is_static = std.mem.startsWith(u8, link_mode, "static");

    const src_path = b.fmt("examples/{s}/main.zig", .{example_name});

    const situation_mod = b.createModule(.{
        .root_source_file = b.path("src/situation.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe_mod = b.createModule(.{
        .root_source_file = b.path(src_path),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{.{ .name = "situation", .module = situation_mod }},
    });

    exe_mod.addLibraryPath(b.path("../../build/dll"));

    if (is_static) {
        const lib_name = if (is_vulkan) "situation_vulkan" else "situation_opengl";
        exe_mod.linkSystemLibrary(lib_name, .{});
        if (mingw_lib) |ml| exe_mod.addLibraryPath(.{ .cwd_relative = ml });
        if (mingw_gcc_lib) |gl| exe_mod.addLibraryPath(.{ .cwd_relative = gl });
        exe_mod.linkSystemLibrary("gdi32", .{});
        exe_mod.linkSystemLibrary("winmm", .{});
        exe_mod.linkSystemLibrary("user32", .{});
        exe_mod.linkSystemLibrary("shell32", .{});
        exe_mod.linkSystemLibrary("ole32", .{});
        exe_mod.linkSystemLibrary("iphlpapi", .{});
        exe_mod.linkSystemLibrary("setupapi", .{});
        exe_mod.linkSystemLibrary("dxgi", .{});
        exe_mod.linkSystemLibrary("propsys", .{});
        exe_mod.linkSystemLibrary("shlwapi", .{});
        exe_mod.linkSystemLibrary("uuid", .{});
        exe_mod.linkSystemLibrary("xinput", .{});
        exe_mod.linkSystemLibrary("ws2_32", .{});
        exe_mod.linkSystemLibrary("psapi", .{});
        if (is_vulkan) {
            if (vk_sdk) |sdk| exe_mod.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/Lib", .{sdk}) });
            exe_mod.linkSystemLibrary("vulkan-1", .{});
            exe_mod.linkSystemLibrary("shaderc_combined", .{});
        } else {
            exe_mod.linkSystemLibrary("opengl32", .{});
        }
    } else {
        const lib_name = if (is_vulkan) "situation_vulkan" else "situation_opengl";
        exe_mod.linkSystemLibrary(lib_name, .{});
    }

    const exe = b.addExecutable(.{
        .name = example_name,
        .root_module = exe_mod,
    });
    b.installArtifact(exe);
}
