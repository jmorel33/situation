/***************************************************************************************************
 *  Situation — 07: YPQ Color Grading
 *
 *  A procedural 512×512 "color laboratory" split down the middle:
 *    LEFT  — original RGB (SituationCmdDrawTexture)
 *    RIGHT — live YPQ grade (SituationCmdDrawTextureYpqGrade, GPU shader)
 *
 *  The atlas mixes a spectrum wheel, hue bars, Macbeth-style chips, and a sunset
 *  gradient so phase / chroma / luma changes are obvious at a glance.
 *  A small P–Q wheel shows that phase is a chroma-plane rotation — not an HSV hue twist.
 *
 *  Keys:
 *    Q/A  phase °     W/S  chroma ×     E/D  luma ×     R/F  mix
 *    1–4  cinematic presets   A  auto phase sweep   SPACE  reset
 *    X    run SituationYpqAnalyzeRgbMapping (console, ~few seconds)
 *
 *  Build:
 *    build\build_examples.bat static-opengl  07_ypq_color_grading
 *    build\build_examples.bat static-vulkan  07_ypq_color_grading
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <math.h>
#include <string.h>

#define LAB_W 512
#define LAB_H 512

static SituationTexture g_tex = {0};
static SituationImage   g_source = {0};

static float g_phase  = 0.0f;
static float g_chroma = 1.0f;
static float g_luma   = 1.0f;
static float g_mix    = 1.0f;
static int   g_auto   = 0;
static char  g_status[96] = "YPQ live grade — GPU path";

typedef struct { float phase, chroma, luma, mix; const char* name; } YpqPreset;

static const YpqPreset k_presets[] = {
    {  28.0f, 1.35f, 1.05f, 1.0f, "Teal / Orange" },
    {   0.0f, 0.35f, 1.45f, 1.0f, "Bleach Bypass" },
    { 180.0f, 0.15f, 0.55f, 1.0f, "Noir"          },
    { 120.0f, 2.00f, 1.15f, 1.0f, "Hyperpop"      },
};

static void hsv_to_rgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b)
{
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf = 0, gf = 0, bf = 0;
    if      (h < 60.0f)  { rf = c; gf = x; }
    else if (h < 120.0f) { rf = x; gf = c; }
    else if (h < 180.0f) { gf = c; bf = x; }
    else if (h < 240.0f) { gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; bf = c; }
    else                 { rf = c; bf = x; }
    *r = (uint8_t)fminf(255.0f, (rf + m) * 255.0f);
    *g = (uint8_t)fminf(255.0f, (gf + m) * 255.0f);
    *b = (uint8_t)fminf(255.0f, (bf + m) * 255.0f);
}

static void put_px(SituationImage* img, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return;
    uint8_t* p = (uint8_t*)img->data + (y * img->width + x) * 4;
    p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
}

static void lerp_color(ColorRGBA a, ColorRGBA b_col, float t, uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = (uint8_t)((1.0f - t) * a.r + t * b_col.r);
    *g = (uint8_t)((1.0f - t) * a.g + t * b_col.g);
    *b = (uint8_t)((1.0f - t) * a.b + t * b_col.b);
}

/** Procedural atlas — no external assets. Designed to show off YPQ phase vs HSV hue. */
static SituationError gen_color_lab(SituationImage* out)
{
    SituationError err = SituationCreateImage(LAB_W, LAB_H, 4, out);
    if (err != SITUATION_SUCCESS) return err;
    out->channels = 4;

    static const ColorRGBA macbeth[] = {
        {115, 82, 68, 255}, {194, 150, 130, 255}, {98, 122, 157, 255}, {87, 108, 67, 255},
        {133, 128, 177, 255}, {103, 189, 170, 255}, {214, 126, 44, 255}, {80, 91, 166, 255},
        {193, 90, 99, 255}, {94, 60, 108, 255}, {157, 188, 64, 255}, {224, 163, 46, 255},
        {56, 61, 150, 255}, {70, 148, 73, 255}, {175, 51, 60, 255}, {231, 199, 31, 255},
        {187, 86, 149, 255}, {8, 133, 161, 255}, {243, 243, 242, 255}, {200, 200, 200, 255},
        {160, 160, 160, 255}, {122, 122, 121, 255}, {85, 85, 85, 255}, {52, 52, 52, 255},
    };

    for (int y = 0; y < LAB_H; ++y) {
        for (int x = 0; x < LAB_W; ++x) {
            uint8_t r = 0, g = 0, b = 0;

            if (x < LAB_W / 2 && y < LAB_H / 2) {
                /* Q1 — spectrum wheel (phase rotation visibly "spins" chroma) */
                float dx = (float)x - 127.5f;
                float dy = (float)y - 127.5f;
                float ang = atan2f(dy, dx) * (180.0f / (float)M_PI);
                if (ang < 0.0f) ang += 360.0f;
                float dist = sqrtf(dx * dx + dy * dy) / 118.0f;
                float sat = fminf(1.0f, dist);
                float val = 0.72f + 0.28f * (1.0f - fminf(1.0f, dist));
                hsv_to_rgb(ang, sat, val, &r, &g, &b);
            } else if (x >= LAB_W / 2 && y < LAB_H / 2) {
                /* Q2 — vertical hue bars + luma ramp */
                int bar = (x - LAB_W / 2) / 16;
                float hue = (float)(bar * 15) * (360.0f / 24.0f);
                float val = 0.35f + 0.65f * (1.0f - (float)y / (float)(LAB_H / 2 - 1));
                hsv_to_rgb(hue, 1.0f, val, &r, &g, &b);
            } else if (x < LAB_W / 2 && y >= LAB_H / 2) {
                /* Q3 — Macbeth-style chip grid */
                int gx = (x * 6) / (LAB_W / 2);
                int gy = ((y - LAB_H / 2) * 4) / (LAB_H / 2);
                int idx = gy * 6 + gx;
                if (idx >= (int)(sizeof macbeth / sizeof macbeth[0])) idx = 0;
                r = macbeth[idx].r; g = macbeth[idx].g; b = macbeth[idx].b;
            } else {
                /* Q4 — cinematic sunset gradient (phase = "film look") */
                float tx = ((float)(x - LAB_W / 2)) / (float)(LAB_W / 2 - 1);
                float ty = ((float)(y - LAB_H / 2)) / (float)(LAB_H / 2 - 1);
                ColorRGBA tl = { 40,  20,  80, 255};
                ColorRGBA tr = {220,  90,  40, 255};
                ColorRGBA bl = { 10,  30,  60, 255};
                ColorRGBA br = {255, 180,  80, 255};
                ColorRGBA top, bot, fin;
                top.r = (uint8_t)(tl.r * (1-tx) + tr.r * tx);
                top.g = (uint8_t)(tl.g * (1-tx) + tr.g * tx);
                top.b = (uint8_t)(tl.b * (1-tx) + tr.b * tx);
                bot.r = (uint8_t)(bl.r * (1-tx) + br.r * tx);
                bot.g = (uint8_t)(bl.g * (1-tx) + br.g * tx);
                bot.b = (uint8_t)(bl.b * (1-tx) + br.b * tx);
                lerp_color(top, bot, ty, &r, &g, &b);
            }
            put_px(out, x, y, r, g, b);
        }
    }
    return SITUATION_SUCCESS;
}

