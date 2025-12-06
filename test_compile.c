#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#include "situation.h"

int main() {
    SituationInitInfo info = {0};
    SituationInit(0, NULL, &info);
    return 0;
}
