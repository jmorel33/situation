/***************************************************************************************************
*
*   situation_api_types_gpu.h - GPU Resource and Render-Pipeline Types
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Render passes, attachments, barriers, virtual-display descriptors, raster/camera/mesh
*   typedefs, texture/sampler enums, and P2.1 graphics semantic types for GL+VK backends.
*
*   Depends on situation_api_types_system.h (included below).
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_TYPES_GPU_H
#define SITUATION_API_TYPES_GPU_H

#include "situation_api_config.h"
#include "situation_api_types_system.h"
#include "situation_base_types.h"

// ---------------------------------------------------------------------------------
//  Buffer Usage Flags (Critical for backend memory optimisation)
//  These flags are translated directly to VkBufferUsageFlags / GL buffer usage hints.
//  Always specify the minimal set required - the backend will place the buffer in the fastest
//  memory type possible based on these hints.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BUFFER_USAGE_VERTEX_BUFFER     = 1 << 0,   // Source of vertex data
    SITUATION_BUFFER_USAGE_INDEX_BUFFER      = 1 << 1,   // Source of index data
    SITUATION_BUFFER_USAGE_UNIFORM_BUFFER    = 1 << 2,   // Uniform Buffer Object (constant data, frequently updated)
    SITUATION_BUFFER_USAGE_STORAGE_BUFFER    = 1 << 3,   // Shader Storage Buffer Object (read/write in shaders)
    SITUATION_BUFFER_USAGE_INDIRECT_BUFFER   = 1 << 4,   // Indirect draw/dispatch command buffer
    SITUATION_BUFFER_USAGE_TRANSFER_SRC      = 1 << 5,   // Source for copy operations (CPU → GPU staging)
    SITUATION_BUFFER_USAGE_TRANSFER_DST      = 1 << 6,   // Destination for copy operations (GPU → CPU readback)
    SITUATION_BUFFER_USAGE_DEVICE_ADDRESS    = 1 << 7,   // Buffer can be accessed via device address (for buffer references)

    // Common combination presets (use these for convenience and maximum performance)
    SITUATION_BUFFER_USAGE_VERTEX_AND_STORAGE = SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
    SITUATION_BUFFER_USAGE_DYNAMIC_VERTEX = SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
    SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM = SITUATION_BUFFER_USAGE_UNIFORM_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
    SITUATION_BUFFER_USAGE_STORAGE_COMPUTE = SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC | SITUATION_BUFFER_USAGE_TRANSFER_DST | SITUATION_BUFFER_USAGE_DEVICE_ADDRESS,
} SituationBufferUsageFlags;
/**
 * @brief Specifies the color encoding of image data.
 *
 * This enum describes whether pixel data is stored in linear or SRGB color space.
 * The encoding affects how the data should be interpreted when creating GPU textures.
 *
 * **SITUATION_COLOR_LINEAR:**
 * - Data is in linear color space with no gamma encoding
 * - Required for storage images (textures writable by compute shaders)
 * - No automatic gamma correction applied during sampling
 * - Maps to UNORM formats:
 *   - Vulkan: VK_FORMAT_R8G8B8A8_UNORM
 *   - OpenGL: GL_RGBA8
 *
 * **SITUATION_COLOR_SRGB:**
 * - Data is in SRGB color space with gamma 2.2 encoding
 * - Preferred for sampled-only textures (photos, UI elements, etc.)
 * - Automatic gamma correction applied when sampled in shaders
 * - Cannot be used with storage images on most GPUs
 * - Maps to SRGB formats:
 *   - Vulkan: VK_FORMAT_R8G8B8A8_SRGB
 *   - OpenGL: GL_SRGB8_ALPHA8
 *
 * @note When creating textures with SITUATION_TEXTURE_USAGE_STORAGE flag, LINEAR encoding
 *       must be used. SRGB formats typically don't support storage image operations.
 * @note For sampled-only textures, SRGB encoding is preferred for proper gamma correction
 *       and color accuracy on standard displays.
 *
 * @since v2.3.40
 */
typedef enum SituationColorEncoding {
    SITUATION_COLOR_LINEAR = 0,     // Linear color space - required for storage images (both OpenGL and Vulkan)
    SITUATION_COLOR_SRGB = 1        // SRGB color space with gamma encoding - for sampled textures (both OpenGL and Vulkan)
} SituationColorEncoding;
/**
 * @brief Renderer Abstraction (legacy enum — prefer SituationGraphicsBackend + SituationGetGraphicsBackend).
 */
typedef enum {
    SIT_RENDERER_OPENGL,
    SIT_RENDERER_VULKAN
} SituationRendererType;

/**
 * @brief Defines a set of common, pre-configured layouts for compute pipelines.
 * @details This enum is passed to SituationCreateComputePipeline* to select an appropriate
 *          VkPipelineLayout that matches the resources declared in the compute shader.
 */
typedef enum {
    SIT_COMPUTE_LAYOUT_ONE_SSBO,                    // A layout for shaders that use one Shader Storage Buffer Object (SSBO) at set 0.
    SIT_COMPUTE_LAYOUT_TWO_SSBOS,                   // A layout for shaders that use two SSBOs at sets 0 and 1.
    SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO,              // A layout for shaders that use one Storage Image at set 0 and one SSBO at set 1.
    SIT_COMPUTE_LAYOUT_PUSH_CONSTANT,               // A layout for shaders that use a 64-byte push constant for small data.
    SIT_COMPUTE_LAYOUT_EMPTY,                       // A layout for simple shaders that take no external resources.
    SIT_COMPUTE_LAYOUT_BUFFER_IMAGE,                // A layout for shaders that use one SSBO (Set 0) and one Storage Image (Set 1).
    SIT_COMPUTE_LAYOUT_GRID,                        // Cell SSBO + storage image + font sampler + overlay sampler (terminal/grid).
    SIT_COMPUTE_LAYOUT_VECTOR,
} SituationComputeLayoutType;

/** @deprecated Use SIT_COMPUTE_LAYOUT_GRID — numeric alias retained for K-Term and wrappers. */
#ifndef SIT_COMPUTE_LAYOUT_TERMINAL
#define SIT_COMPUTE_LAYOUT_TERMINAL SIT_COMPUTE_LAYOUT_GRID
#endif

/**
 * @brief Descriptor pipeline layout for `SituationLoadShaderFromSpirvMemoryEx` (Vulkan graphics).
 * @details **Vulkan:** selects a pre-defined `VkPipelineLayout` matching harness/custom SPIR-V descriptor sets.
 *          **OpenGL:** ignored; load path is unchanged from `SituationLoadShaderFromSpirvMemory`.
 */
typedef enum SituationSpirvLayoutProfile {
    SIT_SPIRV_LAYOUT_PROFILE_MESH = 0,       /**< Default: set 0 dynamic UBO, set 1 sampler (same as `SituationLoadShaderFromSpirvMemory`). */
    SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO,      /**< Set 0 + set 1 storage buffers @ binding 0 each. */
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO,         /**< Set 0 uniform buffer + set 1 storage buffer @ binding 0. */
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER, /**< Set 0 UBO, set 1 SSBO, set 2 combined image sampler (fragment). */
} SituationSpirvLayoutProfile;

/**
 * @brief Flags for texture creation (used in SituationCreateTextureEx)
 */
typedef enum {
    SITUATION_TEXTURE_USAGE_SAMPLED         = 1 << 0, // Standard texture
    SITUATION_TEXTURE_USAGE_STORAGE         = 1 << 1, // Can be used with imageStore/Compute
    SITUATION_TEXTURE_USAGE_TRANSFER_SRC    = 1 << 2, // Can be copied from
    SITUATION_TEXTURE_USAGE_TRANSFER_DST    = 1 << 3, // Can be copied to
    SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED = 1 << 4  // Will be sampled (read-only) in compute shaders
} SituationTextureUsageFlags;

/**
 * @brief Texture formats
 */
typedef enum {
    SIT_TEXTURE_FORMAT_UNKNOWN = 0,
    SIT_TEXTURE_FORMAT_RGBA8_UNORM,
    SIT_TEXTURE_FORMAT_RGBA8_SRGB,
} SituationTextureFormat;

/**
 * @brief Texture filters
 */
typedef enum {
    SIT_TEXTURE_FILTER_NEAREST = 0,
    SIT_TEXTURE_FILTER_LINEAR,
} SituationTextureFilter;

/**
 * @brief Texture wrap modes
 */
typedef enum {
    SIT_TEXTURE_WRAP_CLAMP_TO_EDGE = 0,
    SIT_TEXTURE_WRAP_REPEAT,
} SituationTextureWrap;

/**
 * @brief Texture Information
 */
typedef struct {
    int width;
    int height;
    int mip_levels;
    SituationTextureFormat format;
    SituationTextureUsageFlags usage_flags;
    SituationTextureFilter min_filter;
    SituationTextureFilter mag_filter;
    SituationTextureWrap wrap_s;
    SituationTextureWrap wrap_t;
} SituationTextureInfo;

