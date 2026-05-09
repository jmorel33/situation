/*
 * Minimal syntax test for color encoding changes
 * Tests only the parts we modified without full library dependencies
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Test enum definition
typedef enum SituationColorEncoding {
    SITUATION_COLOR_LINEAR = 0,
    SITUATION_COLOR_SRGB = 1
} SituationColorEncoding;

// Test image struct with color_encoding field
typedef struct SituationImage {
    void *data;
    int width;
    int height;
    int channels;
    SituationColorEncoding color_encoding;
} SituationImage;

// Test texture slot with format fields (simplified)
typedef struct _SituationTextureSlot {
    bool is_active;
    uint32_t generation;
    int width;
    int height;
    
    // Vulkan format field
    #ifdef USE_VULKAN
    uint32_t format;  // VkFormat is uint32_t
    #endif
    
    // OpenGL format field
    #ifdef USE_OPENGL
    uint32_t internal_format;  // GLenum is uint32_t
    #endif
} _SituationTextureSlot;

// Test format selection logic (simplified)
void test_format_selection() {
    SituationImage img = {0};
    img.color_encoding = SITUATION_COLOR_SRGB;
    
    _SituationTextureSlot slot = {0};
    
    #ifdef USE_VULKAN
    // Vulkan format selection
    uint32_t vk_format;
    uint32_t usage_flags = 0x00000008;  // SITUATION_TEXTURE_USAGE_STORAGE
    
    if (usage_flags & 0x00000008) {
        vk_format = 37;  // VK_FORMAT_R8G8B8A8_UNORM
    } else {
        vk_format = (img.color_encoding == SITUATION_COLOR_SRGB) ? 43 : 37;
    }
    
    slot.format = vk_format;
    #endif
    
    #ifdef USE_OPENGL
    // OpenGL format selection
    uint32_t gl_internal_format;
    uint32_t usage_flags = 0x00000008;
    
    if (usage_flags & 0x00000008) {
        gl_internal_format = 0x8058;  // GL_RGBA8
    } else {
        gl_internal_format = (img.color_encoding == SITUATION_COLOR_SRGB) ? 0x8C43 : 0x8058;
    }
    
    slot.internal_format = gl_internal_format;
    #endif
}

int main() {
    test_format_selection();
    return 0;
}