static void draw_rect(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 c)
{
    mat4 m; glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, c);
}

static void txt(SituationCommandBuffer cmd, const char* s, float x, float y, float fs, ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s, (Vector2){{x, y}}, fs, 0.0f, c);
}

static void draw_pq_wheel(SituationCommandBuffer cmd, float cx, float cy, float radius, float phase_deg)
{
    draw_rect(cmd, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f,
              (Vector4){{0.06f, 0.07f, 0.11f, 0.85f}});

    float rad = phase_deg * ((float)M_PI / 180.0f);
    float px = cx + cosf(rad) * (radius - 4.0f);
    float py = cy + sinf(rad) * (radius - 4.0f);
    float mx = (cx + px) * 0.5f;
    float my = (cy + py) * 0.5f;
    float len = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));

    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){mx, my, 0.0f});
    glm_rotate_z(m, rad, m);
    glm_scale(m, (vec3){len, 3.0f, 1.0f});
    SituationCmdDrawQuad(cmd, m, (Vector4){{1.0f, 0.35f, 0.55f, 1.0f}});

    draw_rect(cmd, cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, (Vector4){{1.0f, 0.9f, 0.3f, 1.0f}});
    txt(cmd, "P", cx + radius + 4.0f, cy - 5.0f, 10.0f, (ColorRGBA){180, 190, 230, 220});
    txt(cmd, "Q", cx - 4.0f, cy + radius + 2.0f, 10.0f, (ColorRGBA){180, 190, 230, 220});
}

