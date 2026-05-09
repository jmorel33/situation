import re

with open('README.md', 'r') as f:
    readme = f.read()

new_roadmap_entries = """*   **Vulkan Test Harness (v2.4.42):** 🧹 **COMPLETE!** First pass at getting the Vulkan backend operational under the test harness. Fixes critical bugs in shader compilation, buffer updates, VD creation, VD compositing, pipeline vertex layout selection, and screenshot readback. Vulkan now passes ~55/78 graphics tests.
*   **Graphics Clean Sweep (v2.4.41):** 🎉 **COMPLETE!** Fix all remaining graphics test failures — graphics module now passes 81/81. Fixes span shader uniforms, textured quad rendering, compute pipeline binding, buffer updates, and GL state cleanup for re-initialization.
*   **Node Graph Takeover — Mixer Removed (v2.4.36):** 🛠️ **COMPLETE!** Removed the legacy miniaudio-based mixer API. The node graph system (`SituationAudioGraph` + `SituationProcessGraph`) is now the sole audio routing path. miniaudio remains as the audio device backend.
*   **Audio Node Graph — All 26 Devices Live (v2.4.35):** 🎉 **COMPLETE!** Completed the audio node graph system: all 26 device types are now registered, instantiable, and have live DSP processing. Nodes created via `SituationCreateNode` now properly initialize their device state and process audio through the graph.
*   **Test Harness Expansion (v2.4.33):** 🎉 **COMPLETE!** Added 86 new audio tests covering the full audio subsystem: device registry, node graph lifecycle & patching, control parameters, all 16 registered effects modules, graph serialization roundtrip, and MIDI integration & learn.
*   **Test Harness Complete (v2.4.28):** 🎉 **COMPLETE!** CTest-based unit test framework with headless execution, JSON reporters, memory leak detection, and 300+ assertions verifying API boundaries, GPU state, and audio concurrency.
*   **Modular Revolution & Architectural Reorganization (v2.4.0):** 🎉 **COMPLETE!** The monolithic `situation_impl.h` has been completely restructured into an aggregated header encompassing 16 independent modules (`sit/situation_impl_*.h`). This enables faster compilation and better code hygiene."""

# Insert the new entries before the first old entry
target_string = "*   **OpenGL Graveyard Flush Safety (v2.4.1):**"
if target_string in readme:
    readme = readme.replace(target_string, new_roadmap_entries + "\n" + target_string)

with open('README.md', 'w') as f:
    f.write(readme)
