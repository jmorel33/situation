/*
 * KTerm Showcase - Graphical Features Demo
 * Demonstrates kterm's rich terminal emulation in an 80x50 window
 * 
 * Features demonstrated:
 * - 256-color palette
 * - True Color (24-bit RGB)
 * - Text attributes (bold, italic, underline, blink, reverse)
 * - Cursor styles
 * - Box drawing characters
 * - Animated effects
 * - SGR color sequences
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

// Use Situation + K-Term DLL
#define SITUATION_USE_SHARED
#include "situation.h"
#include "../sit/k-term/kterm_api.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
// Helper to write sequences to terminal
static void write_seq(KTerm* term, const char* seq) {
    KTerm_WriteString(term, seq);
}

// Draw a box using box-drawing characters
static void draw_box(KTerm* term, int x, int y, int width, int height, const char* title) {
    char buf[256];
    
    // Top border
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\xDA", y, x);  // Move and draw top-left corner
    write_seq(term, buf);
    for (int i = 0; i < width - 2; i++) write_seq(term, "\xC4");  // Horizontal line
    write_seq(term, "\xBF");  // Top-right corner
    
    // Title if provided
    if (title && strlen(title) > 0) {
        int title_x = x + (width - strlen(title)) / 2;
        snprintf(buf, sizeof(buf), "\x1B[%d;%dH%s", y, title_x, title);
        write_seq(term, buf);
    }
    
    // Sides
    for (int i = 1; i < height - 1; i++) {
        snprintf(buf, sizeof(buf), "\x1B[%d;%dH\xB3", y + i, x);  // Left side
        write_seq(term, buf);
        snprintf(buf, sizeof(buf), "\x1B[%d;%dH\xB3", y + i, x + width - 1);  // Right side
        write_seq(term, buf);
    }
    
    // Bottom border
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\xC0", y + height - 1, x);
    write_seq(term, buf);
    for (int i = 0; i < width - 2; i++) write_seq(term, "\xC4");
    write_seq(term, "\xD9");  // Bottom-right corner
}

// Display 256-color palette
static void show_256_colors(KTerm* term, int start_y) {
    char buf[256];
    
    // Standard colors (0-15)
    snprintf(buf, sizeof(buf), "\x1B[%d;2H\x1B[1mStandard Colors (0-15):\x1B[0m", start_y);
    write_seq(term, buf);
    
    for (int i = 0; i < 16; i++) {
        snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[48;5;%dm  \x1B[0m", 
                 start_y + 1, 2 + i * 4, i);
        write_seq(term, buf);
    }
    
    // 216 color cube (16-231)
    snprintf(buf, sizeof(buf), "\x1B[%d;2H\x1B[1m216 Color Cube:\x1B[0m", start_y + 3);
    write_seq(term, buf);
    
    for (int i = 0; i < 36; i++) {
        for (int j = 0; j < 6; j++) {
            int color = 16 + i * 6 + j;
            snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[48;5;%dm \x1B[0m", 
                     start_y + 4 + i / 6, 2 + (i % 6) * 6 + j, color);
            write_seq(term, buf);
        }
    }
    
    // Grayscale (232-255)
    snprintf(buf, sizeof(buf), "\x1B[%d;2H\x1B[1mGrayscale:\x1B[0m", start_y + 10);
    write_seq(term, buf);
    
    for (int i = 0; i < 24; i++) {
        snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[48;5;%dm \x1B[0m", 
                 start_y + 11, 2 + i, 232 + i);
        write_seq(term, buf);
    }
}

// Display text attributes
static void show_attributes(KTerm* term, int x, int y) {
    char buf[256];
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[1mBold Text\x1B[0m", y, x);
    write_seq(term, buf);
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[2mFaint Text\x1B[0m", y + 1, x);
    write_seq(term, buf);
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[3mItalic Text\x1B[0m", y + 2, x);
    write_seq(term, buf);
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[4mUnderlined Text\x1B[0m", y + 3, x);
    write_seq(term, buf);
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[5mBlinking Text\x1B[0m", y + 4, x);
    write_seq(term, buf);
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[7mReverse Video\x1B[0m", y + 5, x);
    write_seq(term, buf);
    
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[9mStrikethrough\x1B[0m", y + 6, x);
    write_seq(term, buf);
}

// Display true color gradient
static void show_true_color_gradient(KTerm* term, int y) {
    char buf[256];
    
    snprintf(buf, sizeof(buf), "\x1B[%d;2H\x1B[1mTrue Color (24-bit RGB) Gradient:\x1B[0m", y);
    write_seq(term, buf);
    
    for (int i = 0; i < 60; i++) {
        int r = (int)(255 * (float)i / 60);
        int g = (int)(128 + 127 * sin(i * 0.1));
        int b = (int)(255 - 255 * (float)i / 60);
        
        snprintf(buf, sizeof(buf), "\x1B[%d;%dH\x1B[38;2;%d;%d;%dm\xDB\x1B[0m", 
                 y + 1, 2 + i, r, g, b);
        write_seq(term, buf);
    }
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  KTerm Showcase - Graphical Features\n");
    printf("========================================\n\n");
    
    // Initialize Situation
    SituationInitInfo config = {
        .window_width = 1280,
        .window_height = 960,
        .window_title = "KTerm Showcase - 80x50 Terminal",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
        .enable_vulkan_validation = false
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("[ERROR] Situation initialization failed\n");
        return -1;
    }
    
    printf("[SUCCESS] Situation initialized\n");
    
    // Create KTerm instance (80x50)
    KTermConfig kterm_config = {
        .width = 80,
        .height = 50,
        .response_callback = NULL,
        .max_sixel_width = 0,
        .max_sixel_height = 0,
        .max_kitty_image_pixels = 0,
        .max_ops_per_flush = 0,
        .strict_mode = false
    };
    
    KTerm* term = KTerm_Create(kterm_config);
    if (!term) {
        printf("[ERROR] KTerm creation failed\n");
        SituationShutdown();
        return -1;
    }
    
    printf("[SUCCESS] KTerm created (80x50)\n");
    fflush(stdout);
    
    // Fill with 'E' for testing
    EnhancedTermChar test_cell = {0};
    test_cell.ch = 'E';
    test_cell.fg_color.color_mode = 0;
    test_cell.fg_color.value.index = 7;
    test_cell.bg_color.color_mode = 0;
    test_cell.bg_color.value.index = 0;
    
    for (int y = 0; y < 50; y++) {
        for (int x = 0; x < 80; x++) {
            KTerm_SetCellDirect(term, x, y, test_cell);
        }
    }
    KTermRect full_screen = {0, 0, 80, 50};
    KTerm_MarkRegionDirty(term, full_screen);
    
    printf("Frame buffer filled with 'E'\n");
    fflush(stdout);
    
    // Render one frame
    KTerm_Update(term);
    KTerm_Draw(term);
    
    printf("First frame rendered, taking screenshot...\n");
    fflush(stdout);
    
    SituationImage screenshot = {0};
    if (SituationLoadImageFromScreen(&screenshot) == SITUATION_SUCCESS) {
        printf("Screenshot captured, saving...\n");
        fflush(stdout);
        SituationError result = SituationExportImage(screenshot, "kterm_test.png");
        printf("Save result: %d\n", result);
        fflush(stdout);
        SituationUnloadImage(screenshot);
    }
    
    printf("Press ESC to exit\n");
    fflush(stdout);
    
    // Main loop
    int frame = 1;
    
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        KTerm_Update(term);
        KTerm_Draw(term);
        frame++;
    }
    
    printf("\nShutting down...\n");
    KTerm_Destroy(term);
    SituationShutdown();
    printf("Done!\n");
    
    return 0;
}