static void param_bar(SituationCommandBuffer cmd, const char* label, float val, float lo, float hi,
                      float x, float y, float w, ColorRGBA col)
{
    float t = (hi > lo) ? (val - lo) / (hi - lo) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    txt(cmd, label, x, y, 11.0f, (ColorRGBA){150, 155, 190, 220});
    float bx = x + 72.0f;
    float bw = w - 120.0f;
    draw_rect(cmd, bx, y + 2.0f, bw, 10.0f, (Vector4){{0.07f, 0.08f, 0.12f, 1.0f}});
    if (t > 0.001f) {
        draw_rect(cmd, bx, y + 2.0f, bw * t, 10.0f,
                  (Vector4){{col.r/255.0f, col.g/255.0f, col.b/255.0f, 0.9f}});
    }
    char buf[24];
    snprintf(buf, sizeof buf, "%.2f", val);
    txt(cmd, buf, bx + bw + 6.0f, y, 11.0f, (ColorRGBA){220, 225, 255, 240});
}

static void apply_preset(int idx)
{
    if (idx < 0 || idx >= (int)(sizeof k_presets / sizeof k_presets[0])) return;
    g_phase  = k_presets[idx].phase;
    g_chroma = k_presets[idx].chroma;
    g_luma   = k_presets[idx].luma;
    g_mix    = k_presets[idx].mix;
    snprintf(g_status, sizeof g_status, "Preset: %s", k_presets[idx].name);
}

static void reset_params(void)
{
    g_phase = 0.0f; g_chroma = 1.0f; g_luma = 1.0f; g_mix = 1.0f; g_auto = 0;
    snprintf(g_status, sizeof g_status, "Reset to defaults");
}