/**
 * @brief Texture Readback Format
 */
typedef enum {
    SIT_TEXTURE_READ_RGBA8 = 0,       /* normalized RGBA bytes, backend-independent */
    SIT_TEXTURE_READ_RGB10_PACKED = 1 /* raw A2R10G10B10 uint32 texels (Vulkan 10-bit swapchain) */
} SituationTextureReadFormat;

/**
 * @brief Texture Region
 */
typedef struct {
    int x, y;
    int width, height;
    int mip_level; /* 0 for base */
} SituationTextureRegion;

/**
 * @brief Texture blit filter mode.
 */
typedef enum {
    SITUATION_BLIT_FILTER_NEAREST = 0,
    SITUATION_BLIT_FILTER_LINEAR = 1
} SituationBlitFilter;

/** @brief Phase 14 Slice 1 — transfer usage validation for copy/blit commands. */
typedef enum {
    SIT_TRANSFER_USAGE_STRICT = 0,
    SIT_TRANSFER_USAGE_COMPATIBLE_FALLBACK
} SituationTransferUsagePolicy;

/** @brief Phase 14 — texture layout handling around transfer commands. */
typedef enum {
    SIT_TEXTURE_LAYOUT_EXPLICIT = 0,
    SIT_TEXTURE_LAYOUT_ASSISTED
} SituationTextureLayoutPolicy;

/** @brief Phase 14 Slice 1 — blit filter validation/fallback. */
typedef enum {
    SIT_BLIT_FILTER_STRICT = 0,
    SIT_BLIT_FILTER_DOWNGRADE_NEAREST
} SituationBlitFilterPolicy;

/** @brief Phase 14 — coordinate conventions (reserved strict in v2.4). */
typedef enum {
    SIT_COORDINATE_STRICT = 0
} SituationCoordinatePolicy;

/** @brief Phase 14 — validation tone for opt-in policy fallbacks. */
typedef enum {
    SIT_RENDERER_VALIDATION_STRICT = 0,
    SIT_RENDERER_VALIDATION_WARN,
    SIT_RENDERER_VALIDATION_COMPAT
} SituationRendererValidationPolicy;

/**
 * @brief Command-buffer renderer behavior policy (Phase 14).
 *
 * @details Default (`SituationRendererBehaviorPolicyDefault`) is strict on every axis.
 *          Opt-in convenience is scoped with Set/Push/PopRendererBehavior commands.
 */
typedef struct SituationRendererBehaviorPolicy {
    SituationTransferUsagePolicy transfer_usage;
    SituationTextureLayoutPolicy texture_layout;
    SituationBlitFilterPolicy blit_filter;
    SituationCoordinatePolicy coordinate;
    SituationRendererValidationPolicy validation;
} SituationRendererBehaviorPolicy;

/**
 * @brief 2D texture rectangle in Situation API space.
 *
 * @details Origin is top-left, `y` increases downward, and no backend-specific
 *          implicit flip is applied by blit commands.
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
} SituationTextureRect;

/**
 * @brief Texture-to-texture blit region.
 *
 * @details First implementation slice is color-only 2D textures, mip 0+,
 *          layer 0, with explicit caller-owned texture barriers before and
 *          after the blit.
 */
typedef struct {
    SituationTextureRect src_rect;
    SituationTextureRect dst_rect;
    uint32_t src_mip_level;
    uint32_t dst_mip_level;
    uint32_t src_array_layer;
    uint32_t dst_array_layer;
    SituationBlitFilter filter;
} SituationTextureBlitRegion;

/**
 * @brief Texture-to-texture copy region (exact-size transfer).
 *
 * @details Texture-to-texture: copies `src_rect` to `(dst_x, dst_y)` on the destination mip.
 *          Buffer-to-texture: `src_rect` supplies width/height only (`x`/`y` must be 0); data is read
 *          tightly packed RGBA8 rows from the source buffer offset.
 *          Texture-to-buffer: `src_rect` selects the texture subregion; `dst_x`/`dst_y` are unused.
 *          Copy does not scale (use blit for that). First slice: color-only 2D textures, layer 0,
 *          explicit caller-owned barriers.
 */
typedef struct {
    SituationTextureRect src_rect;
    int dst_x;
    int dst_y;
    uint32_t src_mip_level;
    uint32_t dst_mip_level;
    uint32_t src_array_layer;
    uint32_t dst_array_layer;
} SituationTextureCopyRegion;

/**
 * @brief Readback description for textures
 */
typedef struct {
    SituationTextureRegion region;       /* mip_level 0 unless explicitly supported */
    SituationTextureReadFormat format;   /* default SIT_TEXTURE_READ_RGBA8 */
    size_t dst_row_pitch_bytes;          /* 0 = tightly packed width * 4 */
} SituationTextureReadbackDesc;

/**
 * @brief Readback description for framebuffers
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
    SituationTextureReadFormat format; /* default SIT_TEXTURE_READ_RGBA8 */
    size_t dst_row_pitch_bytes;        /* 0 = tightly packed width * 4 */
} SituationReadPixelsDesc;
#ifdef SITUATION_USE_VULKAN
typedef VkCommandBuffer SituationCommandBuffer;
#else
typedef struct SituationCommandBuffer_t* SituationCommandBuffer;
#endif

/**
 * @brief Defines the color blending mode for a virtual display during compositing.
 * @details These modes determine how a virtual display's texture is drawn onto the main framebuffer.
 */
typedef enum {
    // --- Standard & Simple Modes ---
    SITUATION_BLEND_ALPHA,                          // Default alpha blending. Final = Src * SrcAlpha + Dst * (1 - SrcAlpha). Ideal for UI.
    SITUATION_BLEND_ADDITIVE,                       // Brightening blend (Src + Dst). Black is transparent. Good for glows, sparks.
    SITUATION_BLEND_MULTIPLY,                       // Darkening blend (Src * Dst). White is transparent. Good for shadows, tinting.
    SITUATION_BLEND_SCREEN,                         // Brightening blend, less harsh than additive. Inverts, multiplies, and inverts again.
    SITUATION_BLEND_NONE,                           // Opaque blend (Final = Src). Ignores alpha and overwrites destination.

    // --- Photoshop-Style Blend Modes (require custom shader) ---
    SITUATION_BLEND_OVERLAY,                        // Combines Multiply and Screen. Preserves highlights and shadows of the destination.
    SITUATION_BLEND_SOFT_LIGHT,                     // Darkens or lightens, depending on source color. A softer version of Hard Light.
    SITUATION_BLEND_HARD_LIGHT,                     // Combines Multiply and Screen based on source color. A harsher version of Overlay.
    SITUATION_BLEND_COLOR_DODGE,                    // Brightens the destination color to reflect the source color.
    SITUATION_BLEND_COLOR_BURN,                     // Darkens the destination color to reflect the source color.
    SITUATION_BLEND_DARKEN,                         // Selects the darker of the source and destination pixels.
    SITUATION_BLEND_LIGHTEN,                        // Selects the lighter of the source and destination pixels.
    SITUATION_BLEND_DIFFERENCE,                     // Subtracts the darker color from the lighter color. Black shows no change.
    SITUATION_BLEND_EXCLUSION,                      // Similar to Difference but with lower contrast.
} SituationBlendMode;

/**
 * @brief Defines the scaling and filtering method for a virtual display.
 */
typedef enum {
    // @brief Smoothly stretches the VD to fill its defined rectangle (via offset/resolution).
    // Ignores aspect ratio. Uses GL_LINEAR filtering (blurry). Good for high-res UI.
    SITUATION_SCALING_STRETCH,

    // @brief Sharp, aspect-correct scaling that fills the screen as much as possible.
    // Uses GL_NEAREST filtering (sharp). This is your requested "sharp stretch" mode.
    // This will leave minimal black bars (letterbox/pillarbox).
    SITUATION_SCALING_FIT,

    // @brief Sharp, aspect-correct, integer-only scaling.
    // Guarantees all game pixels are perfect squares on screen, but may leave larger black bars.
    // Uses GL_NEAREST filtering (sharp). This is the "pixel perfect" purist mode.
    SITUATION_SCALING_INTEGER

} SituationScalingMode;
/**
 * @brief Flags for virtual display creation (used with SituationCreateVirtualDisplayEx).
 */
typedef enum {
    SITUATION_VD_FLAG_NONE           = 0,
    SITUATION_VD_FLAG_COMPUTE_TARGET = 1 << 0, // VD texture is writable by compute shaders (adds STORAGE usage, skips depth/render pass)
} SituationVDFlags;

