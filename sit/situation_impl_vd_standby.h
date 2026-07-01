#ifndef SITUATION_IMPL_VD_STANDBY_H
#define SITUATION_IMPL_VD_STANDBY_H

/**
 * @file situation_impl_vd_standby.h
 * @brief VD idle PATTERN standby config — init, stack, std140 pack (P9).
 *
 * Included only from situation_impl_vd.h (library implementation).
 * Public entry points: SituationVdStandby* in situation_api_graphics.h.
 */

#include <stdint.h>
#include <string.h>

static ColorRGBA _SitVDStandbyColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    ColorRGBA c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

static void _SitVDStandbyPaletteInitRglDefaults(SitVdStandbyPalette* pal) {
    if (!pal) return;
    memset(pal, 0, sizeof(*pal));
    pal->bg_dark_gray = _SitVDStandbyColorRGBA(45, 45, 45, 255);
    pal->grid_white = _SitVDStandbyColorRGBA(255, 255, 255, 255);
    pal->bar_light_gray = _SitVDStandbyColorRGBA(192, 192, 192, 255);
    pal->bar_yellow = _SitVDStandbyColorRGBA(192, 192, 0, 255);
    pal->bar_cyan = _SitVDStandbyColorRGBA(0, 192, 192, 255);
    pal->bar_green = _SitVDStandbyColorRGBA(0, 192, 0, 255);
    pal->bar_magenta = _SitVDStandbyColorRGBA(192, 0, 192, 255);
    pal->bar_red = _SitVDStandbyColorRGBA(192, 0, 0, 255);
    pal->bar_blue = _SitVDStandbyColorRGBA(0, 0, 192, 255);
    pal->bar_black = _SitVDStandbyColorRGBA(0, 0, 0, 255);
    pal->bar_white = _SitVDStandbyColorRGBA(255, 255, 255, 255);
    pal->bar_mid_gray = _SitVDStandbyColorRGBA(128, 128, 128, 255);
    pal->bar_dark_gray = _SitVDStandbyColorRGBA(64, 64, 64, 255);
    pal->bar_orange = _SitVDStandbyColorRGBA(208, 132, 45, 255);
    pal->pluge_minus4 = _SitVDStandbyColorRGBA(10, 10, 10, 255);
    pal->pluge_zero = _SitVDStandbyColorRGBA(16, 16, 16, 255);
    pal->pluge_plus4 = _SitVDStandbyColorRGBA(20, 20, 20, 255);
    pal->pluge_plus75 = _SitVDStandbyColorRGBA(30, 30, 30, 255);
}

static void _SitVDStandbyInitDefaultStack(SitVdStandbyConfig* cfg) {
    if (!cfg) return;
    static const uint8_t kDefault[SIT_VD_STANDBY_LAYER_COUNT] = SIT_VD_STANDBY_DEFAULT_STACK;
    memcpy(cfg->layer_stack, kDefault, sizeof(kDefault));
    cfg->layer_stack_count = SIT_VD_STANDBY_DEFAULT_STACK_COUNT;
}

static int _SitVDStandbyStackContains(const SitVdStandbyConfig* cfg, uint8_t layer) {
    if (!cfg) return 0;
    for (uint8_t i = 0; i < cfg->layer_stack_count && i < SIT_VD_STANDBY_LAYER_COUNT; ++i) {
        if (cfg->layer_stack[i] == layer) {
            return 1;
        }
    }
    return 0;
}

static void _SitVDStandbyStackAppend(SitVdStandbyConfig* cfg, uint8_t layer) {
    if (!cfg || layer > 8) return;
    if (_SitVDStandbyStackContains(cfg, layer)) return;
    if (cfg->layer_stack_count >= SIT_VD_STANDBY_LAYER_COUNT) return;
    cfg->layer_stack[cfg->layer_stack_count++] = layer;
}

static void _SitVDStandbyStackRemove(SitVdStandbyConfig* cfg, uint8_t layer) {
    if (!cfg) return;
    uint8_t w = 0;
    for (uint8_t r = 0; r < cfg->layer_stack_count && r < SIT_VD_STANDBY_LAYER_COUNT; ++r) {
        if (cfg->layer_stack[r] != layer) {
            cfg->layer_stack[w++] = cfg->layer_stack[r];
        }
    }
    cfg->layer_stack_count = w;
    while (w < SIT_VD_STANDBY_LAYER_COUNT) {
        cfg->layer_stack[w++] = SIT_VD_STANDBY_STACK_UNUSED;
    }
}

