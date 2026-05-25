#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER
#include "sit/situation_impl_decl.h"
#include <stdio.h>
int main(void) {
    printf("sizeof(_SituationVkAsyncShaderLoad)=%zu\n", sizeof(_SituationVkAsyncShaderLoad));
    printf("sizeof(_SituationSpirvBlob)=%zu\n", sizeof(_SituationSpirvBlob));
    return 0;
}
