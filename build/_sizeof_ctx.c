#include <stdio.h>
typedef enum { SIT_SPIRV_LAYOUT_PROFILE_MESH = 0 } SituationSpirvLayoutProfile;
typedef struct { void* data; size_t size; } _SituationSpirvBlob;
typedef struct {
    int compile_done;
    SituationSpirvLayoutProfile layout_profile;
    char* vs_src;
    char* fs_src;
    unsigned char* vs_spirv_copy;
    size_t vs_spirv_len;
    unsigned char* fs_spirv_copy;
    size_t fs_spirv_len;
    _SituationSpirvBlob vs_spirv;
    _SituationSpirvBlob fs_spirv;
} _SituationVkAsyncShaderLoad;
int main(void) {
    printf("sizeof=%zu\n", sizeof(_SituationVkAsyncShaderLoad));
    return 0;
}