/** Attachment MSAA quality tier (VD-4b). Values > OFF require resolve before composite. */
typedef enum SituationMultisampleQuality {
    SITUATION_MULTISAMPLE_OFF = 0,
    SITUATION_MULTISAMPLE_2X,
    SITUATION_MULTISAMPLE_4X,   /* default "good"; aligns with SITUATION_FLAG_MSAA_4X_HINT */
    SITUATION_MULTISAMPLE_8X,
    SITUATION_MULTISAMPLE_16X,
    /* Future: SITUATION_MULTISAMPLE_ADAPTIVE, etc. */
} SituationMultisampleQuality;

#define SITUATION_MULTISAMPLE_DEFAULT SITUATION_MULTISAMPLE_4X

static inline int SituationMultisampleQualitySampleCount(SituationMultisampleQuality q) {
    static const int k_counts[] = { 1, 2, 4, 8, 16 };
    if ((unsigned)q >= (sizeof(k_counts) / sizeof(k_counts[0]))) {
        return 1;
    }
    return k_counts[q];
}

static inline SituationMultisampleQuality SituationMultisampleQualityFromSampleCount(int samples) {
    switch (samples) {
        case 2:  return SITUATION_MULTISAMPLE_2X;
        case 4:  return SITUATION_MULTISAMPLE_4X;
        case 8:  return SITUATION_MULTISAMPLE_8X;
        case 16: return SITUATION_MULTISAMPLE_16X;
        default: return SITUATION_MULTISAMPLE_OFF;
    }
}

/** Clamp to highest supported PO2 tier <= max_samples (VD-4b create/configure). */
static inline SituationMultisampleQuality SituationMultisampleQualityClamp(SituationMultisampleQuality q, int max_samples) {
    if (max_samples < 2) {
        return SITUATION_MULTISAMPLE_OFF;
    }
    int n = SituationMultisampleQualitySampleCount(q);
    if (n <= max_samples) {
        return q;
    }
    if (max_samples >= 16) return SITUATION_MULTISAMPLE_16X;
    if (max_samples >= 8)  return SITUATION_MULTISAMPLE_8X;
    if (max_samples >= 4)  return SITUATION_MULTISAMPLE_4X;
    if (max_samples >= 2)  return SITUATION_MULTISAMPLE_2X;
    return SITUATION_MULTISAMPLE_OFF;
}

/** Virtual display color attachment format (VD-1: UNORM + enum for SRGB; full sRGB path in VD-2). */
typedef enum SituationVirtualDisplayColorFormat {
    SIT_VD_FORMAT_RGBA8_UNORM = 0,
    SIT_VD_FORMAT_RGBA8_SRGB  = 1
} SituationVirtualDisplayColorFormat;

/** Virtual display depth/stencil attachment mode (D24S8 deferred — enum only until follow-up patch). */
typedef enum SituationVirtualDisplayDepthStencilMode {
    SIT_VD_DEPTH_NONE  = 0,
    SIT_VD_DEPTH_D24   = 1,
    SIT_VD_DEPTH_D24S8 = 2
} SituationVirtualDisplayDepthStencilMode;

/** Composite sampler mip filter (VD-3). */
typedef enum SituationVDMipFilterMode {
    SIT_VD_MIP_FILTER_NEAREST = 0,
    SIT_VD_MIP_FILTER_LINEAR  = 1
} SituationVDMipFilterMode;

/** VD refresh policy (VD-5): dynamic advances frame clock; static is dirty-driven only. */
typedef enum SituationVirtualDisplayUpdateMode {
    SIT_VD_UPDATE_DYNAMIC = 0,
    SIT_VD_UPDATE_STATIC  = 1
} SituationVirtualDisplayUpdateMode;

/** VD allocation preference (VD-5); best-effort on create, no-op when unsupported. */
typedef enum SituationVirtualDisplayMemoryHint {
    SIT_VD_MEMORY_DEFAULT        = 0,
    SIT_VD_MEMORY_PREFER_SPEED   = 1,
    SIT_VD_MEMORY_PREFER_QUALITY = 2
} SituationVirtualDisplayMemoryHint;

/** Explicit composite sampler (VD-3); decoupled from SituationScalingMode layout. */
typedef struct SituationVirtualDisplaySamplerDesc {
    SituationTextureFilter   min_filter;
    SituationTextureFilter   mag_filter;
    SituationVDMipFilterMode mip_filter;
    SituationTextureWrap     wrap_u;
    SituationTextureWrap     wrap_t;
    float                    max_anisotropy;   /* 1.f = off */
    uint32_t                 max_mip_level;    /* LOD clamp when compositing */
} SituationVirtualDisplaySamplerDesc;

/** Compositor idle fallback when no content writes exceed idle_threshold_seconds (Phase 2a). */
typedef enum SituationVDFallbackMode {
    SITUATION_VD_FALLBACK_SOLID = 0,       /**< Flat fallback_color */
    SITUATION_VD_FALLBACK_COLORBURST = 1,   /**< SMPTE EG 1-1990 bars (2/3 main + castellation + PLUGE/I/Q) */
    SITUATION_VD_FALLBACK_PATTERN = 2       /**< Layer bitmask standby — plan §3.4; Vulkan compositor today */
} SituationVDFallbackMode;

/**
 * @brief Specifies how an attachment's contents should be treated at the start of a render pass.
 *
 * Applied by `SituationCmdBeginRenderPass` through `SituationRenderPassInfo`:
 * - **OpenGL:** `SIT_LOAD_OP_CLEAR` issues `glClear` for the attachment aspect; `LOAD` preserves
 *   the bound framebuffer; `DONT_CARE` skips the clear (contents undefined).
 * - **Vulkan:** maps to `VkAttachmentLoadOp` / `VkAttachmentLoadOp` (stencil aspect on the depth
 *   attachment). Clear values come from the matching `SituationAttachmentInfo.clear` field.
 *
 * For mid-pass clears inside an already active pass, use `SituationCmdClear*` instead.
 */
typedef enum {
    SIT_LOAD_OP_LOAD,       // Preserve the existing contents of the attachment.
    SIT_LOAD_OP_CLEAR,      // Clear the attachment to a specified value.
    SIT_LOAD_OP_DONT_CARE   // The existing contents are undefined and can be discarded.
} SituationAttachmentLoadOp;

/**
 * @brief Specifies how an attachment's contents should be treated at the end of a render pass.
 *
 * `SIT_STORE_OP_STORE` keeps the attachment for sampling, present, or a later pass.
 * `SIT_STORE_OP_DONT_CARE` allows the backend to discard the attachment after the pass
 * (typical for transient depth on the main window).
 */
typedef enum {
    SIT_STORE_OP_STORE,     // The rendered contents will be stored in memory for later access.
    SIT_STORE_OP_DONT_CARE  // The rendered contents are not needed after the pass and can be discarded.
} SituationAttachmentStoreOp;

/**
 * @brief Clear values supplied when a begin-pass load op is `SIT_LOAD_OP_CLEAR`.
 *
 * Each attachment reads only the fields relevant to its aspect (`color`, `depth`, `stencil`).
 * Color components are 0–255 (`ColorRGBA`). Depth is normalized 0.0–1.0. Stencil is an integer mask
 * value passed to the backend when stencil aspects are supported on the active target.
 */
typedef struct {
    ColorRGBA color;
    float     depth;
    uint32_t  stencil;
} SituationClearValue;

/** Mid-pass / attachment clear: color only (depth/stencil zero). */
static inline SituationClearValue SituationClearValueColor(ColorRGBA color) {
    SituationClearValue v = {0};
    v.color = color;
    return v;
}

/** Mid-pass / attachment clear: depth only (default 1.0 when using `{0}` init). */
static inline SituationClearValue SituationClearValueDepth(float depth) {
    SituationClearValue v = {0};
    v.depth = depth;
    return v;
}

/** Tier B default load/store/clear for attachment inherit helpers (VD-1). */
typedef struct SituationVirtualDisplayAttachmentDefaults {
    SituationAttachmentLoadOp  color_load;
    SituationAttachmentStoreOp color_store;
    SituationAttachmentLoadOp  depth_load;
    SituationAttachmentStoreOp depth_store;
    SituationAttachmentLoadOp  stencil_load;
    SituationAttachmentStoreOp stencil_store;
    SituationClearValue        clear;
} SituationVirtualDisplayAttachmentDefaults;

/** Desc-struct virtual display creation (VD-1). Legacy Create/CreateEx wrap this with defaults. */
typedef struct SituationVirtualDisplayDesc {
    Vector2 resolution;
    SituationVirtualDisplayColorFormat      color_format;
    SituationVirtualDisplayDepthStencilMode depth_stencil_mode;
    SituationVirtualDisplayAttachmentDefaults attachments;
    int                              msaa_samples;       /* legacy int; mapped to msaa_quality at create. v2.4: must be 1 until VD-4b */
    uint32_t                         color_mip_levels;   /* storage mips to allocate (VD-4a) */
    SituationVirtualDisplaySamplerDesc composite_sampler;
    Vector2              offset;
    float                opacity;
    int                  z_order;
    bool                 visible;
    SituationScalingMode scaling_mode;
    SituationBlendMode   blend_mode;
    SituationVirtualDisplayUpdateMode update_mode;
    SituationVirtualDisplayMemoryHint   memory_hint;
    SituationVDFlags     flags;
    double               frame_time_mult;
} SituationVirtualDisplayDesc;

