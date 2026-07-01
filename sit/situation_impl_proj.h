#ifndef SITUATION_IMPL_PROJ_H
#define SITUATION_IMPL_PROJ_H

#ifdef SITUATION_IMPLEMENTATION

#include <cglm/cglm.h>

SITAPI void SituationCameraBuildView(const SituationCameraDesc* desc, mat4 out_view) {
    if (!desc) return;
    vec3 eye, target, up;
    glm_vec3_copy((float*)&desc->eye, eye);
    glm_vec3_copy((float*)&desc->target, target);
    if (desc->up.x == 0.0f && desc->up.y == 0.0f && desc->up.z == 0.0f) {
        glm_vec3_copy((vec3){0.0f, 1.0f, 0.0f}, up);
    } else {
        glm_vec3_copy((float*)&desc->up, up);
    }
    glm_lookat(eye, target, up, out_view);
}

SITAPI void SituationCameraBuildProj(const SituationCameraDesc* desc, mat4 out_proj) {
    if (!desc) return;
    float aspect = desc->aspect;
    if (aspect <= 0.0f) {
        int w = SituationGetRenderWidth();
        int h = SituationGetRenderHeight();
        aspect = h > 0 ? (float)w / (float)h : 1.0f;
    }
    
    if (desc->flags & SIT_CAMERA_FLAG_ORTHOGRAPHIC) {
        float half_h = desc->ortho_height * 0.5f;
        float half_w = half_h * aspect;
        glm_ortho(-half_w, half_w, -half_h, half_h, desc->z_near, desc->z_far, out_proj);
    } else {
        float fov_rad = glm_rad(desc->vertical_fov_deg);
        if (desc->flags & SIT_CAMERA_FLAG_INFINITE_PROJECTION) {
            // Placeholder: Use a very large far plane to approximate infinite perspective
            glm_perspective(fov_rad, aspect, desc->z_near, 1000000.0f, out_proj);
        } else {
            glm_perspective(fov_rad, aspect, desc->z_near, desc->z_far, out_proj);
        }
    }
}

SITAPI void SituationCameraBuildViewProj(const SituationCameraDesc* desc, mat4 out_vp) {
    if (!desc) return;
    mat4 view, proj;
    SituationCameraBuildView(desc, view);
    SituationCameraBuildProj(desc, proj);
    glm_mat4_mul(proj, view, out_vp);
}

SITAPI void SituationCameraBuildInvViewProj(const SituationCameraDesc* desc, mat4 out_inv_vp) {
    if (!desc) return;
    mat4 vp;
    SituationCameraBuildViewProj(desc, vp);
    glm_mat4_inv(vp, out_inv_vp);
}

SITAPI void SituationCameraUnprojectPixel(const SituationCameraDesc* desc, const mat4 inv_vp, Vector2 pixel, Vector2 framebuffer_px, Vector3* out_ray_origin, Vector3* out_ray_dir) {
    if (!desc || !out_ray_origin || !out_ray_dir) return;
    
    vec3 screen_pos;
    screen_pos[0] = pixel.x;
    // Assuming Situation pixels use top-left origin (standard UI/screen coordinates),
    // and cglm unproject expects a bottom-left origin viewport like OpenGL:
    screen_pos[1] = framebuffer_px.y - pixel.y; 
    screen_pos[2] = 0.0f; // Near plane
    
    vec4 viewport = {0.0f, 0.0f, framebuffer_px.x, framebuffer_px.y};
    
    // cglm's glm_unproject takes a mat4. We cast away const if necessary.
    mat4 m_inv_vp;
    glm_mat4_copy((vec4*)inv_vp, m_inv_vp);
    
    vec3 ray_origin, ray_target;
    glm_unprojecti(screen_pos, m_inv_vp, viewport, ray_origin);
    
    screen_pos[2] = 1.0f; // Far plane
    glm_unprojecti(screen_pos, m_inv_vp, viewport, ray_target);
    
    glm_vec3_copy(ray_origin, (float*)out_ray_origin);
    
    vec3 dir;
    glm_vec3_sub(ray_target, ray_origin, dir);
    glm_vec3_normalize(dir);
    glm_vec3_copy(dir, (float*)out_ray_dir);
}

#endif // SITUATION_IMPLEMENTATION
#endif // SITUATION_IMPL_PROJ_H
