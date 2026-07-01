#include <stdio.h>
#include <stddef.h>
#include "sit/situation_api.h"
int main(void) {
    printf("sizeof(SituationInitInfo)=%zu\n", sizeof(SituationInitInfo));
    printf("off output_color_depth=%zu\n", offsetof(SituationInitInfo, output_color_depth));
    printf("off max_audio_voices=%zu\n", offsetof(SituationInitInfo, max_audio_voices));
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    printf("off render_thread_count=%zu\n", offsetof(SituationInitInfo, render_thread_count));
    printf("off backpressure_policy=%zu\n", offsetof(SituationInitInfo, backpressure_policy));
#endif
    printf("off io_queue_capacity=%zu\n", offsetof(SituationInitInfo, io_queue_capacity));
    printf("off disable_io_thread=%zu\n", offsetof(SituationInitInfo, disable_io_thread));
    printf("off main_thread_name=%zu\n", offsetof(SituationInitInfo, main_thread_name));
    printf("off thread_pool_reserved=%zu\n", offsetof(SituationInitInfo, thread_pool_reserved_threads));
    return 0;
}