static inline SituationVirtualDisplaySamplerDesc SituationVirtualDisplaySamplerDescDefault(void) {
    SituationVirtualDisplaySamplerDesc s = {0};
    s.min_filter = SIT_TEXTURE_FILTER_NEAREST;
    s.mag_filter = SIT_TEXTURE_FILTER_NEAREST;
    s.mip_filter = SIT_VD_MIP_FILTER_NEAREST;
    s.wrap_u = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
    s.wrap_t = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
    s.max_anisotropy = 1.0f;
    s.max_mip_level = 0u;
    return s;
}

/** @brief Attachment bits used by SituationCmdClear. */
typedef enum {
    SIT_CLEAR_COLOR_BIT   = 0x1,
    SIT_CLEAR_DEPTH_BIT   = 0x2,
    SIT_CLEAR_STENCIL_BIT = 0x4
} SituationClearFlags;

/** @brief Buffer layout consumed by SituationCmdDispatchIndirect. */
typedef struct {
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
} SituationDispatchIndirectCommand;

/** @brief Buffer layout consumed by SituationCmdDrawIndirect (matches VkDrawIndirectCommand / GL draw arrays indirect). */
typedef struct {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
} SituationDrawIndirectCommand;

/** @brief Buffer layout consumed by SituationCmdDrawIndexedIndirect (matches VkDrawIndexedIndirectCommand / GL draw elements indirect). */
typedef struct {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
} SituationDrawIndexedIndirectCommand;

/** @brief Load/store/clear configuration for one render-pass attachment aspect. */
typedef struct {
    SituationAttachmentLoadOp  loadOp;
    SituationAttachmentStoreOp storeOp;
    SituationClearValue        clear;
} SituationAttachmentInfo;

/**
 * @brief Complete configuration for `SituationCmdBeginRenderPass`.
 *
 * **Target:** `display_id == -1` renders to the main window swapchain; `display_id >= 0` renders
 * to a Virtual Display FBO/texture. When **`render_target`** is non-null, it overrides **`display_id`**
 * and routes to a user **`SituationRenderTarget`** (Phase 3c — no compositor).
 *
 * **Attachments:** Color, depth, and stencil are configured independently. On combined depth/stencil
 * surfaces, depth and stencil load/store ops map to the corresponding aspects of the same backend
 * attachment. Stencil begin-pass clears are only honored when the active target exposes stencil;
 * otherwise backends may ignore stencil load ops or return `SITUATION_ERROR_NOT_IMPLEMENTED` for
 * stencil-specific mid-pass clears.
 *
 * **Helpers:** `SituationRenderPassInfoDefault` (clear color+depth) and `SituationRenderPassInfoLoad`
 * (preserve all attachments) cover the two most common begin-pass patterns.
 */
typedef struct {
    int                     display_id;     // The render target (-1 for main window, >= 0 for a Virtual Display).
    SituationAttachmentInfo color_attachment;
    SituationAttachmentInfo depth_attachment;
    SituationAttachmentInfo stencil_attachment;
    SituationRenderTarget   render_target;  // When generation != 0, overrides display_id (user offscreen RT).
} SituationRenderPassInfo;

/** @brief Create-time configuration for `SituationCreateRenderTarget` (Phase 3c — single-sample only). */
typedef struct SituationRenderTargetDesc {
    int       width;
    int       height;
    int       msaa_samples;   /* Must be 1 until MSAA slice ships. */
    bool      want_depth;
    bool      want_stencil; /* Reserved — stencil skipped in v2.4.393. */
    ColorRGBA clear_color;  /* Default when pass loadOp is CLEAR; not a stored per-pass override. */
    float     clear_depth;
} SituationRenderTargetDesc;

// SAFETY: SituationBuffer handle identity is packed into a uint64_t (slot_index + generation = 8 bytes).
// Only slot_index and generation are used for handle lookup — size_in_bytes/usage_flags are metadata.
#define SIT_BUFFER_HANDLE_PACK_SIZE 8  // bytes: slot_index(4) + generation(4)
_Static_assert(
    offsetof(SituationBuffer, generation) + sizeof(uint32_t) == SIT_BUFFER_HANDLE_PACK_SIZE,
    "SituationBuffer handle fields (slot_index + generation) must be the first 8 bytes for command buffer packing"
);

// Forward-declaration for the internal uniform map implementation struct.
// The full definition is hidden inside the SITUATION_IMPLEMENTATION block.
struct _SituationUniformMap;

// Enums for the 'type' parameter
typedef enum {
    SIT_UNIFORM_FLOAT,
    SIT_UNIFORM_VEC2,
    SIT_UNIFORM_VEC3,
    SIT_UNIFORM_VEC4,
    SIT_UNIFORM_INT,
    SIT_UNIFORM_IVEC2,
    SIT_UNIFORM_IVEC3,
    SIT_UNIFORM_IVEC4,
    SIT_UNIFORM_MAT4
} SituationUniformType;
typedef struct SituationModelMesh {
    char name[SITUATION_MAX_DEVICE_NAME_LEN]; // Name of the mesh from the model file
    SituationMesh gpu_mesh;                   // The handle to the GPU vertex/index data

    // --- PBR Material Properties ---
    // These are loaded directly from the GLTF material definition.
    Vector4 base_color_factor;                // The base color tint (RGBA)
    float metallic_factor;                    // How metallic the surface is [0-1]
    float roughness_factor;                   // How rough the surface is [0-1]
    Vector3 emissive_factor;                  // The color of light emitted by the surface

    // --- Texture Handles ---
    // These point to textures that are also part of the model.
    SituationTexture base_color_texture;      // Albedo/Diffuse map
    SituationTexture metallic_roughness_texture; // Packed Metal (R), Rough (G) map
    SituationTexture normal_texture;          // Normal map
    SituationTexture occlusion_texture;       // Ambient Occlusion map
    SituationTexture emissive_texture;        // Emissive/Glow map
} SituationModelMesh;

/**
 * @brief Represents a complete 3D model, loaded from a file.
 * @details This is a container for all the meshes and materials that make up a model.
 *          It is the result of a call to SituationLoadModel.
 */
typedef struct SituationModel {
    uint32_t slot_index;
    uint32_t generation;
    int mesh_count;             // Cached metadata
    SituationModelMesh* meshes; // Pointer to meshes (valid until unloaded)
} SituationModel;
// --- Virtual Display Structures ---

/* VD idle standby pattern layers — mirrors sit/gpu/test_patterns/sit_tp_config.glslh. */

typedef enum SitVdStandbyLayer {
    SIT_VD_STANDBY_SMPTE_BARS = 0,
    SIT_VD_STANDBY_CHECKERBOARD = 1,
    SIT_VD_STANDBY_CONVERGENCE = 2,
    SIT_VD_STANDBY_GRADIENTS = 3,
    SIT_VD_STANDBY_GRID_ONLY = 4,
    SIT_VD_STANDBY_PLUGE = 5,
    SIT_VD_STANDBY_CROSSHATCH = 6,
    SIT_VD_STANDBY_MULTIBURST = 7,
    SIT_VD_STANDBY_3D_GRID = 8,
} SitVdStandbyLayer;

#define SIT_VD_STANDBY_LAYER_SMPTE        (1u << 0)
#define SIT_VD_STANDBY_LAYER_CHECKERBOARD (1u << 1)
#define SIT_VD_STANDBY_LAYER_CONVERGENCE  (1u << 2)
#define SIT_VD_STANDBY_LAYER_GRADIENTS    (1u << 3)
#define SIT_VD_STANDBY_LAYER_GRID         (1u << 4)
#define SIT_VD_STANDBY_LAYER_PLUGE        (1u << 5)
#define SIT_VD_STANDBY_LAYER_CROSSHATCH   (1u << 6)
#define SIT_VD_STANDBY_LAYER_MULTIBURST   (1u << 7)
#define SIT_VD_STANDBY_LAYER_3D_GRID      (1u << 8)

/** Calibration layers 0–8 only (excludes standby flags). */
#define SIT_VD_STANDBY_LAYER_CALIBRATION_MASK 0x1FFu
/** Chroma snow when no calibration layers are set (default off = B&W snow). */
#define SIT_VD_STANDBY_LAYER_CHROMA_SNOW      (1u << 16)

#define SIT_VD_STANDBY_LAYER_COUNT           9
#define SIT_VD_STANDBY_DEFAULT_STACK_COUNT   8
#define SIT_VD_STANDBY_STACK_UNUSED          0xFFu

