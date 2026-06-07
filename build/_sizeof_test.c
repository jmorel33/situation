#include <stdio.h>
typedef struct { const unsigned char* data; size_t size; void* internal_result; unsigned long long source_hash; } blob;
typedef struct { int compile_done; int layout_profile; char* vs_src; char* fs_src; unsigned char* vs_spirv_copy; size_t vs_spirv_len; unsigned char* fs_spirv_copy; size_t fs_spirv_len; blob vs_spirv; blob fs_spirv; } ctx;
int main() { printf("sizeof ctx = %zu\n", sizeof(ctx)); return 0; }
