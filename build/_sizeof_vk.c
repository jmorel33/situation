#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include "shaderc/shaderc.h"
typedef enum { SIT_SPIRV_LAYOUT_PROFILE_MESH = 0 } SituationSpirvLayoutProfile;
typedef struct _SituationSpirvBlob {
    const uint8_t* data;
    size_t size;
    shaderc_compilation_result_t internal_result;
    uint64_t source_hash;
} _SituationSpirvBlob;
typedef struct _SituationVkAsyncShaderLoad {
    _Atomic int compile_done;
    SituationSpirvLayoutProfile layout_profile;
    char* vs_src;
    char* fs_src;
    uint8_t* vs_spirv_copy;
    size_t vs_spirv_len;
    uint8_t* fs_spirv_copy;
    size_t fs_spirv_len;
    _SituationSpirvBlob vs_spirv;
    _SituationSpirvBlob fs_spirv;
} _SituationVkAsyncShaderLoad;
int main(void) {
    printf("sizeof(_SituationSpirvBlob)=%zu\n", sizeof(_SituationSpirvBlob));
    printf("sizeof(_SituationVkAsyncShaderLoad)=%zu\n", sizeof(_SituationVkAsyncShaderLoad));
    return 0;
}