/** std140 SitTpConfigBlock header (P10): frame + stack + seed. Layer tunables in SSBO. */
#define SIT_VD_STANDBY_HEADER_UBO_SIZE       160u
/** @deprecated Alias — use SIT_VD_STANDBY_HEADER_UBO_SIZE (was 144 B flat shim). */
#define SIT_VD_STANDBY_CONFIG_UBO_SIZE       SIT_VD_STANDBY_HEADER_UBO_SIZE

#define SIT_VD_STANDBY_HEADER_UBO_OFF_PATTERN_LAYERS   0u
#define SIT_VD_STANDBY_HEADER_UBO_OFF_WIDTH            4u
#define SIT_VD_STANDBY_HEADER_UBO_OFF_HEIGHT           8u
#define SIT_VD_STANDBY_HEADER_UBO_OFF_SHOW_OVERLAY     12u
#define SIT_VD_STANDBY_HEADER_UBO_OFF_NOISE_FRAME_SEED  136u
#define SIT_VD_STANDBY_HEADER_UBO_OFF_STACK_COUNT      140u
#define SIT_VD_STANDBY_HEADER_UBO_OFF_STACK_PACKED     144u

/** Legacy flat-shim offsets (harness readback compat — subset mirrored in header+SSBO). */
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_PATTERN_LAYERS   SIT_VD_STANDBY_HEADER_UBO_OFF_PATTERN_LAYERS
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_WIDTH            SIT_VD_STANDBY_HEADER_UBO_OFF_WIDTH
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_HEIGHT           SIT_VD_STANDBY_HEADER_UBO_OFF_HEIGHT
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_SHOW_OVERLAY     SIT_VD_STANDBY_HEADER_UBO_OFF_SHOW_OVERLAY
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_CHECKER_SIZE_X   16u
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_CHECKER_SIZE_Y   20u
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_STRIPE_WIDTH     24u
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_FREQUENCIES      32u
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_NUM_FREQUENCIES  128u
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_GRID_SIZE        132u
#define SIT_VD_STANDBY_CONFIG_UBO_OFF_NOISE_FRAME_SEED  SIT_VD_STANDBY_HEADER_UBO_OFF_NOISE_FRAME_SEED

/** std430 SitTpLayerParamsBlock — mirrors sit/gpu/test_patterns/sit_tp_layer_params_ssbo.glslh. */
#define SIT_VD_STANDBY_PARAMS_SSBO_SIZE                832u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_SMPTE           0u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_CHECKER         16u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_CONVERGENCE     64u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_GRADIENTS       128u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_GRID            384u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_PLUGE           416u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_CROSSHATCH      428u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_MULTIBURST      448u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_CUBE            480u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_SNOW            528u
#define SIT_VD_STANDBY_PARAMS_SSBO_OFF_PALETTE         544u

#define SIT_VD_STANDBY_DEFAULT_STACK \
    { 1, 2, 3, 5, 7, 6, 0, 4, SIT_VD_STANDBY_STACK_UNUSED }

typedef struct SitVdStandbySmpteParams {
    float content_margin_x;
    float content_margin_y;
    int32_t show_overlay_circle;
    float overlay_circle_radius; /* <= 0: content height * 0.5 */
} SitVdStandbySmpteParams;

typedef struct SitVdStandbyCheckerParams {
    float tile_size_x;
    float tile_size_y;
    ColorRGBA color_a;
    ColorRGBA color_b;
} SitVdStandbyCheckerParams;

typedef struct SitVdStandbyConvergenceParams {
    float stripe_width;
    float central_inset_x;
    float central_inset_y;
    float central_size_w;
    float central_size_h;
    ColorRGBA color_a;
    ColorRGBA color_b;
} SitVdStandbyConvergenceParams;

typedef struct SitVdStandbyGradientsParams {
    ColorRGBA quad[4][4]; /* [quadrant][corner TL,TR,BL,BR] */
} SitVdStandbyGradientsParams;

typedef struct SitVdStandbyGridParams {
    float spacing_px; /* <= 0: width / 32 */
    float line_alpha;
    ColorRGBA line_color;
} SitVdStandbyGridParams;

typedef struct SitVdStandbyPlugeParams {
    float safe_margin;
    int32_t bar_count;
    float bar_height_frac;
} SitVdStandbyPlugeParams;

typedef struct SitVdStandbyCrosshatchParams {
    int32_t grid_nx;
    int32_t grid_ny;
    float crosshair_size;
    float crosshair_thickness;
    float safe_margin;
} SitVdStandbyCrosshatchParams;

typedef struct SitVdStandbyMultiburstParams {
    float frequencies[6];
    int32_t num_frequencies;
    float safe_margin;
} SitVdStandbyMultiburstParams;

typedef struct SitVdStandbyCubeParams {
    float size;
    ColorRGBA diffuse;
    float ambient;
} SitVdStandbyCubeParams;

typedef struct SitVdStandbySnowParams {
    float noise_frame_seed;
    int32_t chroma;
} SitVdStandbySnowParams;

typedef struct SitVdStandbyPalette {
    ColorRGBA bg_dark_gray;
    ColorRGBA grid_white;
    ColorRGBA bar_light_gray;
    ColorRGBA bar_yellow;
    ColorRGBA bar_cyan;
    ColorRGBA bar_green;
    ColorRGBA bar_magenta;
    ColorRGBA bar_red;
    ColorRGBA bar_blue;
    ColorRGBA bar_black;
    ColorRGBA bar_white;
    ColorRGBA bar_mid_gray;
    ColorRGBA bar_dark_gray;
    ColorRGBA bar_orange;
    ColorRGBA pluge_minus4;
    ColorRGBA pluge_zero;
    ColorRGBA pluge_plus4;
    ColorRGBA pluge_plus75;
} SitVdStandbyPalette;

typedef struct SitVdStandbyLayerParams {
    SitVdStandbySmpteParams smpte;
    SitVdStandbyCheckerParams checker;
    SitVdStandbyConvergenceParams convergence;
    SitVdStandbyGradientsParams gradients;
    SitVdStandbyGridParams grid;
    SitVdStandbyPlugeParams pluge;
    SitVdStandbyCrosshatchParams crosshatch;
    SitVdStandbyMultiburstParams multiburst;
    SitVdStandbyCubeParams cube;
} SitVdStandbyLayerParams;

typedef struct SitVdStandbyConfig {
    int32_t pattern_layers;
    uint8_t layer_stack[SIT_VD_STANDBY_LAYER_COUNT];
    uint8_t layer_stack_count;
    float width;
    float height;
    SitVdStandbyPalette palette;
    SitVdStandbyLayerParams layer;
    SitVdStandbySnowParams snow;
} SitVdStandbyConfig;

/** Opaque u64 slots backing SitVirtualDisplay*Backend in situation_impl_vd_backend.h */
#define SIT_VD_BACKEND_STORAGE_U64_COUNT 16