static void _SitVDStandbyInitLayerParamsDefaults(
    SitVdStandbyLayerParams* layer,
    const SitVdStandbyPalette* pal)
{
    if (!layer || !pal) return;
    memset(layer, 0, sizeof(*layer));

    layer->smpte.content_margin_x = 0.1f;
    layer->smpte.content_margin_y = 0.1f;
    layer->smpte.show_overlay_circle = 1;
    layer->smpte.overlay_circle_radius = 0.0f;

    layer->checker.tile_size_x = 32.0f;
    layer->checker.tile_size_y = 32.0f;
    layer->checker.color_a = pal->bar_white;
    layer->checker.color_b = pal->bar_black;

    layer->convergence.stripe_width = 16.0f;
    layer->convergence.central_inset_x = 0.25f;
    layer->convergence.central_inset_y = 0.25f;
    layer->convergence.central_size_w = 0.5f;
    layer->convergence.central_size_h = 0.5f;
    layer->convergence.color_a = pal->bar_white;
    layer->convergence.color_b = pal->bar_black;

    layer->gradients.quad[0][0] = pal->bar_red;
    layer->gradients.quad[0][1] = pal->bar_green;
    layer->gradients.quad[0][2] = pal->bar_black;
    layer->gradients.quad[0][3] = pal->bar_black;
    layer->gradients.quad[1][0] = pal->bar_cyan;
    layer->gradients.quad[1][1] = pal->bar_magenta;
    layer->gradients.quad[1][2] = pal->bar_black;
    layer->gradients.quad[1][3] = pal->bar_black;
    layer->gradients.quad[2][0] = pal->bar_yellow;
    layer->gradients.quad[2][1] = pal->bar_blue;
    layer->gradients.quad[2][2] = pal->bar_black;
    layer->gradients.quad[2][3] = pal->bar_black;
    layer->gradients.quad[3][0] = pal->bar_white;
    layer->gradients.quad[3][1] = pal->bar_mid_gray;
    layer->gradients.quad[3][2] = pal->bar_black;
    layer->gradients.quad[3][3] = pal->bar_black;

    layer->grid.spacing_px = 0.0f;
    layer->grid.line_alpha = 100.0f / 255.0f;
    layer->grid.line_color = pal->grid_white;

    layer->pluge.safe_margin = 0.1f;
    layer->pluge.bar_count = 10;
    layer->pluge.bar_height_frac = 0.6f;

    layer->crosshatch.grid_nx = 16;
    layer->crosshatch.grid_ny = 12;
    layer->crosshatch.crosshair_size = 20.0f;
    layer->crosshatch.crosshair_thickness = 2.0f;
    layer->crosshatch.safe_margin = 0.1f;

    layer->multiburst.frequencies[0] = 0.5f;
    layer->multiburst.frequencies[1] = 1.0f;
    layer->multiburst.frequencies[2] = 2.0f;
    layer->multiburst.frequencies[3] = 3.0f;
    layer->multiburst.frequencies[4] = 4.0f;
    layer->multiburst.frequencies[5] = 5.0f;
    layer->multiburst.num_frequencies = 0;
    layer->multiburst.safe_margin = 0.1f;

    layer->cube.size = 1.0f;
    layer->cube.diffuse = pal->bar_white;
    layer->cube.ambient = 0.5f;
}

static void _SitVDStandbyConfigInitDefaults(
    SitVdStandbyConfig* cfg, int layer_index, float width, float height)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->width = width;
    cfg->height = height;
    cfg->pattern_layers = 0;
    if (layer_index >= 0 && layer_index <= 8) {
        cfg->pattern_layers = (int32_t)(1u << (uint32_t)layer_index);
    }
    _SitVDStandbyPaletteInitRglDefaults(&cfg->palette);
    _SitVDStandbyInitLayerParamsDefaults(&cfg->layer, &cfg->palette);
    _SitVDStandbyInitDefaultStack(cfg);
    cfg->snow.noise_frame_seed = 0.0f;
    cfg->snow.chroma = 0;
}

static uint32_t _SitVDStandbyLayerBit(int layer_index) {
    return 1u << (uint32_t)layer_index;
}

