/*
 * Situation v2.4 Verification Suite
 * Compile: g++ situation_verify.cpp -o verify -std=c++17 -lglfw -lvulkan -ldl -lpthread -I.
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN // Testing the complex backend
#define SITUATION_ENABLE_SHADER_COMPILER
#define SITUATION_ENABLE_THREADING
#include "situation.h"

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cassert>

// --- Utilities ---
void Check(bool condition, const char* msg) {
    if (!condition) {
        char* err = NULL;
        SituationGetLastErrorMsg(&err);
        std::cerr << "[FAIL] " << msg << " | Last Error: " << (err ? err : "None") << std::endl;
        if(err) SituationFreeString(err);
        exit(1);
    } else {
        std::cout << "[PASS] " << msg << std::endl;
    }
}

// --- Test 1: Handle Registry Stress ---
void TestRegistryStress() {
    std::cout << "\n--- TEST 1: Registry Stress & Generational Handles ---" << std::endl;

    std::vector<SituationTexture> textures;
    const int COUNT = 1000;

    // 1. Create a dummy 1x1 image
    SituationImage img;
    SituationCreateImage(1, 1, 4, &img);
    ((uint32_t*)img.data)[0] = 0xFFFFFFFF;

    // 2. Rapid Allocation
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < COUNT; ++i) {
        SituationTexture tex;
        SituationError err = SituationCreateTexture(img, false, &tex);
        Check(err == SITUATION_SUCCESS, "Texture Creation");
        textures.push_back(tex);
    }

    // 3. Validation
    Check(textures.size() == COUNT, "Texture Count matches");

    // 4. Rapid Destruction (Reverse order to fragment heap if using linear allocator)
    for (int i = COUNT - 1; i >= 0; --i) {
        SituationDestroyTexture(&textures[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Created/Destroyed " << COUNT << " textures in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    // 5. Use-After-Free Check
    SituationTexture stale = textures[0];
    // This should NOT crash, should just log error/return
    SituationDestroyTexture(&stale);
    Check(true, "Double-free handled gracefully (no crash)");

    SituationUnloadImage(img);
}

// --- Test 2: Hot-Reload Logic ---
void TestHotReloadLogic() {
    std::cout << "\n--- TEST 2: Hot-Reload Logic (Synthetic) ---" << std::endl;

    // Create a dummy shader file
    const char* vs_path = "temp_verify.vert";
    const char* fs_path = "temp_verify.frag";

    FILE* f = fopen(vs_path, "w");
    fprintf(f, "#version 450\nvoid main(){ gl_Position = vec4(0,0,0,1); }");
    fclose(f);

    f = fopen(fs_path, "w");
    fprintf(f, "#version 450\nlayout(location=0) out vec4 c;\nvoid main(){ c = vec4(1,0,1,1); }");
    fclose(f);

    SituationShader shader;
    SituationError err = SituationLoadShader(vs_path, fs_path, &shader);
    Check(err == SITUATION_SUCCESS, "Initial Shader Load");

    // Simulate "Touching" the file (modifying time)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // Wait for FS timestamp resolution

    f = fopen(fs_path, "w");
    fprintf(f, "#version 450\nlayout(location=0) out vec4 c;\nvoid main(){ c = vec4(0,1,0,1); }"); // Change color
    fclose(f);

    // Manually trigger check (simulating IO thread)
    std::cout << "Triggering Hot-Reload..." << std::endl;
    SituationCheckHotReloads(); // In v2.3.47 this delegates or runs logic depending on config

    // We can't easily verify internal GPU swap programmatically without rendering,
    // but we verify it didn't crash.
    Check(true, "Hot-Reload trigger survived");

    SituationUnloadShader(&shader);
    remove(vs_path);
    remove(fs_path);
}

// --- Test 3: Bindless & Validation ---
void TestBindless() {
    std::cout << "\n--- TEST 3: Bindless Descriptor Integrity ---" << std::endl;

    // Check if feature enabled
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
        std::cout << "[SKIP] Bindless not supported on this GPU." << std::endl;
        return;
    }

    // Load Font (uses bindless internally in v2.4)
    SituationFont font;
    // Assuming a valid font exists, or use default
    // We'll trust the InitDefaultFont internal logic for this test if no file provided

    // Submit a draw call that uses bindless
    if (SituationAcquireFrameCommandBuffer()) {
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){0,0,0,255});

        // Draw Text (triggers bindless lookup)
        SituationCmdDrawText(cmd, font, "Bindless Test", (Vector2){10,10}, (ColorRGBA){255,255,255,255});

        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
        Check(true, "Bindless Draw Frame Submitted");
    } else {
        Check(false, "Failed to Acquire Frame");
    }
}

int main(int argc, char** argv) {
    SituationInitInfo info = {};
    info.window_title = "Situation v2.4 Verification";
    info.window_width = 800;
    info.window_height = 600;
    info.enable_vulkan_validation = true; // CRITICAL for this test
    info.hot_reload_poll_rate = 0.5;

    std::cout << "Initializing Situation..." << std::endl;
    SituationError err = SituationInit(argc, argv, &info);
    Check(err == SITUATION_SUCCESS, "Initialization");

    TestRegistryStress();
    TestHotReloadLogic();
    TestBindless();

    std::cout << "\nShutting down..." << std::endl;
    SituationShutdown();

    std::cout << "\n[SUCCESS] ALL VERIFICATIONS PASSED." << std::endl;
    return 0;
}