typedef struct {
    int      id;                     // Unique sequential ID assigned at creation (used internally for tracking)
    Vector2  resolution;             // Render resolution of this virtual display (width, height in pixels)
    Vector2  offset;                 // Top-left screen position when composited to the main window (in screen pixels)
    float    opacity;                // Global alpha multiplier for the entire display (0.0f = fully transparent, 1.0f = opaque)
    bool     visible;                // If false, the display is skipped entirely during compositing
    int      z_order;                // Sorting key for compositing order — lower values are drawn first (background → foreground)

    // ── Independent Timing & Animation System (allows retro slowdown, bullet-time, UI-independent speed, etc.) ──
    uint64_t frame_count;                // Number of frames this virtual display has advanced (independent of main window)
    double   frame_time_multiplier;      // Speed multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed, etc.)
    double   elapsed_time_seconds;       // Total time this display has been running (affected by frame_time_multiplier)
    float    cycle_animation_value;      // Oscillating value 0.0..1.0..0.0 useful for cheap pulsing/shake effects
    double   last_update_time_seconds;   // Timestamp of the last frame advance (used for delta calculation)
    double   frame_delta_time_seconds;   // Delta time for this virtual display's last frame (affected by multiplier)

    // ── Content update tracking (distinct from frame clock above) ──
    double   last_content_update_time;    // Monotonic time at last pixel write (draw/copy/dispatch)
    uint64_t last_content_update_frame;   // vd->frame_count at last content write
    double   idle_threshold_seconds;      // Seconds without content write before compositor idle fallback (default 1.0)
    SituationVDFallbackMode fallback_mode; // PATTERN (zero layers = snow), COLORBURST, or SOLID
    ColorRGBA fallback_color;             // SOLID idle RGBA (default deep blue; unchanged at create)
    SitVdStandbyConfig standby_pattern; // PATTERN tuning; pattern_layers 0 at create (snow)

    // ── Optimization & Compositing Controls ──
    bool                    is_dirty;       // Set to true when content changed → forces re-render of the off-screen buffer
    SituationScalingMode    scaling_mode;   // How the VD is scaled when composited (Integer, Fit, Stretch, etc.)
    SituationBlendMode      blend_mode;     // Blending style when compositing (Alpha, Additive, Overlay, Soft Light, Screen Grab, etc.)
    SituationVDFlags        flags;          // Creation flags (e.g. COMPUTE_TARGET)
    int                     texture_slot_index; // Index into texture_registry for compute-target VDs (-1 if not a compute target)

    /* ── Attachment configuration (VD-1) ── */
    SituationVirtualDisplayColorFormat      color_format;
    SituationVirtualDisplayDepthStencilMode depth_stencil_mode;
    SituationVirtualDisplayAttachmentDefaults attachment_defaults;

    /* ── Rendering quality + composite sampler (VD-3 / VD-4a) ── */
    SituationMultisampleQuality        msaa_quality;       /* stored from desc.msaa_samples; > OFF gated until VD-4b */
    bool                               pending_gpu_rebuild; /* tier-B heavy rebuild queued (MSAA, etc.) */
    uint32_t                         color_mip_levels;
    SituationVirtualDisplaySamplerDesc composite_sampler;

    /* ── Performance / memory (VD-5) ── */
    SituationVirtualDisplayUpdateMode update_mode;
    SituationVirtualDisplayMemoryHint   memory_hint;

    /* Opaque GPU backend blob — typed as SitVirtualDisplay*Backend in situation_impl_vd_backend.h */
    alignas(8) uint64_t backend[SIT_VD_BACKEND_STORAGE_U64_COUNT];
} SituationVirtualDisplay;
// [v2.3.22] Opaque Render List Handle (Momentum)
typedef struct SituationRenderList_t* SituationRenderList;
/**
 * @brief Flags representing optional GPU capabilities and advanced feature sets.
 * @details Used with SituationIsFeatureSupported() to check runtime availability. These flags cover core
 *          rasterization features, compute capabilities, and next-generation rendering techniques.
 */
typedef enum {
    // ── Core Rasterization ──
    SIT_FEATURE_GEOMETRY_SHADER        = 1 << 0,  // Geometry shader support
    SIT_FEATURE_TESSELLATION_SHADER    = 1 << 1,  // Tessellation control/eval shaders
    SIT_FEATURE_WIDE_LINES             = 1 << 2,  // Lines with width > 1.0
    SIT_FEATURE_FILL_MODE_NON_SOLID    = 1 << 3,  // Wireframe/Point rendering (PolygonMode)
    SIT_FEATURE_SAMPLER_ANISOTROPY     = 1 << 4,  // Anisotropic texture filtering
    SIT_FEATURE_MULTI_VIEWPORT         = 1 << 5,  // Multiple viewports/scissors (e.g. for VR/Split-screen without multiple passes)

    // ── Compute & Precision ──
    SIT_FEATURE_COMPUTE_SHADER         = 1 << 6,  // Compute shader support (Standard in Vulkan, GL 4.3+)
    SIT_FEATURE_INT64                  = 1 << 7,  // 64-bit integer support in shaders (int64_t)
    SIT_FEATURE_FLOAT64                = 1 << 8,  // 64-bit float (double) support in shaders
    SIT_FEATURE_FLOAT16                = 1 << 9,  // 16-bit float (half) support for storage/arithmetic (performance/bandwidth optimization)
    SIT_FEATURE_SUBGROUP_OPERATIONS    = 1 << 10, // Subgroup/Wave intrinsics (ballot, shuffle, arithmetic)

    // ── Modern Memory Model (Bindless) ──
    SIT_FEATURE_BINDLESS_BUFFERS       = 1 << 11, // Buffer Device Address / GL_EXT_buffer_reference (Pointers in shaders)
    SIT_FEATURE_BINDLESS_TEXTURES      = 1 << 12, // Descriptor Indexing / Bindless Textures (Arrays of unbounded textures)

    // ── GPU-Driven Rendering ──
    SIT_FEATURE_DRAW_INDIRECT_COUNT    = 1 << 13, // DrawIndirectCount / MultiDrawIndirect with count buffer (GPU culling)
    SIT_FEATURE_MULTI_DRAW_INDIRECT    = 1 << 14, // Standard MultiDrawIndirect support

    // ── Advanced Rendering ──
    SIT_FEATURE_MESH_SHADER            = 1 << 15, // Mesh Shaders (NV/EXT) - Replaces vertex/geometry pipeline
    SIT_FEATURE_RAY_TRACING            = 1 << 16, // Hardware Ray Tracing (KHR_ray_tracing_pipeline / queries)
    SIT_FEATURE_VARIABLE_RATE_SHADING  = 1 << 17, // Variable Rate Shading (VRS) for performance optimization
    SIT_FEATURE_ATOMIC_FLOAT           = 1 << 18, // Atomic operations on floating point images/buffers

    // ── Asset Support ──
    SIT_FEATURE_TEXTURE_COMPRESSION_BC = 1 << 19, // Block Compression (BC1-BC7 / S3TC) support
    SIT_FEATURE_TEXTURE_COMPRESSION_ASTC = 1 << 20, // ASTC Compression support (Mobile/High-end)
    SIT_FEATURE_HDR_OUTPUT             = 1 << 21, // HDR10/PQ swapchain active (not 10-bit SDR alone)
    SIT_FEATURE_10BIT_SDR_OUTPUT       = 1 << 22, // 10-bit SDR swapchain active (A2R10 + SRGB_NONLINEAR)
    SIT_FEATURE_GPU_TIMESTAMPS         = 1 << 23, // GPU elapsed/timestamp queries for frame profile zones (P10.3)

} SituationRenderFeature;

/** Predefined GPU profile zone IDs (P10.3). User slots: SITUATION_GPU_ZONE_USER_0 .. _USER_11 (4–15). */
typedef enum SituationGPUProfileZone {
    SITUATION_GPU_ZONE_COMPOSITE   = 0,  /* Full SituationRenderVirtualDisplays pass */
    SITUATION_GPU_ZONE_VD_PATH_A   = 1,  /* VD advanced-blend (screen-copy) path — last draw per frame if multiple */
    SITUATION_GPU_ZONE_VD_PATH_B   = 2,  /* VD simple alpha-blend path — last draw per frame if multiple */
    SITUATION_GPU_ZONE_TEXT_BATCH  = 3,  /* Internal batched text draw */
    SITUATION_GPU_ZONE_USER_0      = 4,
    SITUATION_GPU_ZONE_USER_1      = 5,
    SITUATION_GPU_ZONE_USER_2      = 6,
    SITUATION_GPU_ZONE_USER_3      = 7,
    SITUATION_GPU_ZONE_USER_4      = 8,
    SITUATION_GPU_ZONE_USER_5      = 9,
    SITUATION_GPU_ZONE_USER_6      = 10,
    SITUATION_GPU_ZONE_USER_7      = 11,
    SITUATION_GPU_ZONE_USER_8      = 12,
    SITUATION_GPU_ZONE_USER_9      = 13,
    SITUATION_GPU_ZONE_USER_10     = 14,
    SITUATION_GPU_ZONE_USER_11     = 15,
} SituationGPUProfileZone;
// --- Legacy OpenGL-style barrier bits (kept for low-level compatibility helpers) ---
#define SITUATION_BARRIER_VERTEX_ATTRIB_ARRAY_BIT   		0x00000001
#define SITUATION_BARRIER_ELEMENT_ARRAY_BIT         		0x00000002
#define SITUATION_BARRIER_UNIFORM_BARRIER_BIT       		0x00000004
#define SITUATION_BARRIER_TEXTURE_FETCH_BARRIER_BIT 		0x00000008
#define SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BARRIER_BIT 	0x00000020
#define SITUATION_BARRIER_COMMAND_BARRIER_BIT       		0x00000040
#define SITUATION_BARRIER_PIXEL_BUFFER_BARRIER_BIT  		0x00000080
#define SITUATION_BARRIER_TEXTURE_UPDATE_BARRIER_BIT 		0x00000100
#define SITUATION_BARRIER_BUFFER_UPDATE_BARRIER_BIT 		0x00000200
#define SITUATION_BARRIER_FRAMEBUFFER_BARRIER_BIT   		0x00000400
#define SITUATION_BARRIER_TRANSFORM_FEEDBACK_BARRIER_BIT 	0x00000800
#define SITUATION_BARRIER_ATOMIC_COUNTER_BARRIER_BIT 		0x00001000
#define SITUATION_BARRIER_SHADER_STORAGE_BARRIER_BIT 		0x00002000
#define SITUATION_BARRIER_ALL_BARRIER_BITS          		0xFFFFFFFF

