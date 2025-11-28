#ifndef CGLM_H
#define CGLM_H

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];
typedef vec4 mat4[4];
typedef float mat2[2][2];

// Mock functions used in situation.h
#define glm_vec2_mul(a, b, c)
#define glm_vec2_add(a, b, c)
#define glm_vec2_sub(a, b, c)
#define glm_vec2_copy(a, b)
#define glm_vec2_one(v) (v[0]=1.0f, v[1]=1.0f)
#define glm_mat4_identity(m)
#define glm_mat4_copy(src, dst)
#define glm_vec4_copy(src, dst)
#define glm_ortho(l, r, b, t, n, f, dest)
#define glm_translate(m, v)
#define glm_translate_make(m, v)
#define glm_scale(m, v)
#define glm_scale_make(m, v)
#define glm_mat4_mul(a, b, c)
#define cglm_mat2_mulv(m, v, r)

#endif // CGLM_H