static void draw_scene(SituationCommandBuffer cmd, int sw, int sh)
{
    const float panel_y = 58.0f;
    const float panel_h = (float)sh - panel_y - 110.0f;
    const float gap = 8.0f;
    const float total_w = fminf((float)sw - 40.0f, 1020.0f);
    const float half_w = (total_w - gap) * 0.5f;
    const float ox = ((float)sw - total_w) * 0.5f;
    const float oy = panel_y + 20.0f;
    const float img_h = fminf(panel_h - 24.0f, half_w);

    txt(cmd, "ORIGINAL RGB", ox + half_w * 0.5f - 50.0f, panel_y,
        13.0f, (ColorRGBA){140, 150, 180, 220});
    txt(cmd, "YPQ GRADED (GPU)", ox + half_w + gap + half_w * 0.5f - 58.0f, panel_y,
        13.0f, (ColorRGBA){255, 210, 100, 255});

    SitRectangle src = {0.0f, 0.0f, (float)LAB_W, (float)LAB_H};
    SitRectangle dst_l = {ox, oy, half_w, img_h};
    SitRectangle dst_r = {ox + half_w + gap, oy, half_w, img_h};
    Vector2 origin = {{0.0f, 0.0f}};

    SituationCmdDrawTexture(cmd, g_tex, src, dst_l, origin, 0.0f,
                            (ColorRGBA){255, 255, 255, 255});
    SituationCmdDrawTextureYpqGrade(cmd, g_tex, src, dst_r, origin, 0.0f,
                                    g_phase, g_chroma, g_luma, g_mix);

    draw_rect(cmd, ox + half_w + gap * 0.5f - 2.0f, oy - 4.0f, 4.0f, img_h + 8.0f,
              (Vector4){{1.0f, 1.0f, 1.0f, 0.85f}});

    float hud_y = oy + img_h + 14.0f;
    param_bar(cmd, "Phase°", g_phase, 0.0f, 360.0f, ox, hud_y, half_w,
              (ColorRGBA){255, 120, 180, 255});
    param_bar(cmd, "Chroma", g_chroma, 0.0f, 2.5f, ox + half_w + gap, hud_y, half_w,
              (ColorRGBA){120, 220, 255, 255});
    param_bar(cmd, "Luma", g_luma, 0.25f, 1.75f, ox, hud_y + 18.0f, half_w,
              (ColorRGBA){255, 220, 100, 255});
    param_bar(cmd, "Mix", g_mix, 0.0f, 1.0f, ox + half_w + gap, hud_y + 18.0f, half_w,
              (ColorRGBA){180, 255, 160, 255});

    draw_pq_wheel(cmd, ox + total_w - 52.0f, hud_y + 36.0f, 34.0f, g_phase);

    txt(cmd, "Phase rotates chroma in the YIQ plane — not the same as HSV hue.",
        ox, hud_y + 42.0f, 10.0f, (ColorRGBA){130, 135, 170, 210});
    txt(cmd, g_status, ox, (float)sh - 56.0f, 11.0f, (ColorRGBA){255, 220, 90, 255});
    if (g_auto) {
        txt(cmd, "[AUTO SWEEP]", ox + total_w - 110.0f, hud_y + 42.0f, 11.0f,
            (ColorRGBA){100, 255, 180, 255});
    }
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "07 — YPQ Color Grading") != SITUATION_SUCCESS) {
        return -1;
    }

    if (gen_color_lab(&g_source) != SITUATION_SUCCESS) {
        fprintf(stderr, "[07] gen_color_lab failed\n");
        SitExample_Shutdown();
        return -1;
    }

    /* Teach CPU path once (plan API) — GPU handles live preview. */
    {
        SituationImage cpu = {0};
        if (SituationImageCopy(g_source, &cpu) == SITUATION_SUCCESS) {
            cpu.channels = 4;
            SituationImageAdjustYPQ(&cpu, 45.0f, 1.5f, 1.1f, 1.0f);
            SituationUnloadImage(cpu);
            printf("[07] CPU SituationImageAdjustYPQ verified on copy\n");
        }
    }

    if (SituationCreateTexture(g_source, false, &g_tex) != SITUATION_SUCCESS) {
        fprintf(stderr, "[07] CreateTexture failed\n");
        SituationUnloadImage(g_source);
        SitExample_Shutdown();
        return -1;
    }

    printf("[07] Color lab %dx%d — split view, GPU YPQ grade on right\n", LAB_W, LAB_H);

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        float dt = SituationGetFrameTime();
        if (SituationIsKeyDown(SIT_KEY_Q)) g_phase = fmodf(g_phase + dt * 90.0f, 360.0f);
        if (SituationIsKeyDown(SIT_KEY_A)) g_phase = fmodf(g_phase - dt * 90.0f + 360.0f, 360.0f);
        if (SituationIsKeyDown(SIT_KEY_W))   g_chroma = fminf(g_chroma + dt * 0.8f, 2.5f);
        if (SituationIsKeyDown(SIT_KEY_S))   g_chroma = fmaxf(g_chroma - dt * 0.8f, 0.0f);
        if (SituationIsKeyDown(SIT_KEY_E))   g_luma   = fminf(g_luma + dt * 0.6f, 1.75f);
        if (SituationIsKeyDown(SIT_KEY_D))   g_luma   = fmaxf(g_luma - dt * 0.6f, 0.25f);
        if (SituationIsKeyDown(SIT_KEY_R))   g_mix    = fminf(g_mix + dt * 0.5f, 1.0f);
        if (SituationIsKeyDown(SIT_KEY_F))   g_mix    = fmaxf(g_mix - dt * 0.5f, 0.0f);

        if (SituationIsKeyPressed(SIT_KEY_1)) apply_preset(0);
        if (SituationIsKeyPressed(SIT_KEY_2)) apply_preset(1);
        if (SituationIsKeyPressed(SIT_KEY_3)) apply_preset(2);
        if (SituationIsKeyPressed(SIT_KEY_4)) apply_preset(3);
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) reset_params();
        if (SituationIsKeyPressed(SIT_KEY_TAB)) g_auto = !g_auto;

        if (g_auto) {
            g_phase = fmodf(g_phase + dt * 48.0f, 360.0f);
        }

        if (SituationIsKeyPressed(SIT_KEY_X)) {
            snprintf(g_status, sizeof g_status, "Analyzing 256³ YPQ cube…");
            SituationYpqRgbMappingStats stats = {0};
            SituationError err = SituationYpqAnalyzeRgbMapping(&stats);
            if (err == SITUATION_SUCCESS) {
                printf("[07] YPQ→RGB: unique=%lld dup=%lld holes=%lld worstQ@%d=%d\n",
                       (long long)stats.unique_rgb,
                       (long long)stats.duplicate_mappings,
                       (long long)stats.rgb_holes,
                       stats.worst_axis_at, stats.worst_axis_dup);
                snprintf(g_status, sizeof g_status,
                         "unique RGB: %.2fM  holes: %.2fM  (see console)",
                         stats.unique_rgb / 1e6, stats.rgb_holes / 1e6);
            } else {
                snprintf(g_status, sizeof g_status, "Analyze failed");
            }
        }

        if (SituationIsAppPaused()) continue;

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationRenderPassInfo pass =
            SituationRenderPassInfoDefault(-1, (ColorRGBA){6, 8, 14, 255});
        SituationCmdBeginRenderPass(cmd, &pass);
        draw_scene(cmd, SituationGetRenderWidth(), SituationGetRenderHeight());
        SitExample_DrawHUD(cmd, "07 — YPQ Color Grading",
            "Q/A phase  W/S chroma  E/D luma  R/F mix  1-4 presets  TAB auto  X analyze");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SituationDestroyTexture(&g_tex);
    SituationUnloadImage(g_source);
    SitExample_Shutdown();
    return 0;
}