static void _SitVDStandbyToggleLayer(SitVdStandbyConfig* cfg, int layer_index, bool enabled) {
    if (!cfg || layer_index < 0 || layer_index > 8) return;
    uint32_t bit = _SitVDStandbyLayerBit(layer_index);
    if (enabled) {
        cfg->pattern_layers |= (int32_t)bit;
        _SitVDStandbyStackAppend(cfg, (uint8_t)layer_index);
    } else {
        cfg->pattern_layers &= (int32_t)~bit;
        _SitVDStandbyStackRemove(cfg, (uint8_t)layer_index);
    }
}

static void _SitVDStandbySetLayerOrder(
    SitVdStandbyConfig* cfg, const uint8_t* stack, uint8_t count)
{
    if (!cfg) return;
    memset(cfg->layer_stack, (int)SIT_VD_STANDBY_STACK_UNUSED, sizeof(cfg->layer_stack));
    cfg->layer_stack_count = 0;
    if (!stack || count == 0) {
        _SitVDStandbyInitDefaultStack(cfg);
        return;
    }
    if (count > SIT_VD_STANDBY_LAYER_COUNT) {
        count = SIT_VD_STANDBY_LAYER_COUNT;
    }
    memcpy(cfg->layer_stack, stack, count);
    cfg->layer_stack_count = count;
    for (uint8_t i = count; i < SIT_VD_STANDBY_LAYER_COUNT; ++i) {
        cfg->layer_stack[i] = SIT_VD_STANDBY_STACK_UNUSED;
    }
}

static void _SitVDPackColorVec4(uint8_t* out, ColorRGBA c) {
    float v[4] = {
        (float)c.r / 255.0f,
        (float)c.g / 255.0f,
        (float)c.b / 255.0f,
        (float)c.a / 255.0f
    };
    memcpy(out, v, sizeof(v));
}

static void _SitVDPackStackPacked(uint8_t* out_uvec4, const uint8_t stack[SIT_VD_STANDBY_LAYER_COUNT]) {
    uint32_t words[4] = {0u, 0u, 0u, 0u};
    for (int i = 0; i < 9; ++i) {
        words[i / 4] |= ((uint32_t)stack[i]) << (uint32_t)((i & 3) * 8);
    }
    memcpy(out_uvec4, words, sizeof(words));
}

static void _SitVDPackStandbyHeaderStd140(
    uint8_t out[SIT_VD_STANDBY_HEADER_UBO_SIZE], const SitVdStandbyConfig* cfg)
{
    if (!out || !cfg) return;
    memset(out, 0, SIT_VD_STANDBY_HEADER_UBO_SIZE);

    int32_t pattern_layers = cfg->pattern_layers;
    float width = cfg->width;
    float height = cfg->height;
    int32_t show_overlay = cfg->layer.smpte.show_overlay_circle;
    float noise_seed = cfg->snow.noise_frame_seed;
    int32_t stack_count = (int32_t)cfg->layer_stack_count;

    memcpy(out + SIT_VD_STANDBY_HEADER_UBO_OFF_PATTERN_LAYERS, &pattern_layers, sizeof(pattern_layers));
    memcpy(out + SIT_VD_STANDBY_HEADER_UBO_OFF_WIDTH, &width, sizeof(width));
    memcpy(out + SIT_VD_STANDBY_HEADER_UBO_OFF_HEIGHT, &height, sizeof(height));
    memcpy(out + SIT_VD_STANDBY_HEADER_UBO_OFF_SHOW_OVERLAY, &show_overlay, sizeof(show_overlay));
    memcpy(out + SIT_VD_STANDBY_HEADER_UBO_OFF_NOISE_FRAME_SEED, &noise_seed, sizeof(noise_seed));
    memcpy(out + SIT_VD_STANDBY_HEADER_UBO_OFF_STACK_COUNT, &stack_count, sizeof(stack_count));
    _SitVDPackStackPacked(out + SIT_VD_STANDBY_HEADER_UBO_OFF_STACK_PACKED, cfg->layer_stack);
}

