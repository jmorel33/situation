#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER
#include "situation.h"

int main() {
    SituationInitInfo info = {0};
    // Basic check to ensure new symbols are visible
    SituationCreateImage(100, 100, 4);
    return 0;
}