// Aliases to match implementation usage
#define SITUATION_BARRIER_INDEX_BUFFER_BIT          SITUATION_BARRIER_ELEMENT_ARRAY_BIT
#define SITUATION_BARRIER_UNIFORM_BUFFER_BIT        SITUATION_BARRIER_UNIFORM_BARRIER_BIT
#define SITUATION_BARRIER_TEXTURE_FETCH_BIT         SITUATION_BARRIER_TEXTURE_FETCH_BARRIER_BIT
#define SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BIT   SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define SITUATION_BARRIER_COMMAND_BIT               SITUATION_BARRIER_COMMAND_BARRIER_BIT
#define SITUATION_BARRIER_SHADER_STORAGE_BIT        SITUATION_BARRIER_SHADER_STORAGE_BARRIER_BIT
// ---------------------------------------------------------------------------------
//  Vertex Attribute Data Types (used in SituationVertexAttribute layout descriptions)
// ---------------------------------------------------------------------------------
typedef enum {
    SIT_DATA_BYTE           = 0,  // 8-bit signed integer   (normalized possible)
    SIT_DATA_UNSIGNED_BYTE  = 1,  // 8-bit unsigned integer (normalized possible)
    SIT_DATA_SHORT          = 2,  // 16-bit signed integer
    SIT_DATA_UNSIGNED_SHORT = 3,  // 16-bit unsigned integer
    SIT_DATA_INT            = 4,  // 32-bit signed integer
    SIT_DATA_UNSIGNED_INT   = 5,  // 32-bit unsigned integer
    SIT_DATA_FLOAT          = 6,  // 32-bit IEEE floating point (default for most attributes)
    SIT_DATA_DOUBLE         = 7,  // 64-bit IEEE floating point (rare, only when explicitly needed)
} SituationDataType;
// ---------------------------------------------------------------------------------
//  Compute Shader Source Format Specification
//  Used when creating compute pipelines on backends that support multiple input formats.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_GL_SHADER_SOURCE_TYPE_GLSL   = 0,  // Null-terminated GLSL source string (compiled at runtime via shaderc when enabled)
    SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV  = 1,  // Raw SPIR-V bytecode blob (uint32_t array) - used when pre-compiling offline
} SituationGLShaderSourceType;

// ---------------------------------------------------------------------------------
//  Pipeline Barrier Source Access Flags
//  Describes which previous pipeline stages have written to memory that later stages need to read.
//  Combine with bitwise OR.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BARRIER_VERTEX_SHADER_WRITE   = 1 << 0,   // Vertex shader wrote to SSBO / image
    SITUATION_BARRIER_FRAGMENT_SHADER_WRITE  = 1 << 1,   // Fragment shader wrote to SSBO / image / color attachment
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE   = 1 << 2,   // Compute shader wrote to storage buffer / image
    SITUATION_BARRIER_TRANSFER_WRITE         = 1 << 3,   // Copy/blit/fill operations wrote to buffer/image
} SituationBarrierSrcFlags;

// ---------------------------------------------------------------------------------
//  Pipeline Barrier Destination Access Flags
//  Describes which subsequent pipeline stages will read memory written by earlier stages.
//  Combine with bitwise OR.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BARRIER_VERTEX_SHADER_READ     = 1 << 0,   // Vertex shader will read SSBO/image
    SITUATION_BARRIER_FRAGMENT_SHADER_READ    = 1 << 1,   // Fragment shader will read SSBO/image/color attachment
    SITUATION_BARRIER_COMPUTE_SHADER_READ     = 1 << 2,   // Compute shader will read storage buffer/image
    SITUATION_BARRIER_TRANSFER_READ           = 1 << 3,   // Copy/blit operations will read from buffer/image
    SITUATION_BARRIER_INDIRECT_COMMAND_READ   = 1 << 4,   // Indirect draw/dispatch buffer will be read by command processor
} SituationBarrierDstFlags;

typedef enum {
    SITUATION_PIPELINE_STAGE_TOP              = 1 << 0,
    SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND = 1 << 1,
    SITUATION_PIPELINE_STAGE_VERTEX_INPUT     = 1 << 2,
    SITUATION_PIPELINE_STAGE_VERTEX_SHADER    = 1 << 3,
    SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER  = 1 << 4,
    SITUATION_PIPELINE_STAGE_COLOR_ATTACHMENT = 1 << 5,
    SITUATION_PIPELINE_STAGE_DEPTH_STENCIL    = 1 << 6,
    SITUATION_PIPELINE_STAGE_COMPUTE_SHADER   = 1 << 7,
    SITUATION_PIPELINE_STAGE_TRANSFER         = 1 << 8,
    SITUATION_PIPELINE_STAGE_HOST             = 1 << 9,
    SITUATION_PIPELINE_STAGE_BOTTOM           = 1 << 10
} SituationPipelineStageFlags;

typedef enum {
    SITUATION_ACCESS_INDIRECT_COMMAND_READ   = 1 << 0,
    SITUATION_ACCESS_VERTEX_READ             = 1 << 1,
    SITUATION_ACCESS_INDEX_READ              = 1 << 2,
    SITUATION_ACCESS_UNIFORM_READ            = 1 << 3,
    SITUATION_ACCESS_SHADER_READ             = 1 << 4,
    SITUATION_ACCESS_SHADER_WRITE            = 1 << 5,
    SITUATION_ACCESS_COLOR_ATTACHMENT_READ   = 1 << 6,
    SITUATION_ACCESS_COLOR_ATTACHMENT_WRITE  = 1 << 7,
    SITUATION_ACCESS_DEPTH_STENCIL_READ      = 1 << 8,
    SITUATION_ACCESS_DEPTH_STENCIL_WRITE     = 1 << 9,
    SITUATION_ACCESS_TRANSFER_READ           = 1 << 10,
    SITUATION_ACCESS_TRANSFER_WRITE          = 1 << 11,
    SITUATION_ACCESS_HOST_READ               = 1 << 12,
    SITUATION_ACCESS_HOST_WRITE              = 1 << 13
} SituationAccessFlags;

typedef struct {
    uint32_t src_stages;
    uint32_t src_access;
    uint32_t dst_stages;
    uint32_t dst_access;
} SituationPipelineBarrierDesc;

typedef struct {
    SituationBuffer buffer;
    size_t offset;
    size_t size;
    uint32_t src_stages;
    uint32_t src_access;
    uint32_t dst_stages;
    uint32_t dst_access;
} SituationBufferBarrierDesc;

/**
 * @brief Backend-neutral texture layouts for explicit image barriers.
 *
 * @details This is a vocabulary for commands such as `SituationCmdTextureBarrier`.
 *          It does not imply automatic layout tracking. Callers must provide the
 *          actual old layout and intended new layout for the texture subresource.
 */
typedef enum {
    SITUATION_TEXTURE_LAYOUT_UNDEFINED = 0,
    SITUATION_TEXTURE_LAYOUT_GENERAL,
    SITUATION_TEXTURE_LAYOUT_SHADER_READ,
    SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC,
    SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
    SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT,
    SITUATION_TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
    SITUATION_TEXTURE_LAYOUT_PRESENT
} SituationTextureLayout;

/**
 * @brief Explicit texture memory/layout barrier for a 2D texture subresource range.
 *
 * @details For the first slice, public textures are treated as color-only 2D images.
 *          `mip_level_count == 0` means one mip level. `array_layer_count == 0`
 *          means one layer. Array layers other than layer 0 are reserved until
 *          array/cube texture ownership is exposed. `old_layout` may be
 *          `SITUATION_TEXTURE_LAYOUT_UNDEFINED`; `new_layout` must be a real
 *          usage layout.
 */
typedef struct {
    SituationTextureLayout old_layout;
    SituationTextureLayout new_layout;
    uint32_t base_mip_level;
    uint32_t mip_level_count;
    uint32_t base_array_layer;
    uint32_t array_layer_count;
} SituationTextureBarrierDesc;
/** Active renderer backend for this Situation DLL build (OpenGL vs Vulkan). */
typedef enum SituationGraphicsBackend {
    SIT_GRAPHICS_BACKEND_UNKNOWN = 0,
    SIT_GRAPHICS_BACKEND_OPENGL  = 1,
    SIT_GRAPHICS_BACKEND_VULKAN  = 2,
} SituationGraphicsBackend;