static void _SitVDPackStandbyParamsStd430(
    uint8_t out[SIT_VD_STANDBY_PARAMS_SSBO_SIZE], const SitVdStandbyConfig* cfg)
{
    if (!out || !cfg) return;
    memset(out, 0, SIT_VD_STANDBY_PARAMS_SSBO_SIZE);

    const SitVdStandbyLayerParams* layer = &cfg->layer;
    const SitVdStandbyPalette* pal = &cfg->palette;

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_SMPTE, &layer->smpte, sizeof(layer->smpte));

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CHECKER, &layer->checker.tile_size_x, sizeof(float) * 2u);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CHECKER + 16u, layer->checker.color_a);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CHECKER + 32u, layer->checker.color_b);

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CONVERGENCE, &layer->convergence.stripe_width, sizeof(float) * 5u);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CONVERGENCE + 32u, layer->convergence.color_a);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CONVERGENCE + 48u, layer->convergence.color_b);

    for (int q = 0; q < 4; ++q) {
        for (int c = 0; c < 4; ++c) {
            uint32_t off = SIT_VD_STANDBY_PARAMS_SSBO_OFF_GRADIENTS
                + (uint32_t)(q * 4 + c) * 16u;
            _SitVDPackColorVec4(out + off, layer->gradients.quad[q][c]);
        }
    }

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_GRID, &layer->grid.spacing_px, sizeof(float) * 2u);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_GRID + 16u, layer->grid.line_color);

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PLUGE, &layer->pluge, sizeof(layer->pluge));
    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CROSSHATCH, &layer->crosshatch, sizeof(layer->crosshatch));
    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_MULTIBURST, layer->multiburst.frequencies, sizeof(layer->multiburst.frequencies));
    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_MULTIBURST + 24u, &layer->multiburst.num_frequencies, sizeof(layer->multiburst.num_frequencies));
    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_MULTIBURST + 28u, &layer->multiburst.safe_margin, sizeof(layer->multiburst.safe_margin));

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CUBE, &layer->cube.size, sizeof(layer->cube.size));
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CUBE + 16u, layer->cube.diffuse);
    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CUBE + 32u, &layer->cube.ambient, sizeof(layer->cube.ambient));

    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_SNOW, &cfg->snow.noise_frame_seed, sizeof(cfg->snow.noise_frame_seed));
    memcpy(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_SNOW + 4u, &cfg->snow.chroma, sizeof(cfg->snow.chroma));

    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 0u, pal->bg_dark_gray);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 16u, pal->grid_white);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 32u, pal->bar_light_gray);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 48u, pal->bar_yellow);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 64u, pal->bar_cyan);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 80u, pal->bar_green);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 96u, pal->bar_magenta);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 112u, pal->bar_red);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 128u, pal->bar_blue);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 144u, pal->bar_black);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 160u, pal->bar_white);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 176u, pal->bar_mid_gray);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 192u, pal->bar_dark_gray);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 208u, pal->bar_orange);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 224u, pal->pluge_minus4);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 240u, pal->pluge_zero);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 256u, pal->pluge_plus4);
    _SitVDPackColorVec4(out + SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE + 272u, pal->pluge_plus75);
}

/** @deprecated Flat 144 B shim — forwards to P10 header pack (160 B). */
static void _SitVDPackStandbyConfigStd140(
    uint8_t out[SIT_VD_STANDBY_CONFIG_UBO_SIZE], const SitVdStandbyConfig* cfg)
{
    _SitVDPackStandbyHeaderStd140(out, cfg);
}

SITAPI void SituationVdStandbyConfigInitDefaults(
    SitVdStandbyConfig* cfg, int layer_index, float width, float height)
{
    _SitVDStandbyConfigInitDefaults(cfg, layer_index, width, height);
}

SITAPI uint32_t SituationVdStandbyLayerBit(int layer_index) {
    return _SitVDStandbyLayerBit(layer_index);
}

SITAPI void SituationVdStandbyToggleLayer(SitVdStandbyConfig* cfg, int layer_index, bool enabled) {
    _SitVDStandbyToggleLayer(cfg, layer_index, enabled);
}

SITAPI void SituationVdStandbySetLayerOrder(
    SitVdStandbyConfig* cfg, const uint8_t* stack, uint8_t count)
{
    _SitVDStandbySetLayerOrder(cfg, stack, count);
}

SITAPI void SituationVdStandbyPackStd140(
    uint8_t out[SIT_VD_STANDBY_CONFIG_UBO_SIZE], const SitVdStandbyConfig* cfg)
{
    _SitVDPackStandbyHeaderStd140(out, cfg);
}

SITAPI void SituationVdStandbyPackParamsStd430(
    uint8_t out[SIT_VD_STANDBY_PARAMS_SSBO_SIZE], const SitVdStandbyConfig* cfg)
{
    _SitVDPackStandbyParamsStd430(out, cfg);
}

#endif /* SITUATION_IMPL_VD_STANDBY_H */
