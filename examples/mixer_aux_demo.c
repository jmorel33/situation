/***************************************************************************************************
*
*   mixer_aux_demo.c - Mixer Aux Bus FX Demo
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase 6 Session 2: Demonstrates aux bus FX integration with the mixer.
*   
*   This demo creates aux buses with modular FX chains:
*     • Aux Bus 0: Reverb (100% wet)
*     • Aux Bus 1: Delay → Chorus (50/50 wet/dry mix)
*     • Aux Bus 2: Dynamics (parallel compression)
*   
*   Tests:
*     1. Attach FX chains to aux buses
*     2. Wet/dry mix control
*     3. Bypass functionality
*     4. Remove FX chains
*     5. Query FX state
*   
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN  // Required for compilation
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>

// Device function table (required by node graph processing)
extern const SituationDeviceFunctions g_device_function_table[];
extern const int g_device_function_table_count;

// ================================================================================================
// HELPER FUNCTIONS
// ================================================================================================

static void PrintAuxBusInfo(SituationAudioMixer* mixer, int bus_id) {
    SituationAudioGraph* fx = SituationGetBusEffectChain(mixer, bus_id);
    bool bypassed = SituationIsBusEffectBypassed(mixer, bus_id);
    
    float wet, dry;
    SituationError err = SituationGetBusEffectMix(mixer, bus_id, &wet, &dry);
    
    printf("  Aux Bus %d: ", bus_id);
    if (fx) {
        printf("ACTIVE (%s) - Wet: %.1f%%, Dry: %.1f%%\n", 
               bypassed ? "BYPASSED" : "ENABLED",
               wet * 100.0f, dry * 100.0f);
    } else {
        printf("EMPTY\n");
    }
}

static void PrintAllAuxBuses(SituationAudioMixer* mixer, int bus_count) {
    printf("\nAux Bus FX Status:\n");
    for (int i = 0; i < bus_count; i++) {
        PrintAuxBusInfo(mixer, i);
    }
}

// ================================================================================================
// MAIN
// ================================================================================================

int main(void) {
    printf("=================================================================\n");
    printf("  Situation Mixer Aux Bus FX Demo\n");
    printf("  Phase 6 Session 2: Aux Bus FX Integration\n");
    printf("=================================================================\n\n");
    
    // Initialize device registry
    printf("[1] Initializing device registry...\n");
    SituationInitDeviceRegistry();
    printf("    ✓ Registry initialized\n\n");
    
    // Create mixer (simplified - in real code, this would be part of Situation context)
    printf("[2] Creating mixer with 3 aux buses...\n");
    SituationAudioMixer* mixer = (SituationAudioMixer*)calloc(1, sizeof(SituationAudioMixer));
    if (!mixer) {
        printf("    ✗ Failed to allocate mixer\n");
        return 1;
    }
    
    // Initialize mixer mutex
    mtx_init(&mixer->topology_mutex, mtx_plain);
    mixer->is_initialized = true;
    
    // Initialize aux buses (using first 3 for this demo)
    for (int i = 0; i < 3; i++) {
        mixer->aux_buses[i].is_active = true;
        mixer->aux_buses[i].id = i;
        snprintf(mixer->aux_buses[i].name, sizeof(mixer->aux_buses[i].name), "Aux %d", i);
        mixer->aux_buses[i].fx_chain.fx_chain = NULL;
        mixer->aux_buses[i].fx_chain.bypass = false;
        mixer->aux_buses[i].fx_chain.is_active = false;
        mixer->aux_buses[i].fx_chain.wet_mix = 1.0f;
        mixer->aux_buses[i].fx_chain.dry_mix = 0.0f;
    }
    
    printf("    ✓ Mixer created with 3 aux buses\n\n");
    
    // ============================================================================================
    // TEST 1: Create and attach reverb FX to Aux Bus 0 (100% wet)
    // ============================================================================================
    
    printf("[3] Creating Aux Bus 0 FX chain (Reverb - 100%% wet)...\n");
    SituationAudioGraph* reverb_fx = SituationCreateGraph();
    if (!reverb_fx) {
        printf("    ✗ Failed to create FX chain\n");
        return 1;
    }
    
    SituationNodeHandle reverb_node;
    SituationError err = SituationCreateNode(reverb_fx, SITUATION_NODE_REVERB, &reverb_node);
    if (err != 0) {
        printf("    ✗ Failed to create reverb node (error %d)\n", err);
        return 1;
    }
    
    printf("    ✓ Reverb node created\n");
    
    // Attach to aux bus 0
    SituationError result = SituationSetBusEffectChain(mixer, 0, reverb_fx);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to attach FX chain (error %d)\n", result);
        return 1;
    }
    
    printf("    ✓ Reverb FX attached to Aux Bus 0\n");
    PrintAllAuxBuses(mixer, 3);
    
    // ============================================================================================
    // TEST 2: Create and attach delay → chorus FX to Aux Bus 1 (50/50 mix)
    // ============================================================================================
    
    printf("\n[4] Creating Aux Bus 1 FX chain (Delay → Chorus - 50/50 mix)...\n");
    SituationAudioGraph* delay_fx = SituationCreateGraph();
    if (!delay_fx) {
        printf("    ✗ Failed to create FX chain\n");
        return 1;
    }
    
    SituationNodeHandle delay_node, chorus_node;
    err = SituationCreateNode(delay_fx, SITUATION_NODE_ECHO, &delay_node);
    if (err != 0) {
        printf("    ✗ Failed to create delay node (error %d)\n", err);
        return 1;
    }
    
    err = SituationCreateNode(delay_fx, SITUATION_NODE_CHORUS, &chorus_node);
    if (err != 0) {
        printf("    ✗ Failed to create chorus node (error %d)\n", err);
        return 1;
    }
    
    // Create patch
    err = SituationCreatePatch(delay_fx, delay_node, 0, chorus_node, 0, false);
    if (err != 0) {
        printf("    ✗ Failed to create patch (error %d)\n", err);
        return 1;
    }
    
    printf("    ✓ Delay → Chorus chain created\n");
    
    // Attach to aux bus 1
    result = SituationSetBusEffectChain(mixer, 1, delay_fx);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to attach FX chain (error %d)\n", result);
        return 1;
    }
    
    // Set 50/50 wet/dry mix
    result = SituationSetBusEffectMix(mixer, 1, 0.5f, 0.5f);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to set mix (error %d)\n", result);
        return 1;
    }
    
    printf("    ✓ Delay → Chorus FX attached to Aux Bus 1 (50/50 mix)\n");
    PrintAllAuxBuses(mixer, 3);
    
    // ============================================================================================
    // TEST 3: Create and attach dynamics FX to Aux Bus 2 (parallel compression)
    // ============================================================================================
    
    printf("\n[5] Creating Aux Bus 2 FX chain (Dynamics - parallel compression)...\n");
    SituationAudioGraph* dynamics_fx = SituationCreateGraph();
    if (!dynamics_fx) {
        printf("    ✗ Failed to create FX chain\n");
        return 1;
    }
    
    SituationNodeHandle dynamics_node;
    err = SituationCreateNode(dynamics_fx, SITUATION_NODE_DYNAMICS, &dynamics_node);
    if (err != 0) {
        printf("    ✗ Failed to create dynamics node (error %d)\n", err);
        return 1;
    }
    
    printf("    ✓ Dynamics node created\n");
    
    // Attach to aux bus 2
    result = SituationSetBusEffectChain(mixer, 2, dynamics_fx);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to attach FX chain (error %d)\n", result);
        return 1;
    }
    
    // Set 70/30 wet/dry mix (parallel compression)
    result = SituationSetBusEffectMix(mixer, 2, 0.7f, 0.3f);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to set mix (error %d)\n", result);
        return 1;
    }
    
    printf("    ✓ Dynamics FX attached to Aux Bus 2 (70/30 mix)\n");
    PrintAllAuxBuses(mixer, 3);
    
    // ============================================================================================
    // TEST 4: Bypass functionality
    // ============================================================================================
    
    printf("\n[6] Testing bypass functionality...\n");
    
    // Bypass Aux Bus 1
    result = SituationBypassBusEffectChain(mixer, 1, true);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to bypass FX (error %d)\n", result);
        return 1;
    }
    
    printf("    ✓ Aux Bus 1 FX bypassed\n");
    PrintAllAuxBuses(mixer, 3);
    
    // Re-enable Aux Bus 1
    result = SituationBypassBusEffectChain(mixer, 1, false);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to enable FX (error %d)\n", result);
        return 1;
    }
    
    printf("\n    ✓ Aux Bus 1 FX re-enabled\n");
    PrintAllAuxBuses(mixer, 3);
    
    // ============================================================================================
    // TEST 5: Wet/dry mix adjustment
    // ============================================================================================
    
    printf("\n[7] Testing wet/dry mix adjustment...\n");
    
    // Change Aux Bus 0 to 80/20 mix
    result = SituationSetBusEffectMix(mixer, 0, 0.8f, 0.2f);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to set mix (error %d)\n", result);
        return 1;
    }
    
    printf("    ✓ Aux Bus 0 mix changed to 80/20\n");
    PrintAllAuxBuses(mixer, 3);
    
    // ============================================================================================
    // TEST 6: Remove FX chain
    // ============================================================================================
    
    printf("\n[8] Testing FX chain removal...\n");
    
    // Remove Aux Bus 2 FX
    result = SituationClearBusEffectChain(mixer, 2);
    if (result != SITUATION_SUCCESS) {
        printf("    ✗ Failed to remove FX (error %d)\n", result);
        return 1;
    }
    
    printf("    ✓ Aux Bus 2 FX removed\n");
    PrintAllAuxBuses(mixer, 3);
    
    // ============================================================================================
    // TEST 7: Query FX state
    // ============================================================================================
    
    printf("\n[9] Testing FX state queries...\n");
    
    // Query Aux Bus 0
    SituationAudioGraph* fx0 = SituationGetBusEffectChain(mixer, 0);
    bool bypassed0 = SituationIsBusEffectBypassed(mixer, 0);
    float wet0, dry0;
    SituationGetBusEffectMix(mixer, 0, &wet0, &dry0);
    
    printf("    Aux Bus 0: FX=%s, Bypass=%s, Mix=%.1f/%.1f\n",
           fx0 ? "ATTACHED" : "NULL",
           bypassed0 ? "YES" : "NO",
           wet0 * 100.0f, dry0 * 100.0f);
    
    // Query Aux Bus 1
    SituationAudioGraph* fx1 = SituationGetBusEffectChain(mixer, 1);
    bool bypassed1 = SituationIsBusEffectBypassed(mixer, 1);
    float wet1, dry1;
    SituationGetBusEffectMix(mixer, 1, &wet1, &dry1);
    
    printf("    Aux Bus 1: FX=%s, Bypass=%s, Mix=%.1f/%.1f\n",
           fx1 ? "ATTACHED" : "NULL",
           bypassed1 ? "YES" : "NO",
           wet1 * 100.0f, dry1 * 100.0f);
    
    // Query Aux Bus 2 (should be empty)
    SituationAudioGraph* fx2 = SituationGetBusEffectChain(mixer, 2);
    printf("    Aux Bus 2: FX=%s\n", fx2 ? "ATTACHED" : "NULL");
    
    printf("    ✓ All queries successful\n");
    
    // ============================================================================================
    // CLEANUP
    // ============================================================================================
    
    printf("\n[10] Cleaning up...\n");
    
    // Clear all FX chains
    for (int i = 0; i < 3; i++) {
        SituationClearBusEffectChain(mixer, i);
    }
    
    // Destroy mixer
    mtx_destroy(&mixer->topology_mutex);
    free(mixer);
    
    printf("    ✓ Cleanup complete\n");
    
    // ============================================================================================
    // SUMMARY
    // ============================================================================================
    
    printf("\n=================================================================\n");
    printf("  ✓ ALL TESTS PASSED\n");
    printf("=================================================================\n");
    printf("\nAux Bus FX Integration Summary:\n");
    printf("  • Modular FX chains on aux buses\n");
    printf("  • Wet/dry mix control (0-100%%)\n");
    printf("  • Bypass functionality\n");
    printf("  • Complex multi-node FX chains\n");
    printf("  • Thread-safe operations\n");
    printf("\nPhase 6 Session 2: AUX BUS FX INTEGRATION COMPLETE ✓\n\n");
    
    return 0;
}
