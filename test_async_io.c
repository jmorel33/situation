#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // minimal
#define SITUATION_ENABLE_THREADING
#include "situation.h"
#include <stdio.h>
#include <string.h>

volatile bool load_complete = false;
volatile bool save_complete = false;

void OnLoad(void* data, size_t size, void* user_data) {
    printf("Callback: Load complete. Size: %zu\n", size);
    if (data) {
        printf("Data: %s\n", (char*)data);
        SIT_FREE(data);
    }
    load_complete = true;
}

void OnSave(bool success, void* user_data) {
    printf("Callback: Save complete. Success: %d\n", success);
    save_complete = true;
}

int main() {
    printf("Initializing...\n");
    SituationInitInfo info = {0};
    info.flags = 0;

    // We use headless or minimal init for test
    if (SituationInit(0, NULL, &info) != SITUATION_SUCCESS) {
        printf("Init failed: %s\n", SituationGetLastErrorMsg());
        return 1;
    }

    SituationThreadPool pool;
    if (!SituationCreateThreadPool(&pool, 2, 1024)) {
        printf("Thread pool creation failed.\n");
        return 1;
    }

    // TEST 1: Async Save
    const char* test_data = "Hello Async World!";
    printf("Starting Async Save...\n");
    SituationSaveFileAsync(&pool, "test_async.txt", test_data, strlen(test_data)+1, OnSave, NULL);

    while (!save_complete) {
        printf(".");
        fflush(stdout);
        SituationPollInputEvents(); // Keep main loop alive
        thrd_sleep(&(struct timespec){.tv_nsec=10000000}, NULL); // 10ms
    }
    printf("\nAsync Save Done.\n");

    // TEST 2: Async Load
    printf("Starting Async Load...\n");
    SituationLoadFileAsync(&pool, "test_async.txt", OnLoad, NULL);

    while (!load_complete) {
        printf(".");
        fflush(stdout);
        SituationPollInputEvents();
        thrd_sleep(&(struct timespec){.tv_nsec=10000000}, NULL);
    }
    printf("\nAsync Load Done.\n");

    SituationDestroyThreadPool(&pool);
    SituationShutdown();

    remove("test_async.txt"); // Cleanup
    return 0;
}
