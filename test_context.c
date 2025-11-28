#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#define SITUATION_NO_STB
#include "situation.h"

int main() {
    SituationInitInfo info = {0};
    SituationInit(0, NULL, &info);
    SituationShutdown();
    return 0;
}
