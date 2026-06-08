const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const situation_mod = b.createModule(.{
        .root_source_file = b.path("src/situation.zig"),
        .target = target,
        .optimize = optimize,
    });

    // In Zig 0.17-dev, linkLibC / addLibraryPath / linkSystemLibrary moved to Module.
    const exe_mod = b.createModule(.{
        .root_source_file = b.path("examples/hello_situation/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{.{ .name = "situation", .module = situation_mod }},
    });
    exe_mod.addLibraryPath(b.path("../../build/dll"));
    exe_mod.linkSystemLibrary("situation_opengl", .{});

    const exe = b.addExecutable(.{
        .name = "hello_situation",
        .root_module = exe_mod,
    });
    b.installArtifact(exe);
}
