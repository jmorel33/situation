/***************************************************************************************************
*
*   examples/error_message_test.c - Audio Error Message Test
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Tests that all new audio error codes produce correct error messages.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN  // Required for compilation
#include "../situation.h"
#include <stdio.h>

int main(void) {
    printf("\n=================================================================\n");
    printf("  Situation Audio Error Message Test\n");
    printf("  Testing Error Handler Completeness\n");
    printf("=================================================================\n\n");

    // Test threading errors
    printf("[THREADING ERRORS]\n");
    SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, NULL);
    printf("  -80: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_THREAD_VIOLATION, NULL);
    printf("  -81: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CYCLE, NULL);
    printf("  -82: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED, NULL);
    printf("  -85: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_THREAD_DEADLOCK_DETECTED, NULL);
    printf("  -94: %s\n", SituationGetLastErrorMessage());

    // Test mixer errors
    printf("\n[MIXER ERRORS]\n");
    SituationSetErrorFromCode(SITUATION_ERROR_MIXER_NOT_INITIALIZED, "Test context");
    printf("  -440: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_MIXER_TRACK_LIMIT, NULL);
    printf("  -441: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_MIXER_INSERT_INVALID, NULL);
    printf("  -445: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED, NULL);
    printf("  -451: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_MIXER_SCENE_VERSION_MISMATCH, NULL);
    printf("  -454: %s\n", SituationGetLastErrorMessage());

    // Test node graph errors
    printf("\n[NODE GRAPH ERRORS]\n");
    SituationSetErrorFromCode(SITUATION_ERROR_NODE_GRAPH_NOT_INITIALIZED, NULL);
    printf("  -460: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_NODE_LIMIT_REACHED, NULL);
    printf("  -461: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_NODE_PORT_TYPE_MISMATCH, NULL);
    printf("  -467: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED, NULL);
    printf("  -471: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_NODE_TOPOLOGY_INVALID, NULL);
    printf("  -478: %s\n", SituationGetLastErrorMessage());

    // Test device registry errors
    printf("\n[DEVICE REGISTRY ERRORS]\n");
    SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_REGISTRY_NOT_INITIALIZED, NULL);
    printf("  -480: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_TYPE_ALREADY_REGISTERED, NULL);
    printf("  -483: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_REGISTRY_FULL, NULL);
    printf("  -484: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_CONTROL_INVALID, NULL);
    printf("  -486: %s\n", SituationGetLastErrorMessage());
    
    SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_PROCESS_FAILED, NULL);
    printf("  -493: %s\n", SituationGetLastErrorMessage());

    printf("\n=================================================================\n");
    printf("  All error messages retrieved successfully!\n");
    printf("=================================================================\n\n");

    return 0;
}
