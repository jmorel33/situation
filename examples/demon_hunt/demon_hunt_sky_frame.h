/* std140 SkyFrame UBO — must match layout in demon_hunt_sky.fs (binding = 0). */
#ifndef DEMON_HUNT_SKY_FRAME_H
#define DEMON_HUNT_SKY_FRAME_H

#include <stdint.h>

typedef struct DemonHuntSkyFrameUbo {
    float u_time;
    float u_bob_phase;
    float u_threat_pulse;
    float u_music_pulse;
    float u_yaw;
    float u_horizon_px_from_top;
    float u_horizon_shift_px;
    float u_tan_half_fov;
    float u_resolution[2];
    float _pad_before_cam[2];
    float u_cam_pos[3];
    float _pad_cam;
    float u_sun_dir[3];
    float _pad_sun;
    float u_map_size[2];
    int32_t u_sprite_count;
    int32_t u_sprite_debug_mode;
    int32_t u_shader_sprites_enabled;
    float u_pain_flash;
    float _pad_before_mat4[2]; /* std140: mat4 aligns to 112 */
    float u_flat_inv_vp[16];
} DemonHuntSkyFrameUbo;

#define DEMON_HUNT_SKY_FRAME_UBO_BYTES ((size_t)sizeof(DemonHuntSkyFrameUbo))

#endif
