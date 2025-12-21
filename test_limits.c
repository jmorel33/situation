#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <stdio.h>

int main() {
    if (SituationInit(0, NULL, NULL) != SITUATION_SUCCESS) {
        printf("Failed to initialize Situation\n");
        return 1;
    }

    int x = 0, y = 0, z = 0;
    SituationGetMaxComputeWorkGroups(&x, &y, &z);
    printf("Max work groups: %d, %d, %d\n", x, y, z);

    SituationShutdown();
    return 0;
}