typedef struct SituationGraphicsCaps {
    uint32_t api_version_packed;        /* Situation backend target: (4<<16)|6 OpenGL, (1<<16)|4 Vulkan */
    int      max_msaa_samples;
    int      bindless_textures;
    int      shader_compiler_available;
    int      compute_supported;
    int      max_viewports;             /* GL_MAX_VIEWPORTS / VkPhysicalDeviceLimits::maxViewports (>=1 after init) */
    SituationGraphicsBackend backend;   /* Canonical: same as SituationGetGraphicsBackend() after init */
    uint32_t device_api_version_packed; /* Runtime GL context / VkPhysicalDevice version (major<<16|minor) */
    uint8_t  output_bits_per_channel;   /* 8 or 10 after init (defaults to 8) */
    uint8_t  output_color_depth_active; /* 1 when a 10-bit swapchain / default FB is in use */
    uint8_t  output_hdr_active;         /* 1 when HDR10 ST2084 swapchain color space is active */
    uint8_t  output_color_space;        /* SituationOutputColorSpace */
    uint8_t  wsi_supports_10bit_sdr;    /* Vulkan: WSI lists A2R10G10B10+SRGB_NONLINEAR (0/1) */
    uint8_t  wsi_supports_hdr10;        /* Vulkan: WSI lists A2R10G10B10+HDR10_ST2084 (0/1) */
} SituationGraphicsCaps;

// --- Raster & fixed-function state (Graphics module) ---
typedef enum SituationCullMode {
    SIT_CULL_NONE = 0,
    SIT_CULL_BACK,
    SIT_CULL_FRONT
} SituationCullMode;

typedef enum SituationFrontFace {
    SIT_FRONT_FACE_CCW = 0,
    SIT_FRONT_FACE_CW
} SituationFrontFace;

typedef enum SituationPrimitiveTopology {
    SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 0,
    SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    SIT_PRIMITIVE_TOPOLOGY_LINE_LIST,
    SIT_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    SIT_PRIMITIVE_TOPOLOGY_POINT_LIST
} SituationPrimitiveTopology;

typedef enum SituationPolygonMode {
    SIT_POLYGON_MODE_FILL = 0,
    SIT_POLYGON_MODE_LINE,
    SIT_POLYGON_MODE_POINT
} SituationPolygonMode;

typedef enum SituationIndexType {
    SIT_INDEX_UINT32 = 0,
    SIT_INDEX_UINT16
} SituationIndexType;

typedef enum SituationDepthCompareOp {
    SIT_DEPTH_COMPARE_ALWAYS = 0,
    SIT_DEPTH_COMPARE_LESS,
    SIT_DEPTH_COMPARE_LEQUAL,
    SIT_DEPTH_COMPARE_GREATER,
    SIT_DEPTH_COMPARE_GEQUAL,
    SIT_DEPTH_COMPARE_EQUAL,
    SIT_DEPTH_COMPARE_NOTEQUAL,
    SIT_DEPTH_COMPARE_NEVER
} SituationDepthCompareOp;

typedef enum SituationStencilOp {
    SIT_STENCIL_OP_KEEP = 0,
    SIT_STENCIL_OP_ZERO,
    SIT_STENCIL_OP_REPLACE,
    SIT_STENCIL_OP_INCREMENT_CLAMP,
    SIT_STENCIL_OP_DECREMENT_CLAMP,
    SIT_STENCIL_OP_INVERT,
    SIT_STENCIL_OP_INCREMENT_WRAP,
    SIT_STENCIL_OP_DECREMENT_WRAP
} SituationStencilOp;

typedef struct SituationStencilState {
    SituationDepthCompareOp compare_op;
    SituationStencilOp fail_op;
    SituationStencilOp depth_fail_op;
    SituationStencilOp pass_op;
    uint32_t compare_mask;
    uint32_t write_mask;
    uint32_t reference;
} SituationStencilState;

typedef struct SituationMultisampleState {
    bool sample_shading_enable;
    float min_sample_shading;
    uint32_t sample_mask;
    bool alpha_to_coverage_enable;
} SituationMultisampleState;

typedef enum SituationBlendFactor {
    SIT_BLEND_ZERO = 0,
    SIT_BLEND_ONE,
    SIT_BLEND_SRC_COLOR,
    SIT_BLEND_ONE_MINUS_SRC_COLOR,
    SIT_BLEND_DST_COLOR,
    SIT_BLEND_ONE_MINUS_DST_COLOR,
    SIT_BLEND_SRC_ALPHA,
    SIT_BLEND_ONE_MINUS_SRC_ALPHA,
    SIT_BLEND_DST_ALPHA,
    SIT_BLEND_ONE_MINUS_DST_ALPHA
} SituationBlendFactor;

/** Vertex layout / draw-mode tag set at mesh creation (Phase C). */
typedef enum SituationMeshVertexLayout {
    SIT_MESH_LAYOUT_POS_NRM_TEX = 0, /**< Default legacy contract: vec3 pos + vec3 normal + vec2 uv (32 bytes). */
    SIT_MESH_LAYOUT_POS_ONLY,        /**< vec3 position only (12 bytes). */
    SIT_MESH_LAYOUT_POS_TEX,         /**< vec3 position + vec2 uv (20 bytes). */
    SIT_MESH_LAYOUT_POS_NRM,         /**< vec3 position + vec3 normal (24 bytes). */
    SIT_MESH_LAYOUT_POS_NRM_TAN_TEX, /**< PBR: vec3 pos + vec3 normal + vec4 tangent + vec2 uv (48 bytes). */
    SIT_MESH_LAYOUT_PULL,            /**< Vertex-pull draw path: BDA + buffer_reference VS; VAO/IA fallback still valid. */
} SituationMeshVertexLayout;

/**
 * Standard push-constant block for vertex-pull shaders (Phase C2).
 * Matches `layout(push_constant) uniform PC { uint64_t vertex_address; uint64_t index_address; }`.
 * Index address is 0 when the mesh has no index buffer.
 */
typedef struct SituationMeshPullPushConstants {
    uint64_t vertex_address;
    uint64_t index_address;
} SituationMeshPullPushConstants;

/** Single SSBO block descriptor returned by SituationQueryShaderStorageBlocks. */
typedef struct SituationShaderStorageBlockInfo {
    char name[128];         /**< Block name as reported by GL (may be empty on SPIR-V). */
    uint32_t binding_point; /**< Active binding point assigned after link (post _SituationBindGLProgramStorageBlocks). */
    uint32_t block_index;   /**< GL resource index (0-based). */
} SituationShaderStorageBlockInfo;

typedef struct SituationUniformExpectation {
    const char* name;
    SituationUniformType type;
    int array_length; /* 0 = scalar */
} SituationUniformExpectation;

typedef enum SituationCameraFlags {
    SIT_CAMERA_FLAG_NONE                 = 0,
    SIT_CAMERA_FLAG_ORTHOGRAPHIC         = 1 << 0,  // Use orthographic instead of perspective
    SIT_CAMERA_FLAG_REVERSE_Z            = 1 << 1,  // Use infinite reverse-Z projection (1.0 near, 0.0 far)
    SIT_CAMERA_FLAG_INFINITE_PROJECTION  = 1 << 2   // Use infinite projection (normal Z)
} SituationCameraFlags;

typedef struct SituationCameraDesc {
    Vector3 eye;
    Vector3 target;
    Vector3 up;               // Default to {0,1,0} if {0,0,0}
    float   vertical_fov_deg; // Used if perspective
    float   ortho_height;     // Used if SIT_CAMERA_FLAG_ORTHOGRAPHIC is set
    float   aspect;           // 0.0f auto-uses current window/render target aspect
    float   z_near;
    float   z_far;
    uint32_t flags;           // Bitmask of SituationCameraFlags
} SituationCameraDesc;

/** Version of SituationFrameProfile layout (P10.1). Bump when fields change. */
#define SITUATION_FRAME_PROFILE_VERSION 1
#ifndef SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT
#define SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT 16
#endif

/** Headless frame telemetry snapshot (wraps P10.0 getters). gpu_zone_ns[] populated when SIT_FEATURE_GPU_TIMESTAMPS. */
typedef struct SituationFrameProfile {
    uint32_t struct_version;  // SITUATION_FRAME_PROFILE_VERSION
    uint32_t struct_size;     // sizeof(SituationFrameProfile) — for forward-compatible callers

    double   frame_time_ms;
    double   max_frame_time_ms;
    uint32_t spike_count;
    uint32_t _reserved0;

    uint64_t backpressure_ns;
    uint64_t fence_wait_ns;
    uint64_t execute_ns;
    uint64_t present_ns;
    uint64_t poll_ns;
    uint64_t update_ns;

    uint64_t render_latency_avg_ns;
    uint64_t render_latency_max_ns;
    size_t   queue_depth;

    uint64_t gpu_zone_ns[SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT];
} SituationFrameProfile;

/** User query pool type (P10.4). Pipeline statistics deferred. */
typedef enum {
    SITUATION_QUERY_TYPE_TIMESTAMP = 0,
    SITUATION_QUERY_TYPE_OCCLUSION = 1,
} SituationQueryType;

/** Flags for SituationGetQueryPoolResults. */
typedef enum {
    SITUATION_QUERY_RESULT_WAIT_BIT = 1u << 0,
} SituationQueryResultFlags;

#endif /* SITUATION_API_TYPES_GPU_H */
