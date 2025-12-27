/***************************************************************************************************
*   Situation Library - Example: Simple Mandelbrot Zoomer
*   -----------------------------------------------------
*   A simple CPU-based Mandelbrot set renderer.
*   It updates a texture every frame to simulate zooming.
*
*   Note: CPU rendering is slow at high resolutions. This is a "simple" demo.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"

// Resolution of the Mandelbrot texture (keep low for CPU performance)
#define TEX_WIDTH 320
#define TEX_HEIGHT 240

// Mandelbrot parameters
double zoom = 1.0;
double offset_x = -0.75;
double offset_y = 0.0;

void RenderMandelbrot(SituationImage* img) {
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            double zx = 0.0;
            double zy = 0.0;
            double cx = (x - img->width / 2.0) / (0.5 * zoom * img->width) + offset_x;
            double cy = (y - img->height / 2.0) / (0.5 * zoom * img->height) + offset_y;
            int iter = 0;
            int max_iter = 100;

            while (zx * zx + zy * zy < 4.0 && iter < max_iter) {
                double temp = zx * zx - zy * zy + cx;
                zy = 2.0 * zx * zy + cy;
                zx = temp;
                iter++;
            }

            // Map iteration count to color
            ColorRGBA col;
            if (iter == max_iter) {
                col = (ColorRGBA){0, 0, 0, 255};
            } else {
                float t = (float)iter / max_iter;
                col = (ColorRGBA){
                    (unsigned char)(9 * (1-t)*t*t*t * 255),
                    (unsigned char)(15 * (1-t)*(1-t)*t*t * 255),
                    (unsigned char)(8.5 * (1-t)*(1-t)*(1-t)*t * 255),
                    255
                };
            }
            SituationSetPixelColor(img, x, y, col);
        }
    }
}

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Situation - Mandelbrot Zoomer",
        .window_width = 800, .window_height = 600
    };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    // Create a persistent image buffer
    SituationImage buffer = {0};
    SituationCreateImage(TEX_WIDTH, TEX_HEIGHT, 4, &buffer);

    SituationTexture texture = {0};

    printf("Controls:\n  [Mouse Wheel] Zoom\n  [Arrow Keys] Pan\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        // --- Update Parameters ---
        if (SituationIsKeyDown(SIT_KEY_LEFT))  offset_x -= 0.05 / zoom;
        if (SituationIsKeyDown(SIT_KEY_RIGHT)) offset_x += 0.05 / zoom;
        if (SituationIsKeyDown(SIT_KEY_UP))    offset_y -= 0.05 / zoom;
        if (SituationIsKeyDown(SIT_KEY_DOWN))  offset_y += 0.05 / zoom;

        float scroll = SituationGetMouseWheelMove();
        if (scroll > 0) zoom *= 1.1;
        if (scroll < 0) zoom /= 1.1;

        // --- Render to CPU Buffer ---
        RenderMandelbrot(&buffer);

        // --- Upload to GPU ---
        // Destroy old texture (if exists) and create new one
        if (texture.generation > 0) SituationDestroyTexture(&texture);
        SituationCreateTexture(buffer, false, &texture);

        // --- Draw ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = { .display_id = -1, .color_attachment = { .loadOp = SIT_LOAD_OP_DONT_CARE } };

            SituationCmdBeginRenderPass(cmd, &pass);

            // Scale texture to fill screen
            Rectangle src = {0, 0, TEX_WIDTH, TEX_HEIGHT};
            Rectangle dst = {0, 0, 800, 600};
            SituationCmdDrawTexture(cmd, texture, src, dst, (Vector2){0,0}, 0.0f, (ColorRGBA){255,255,255,255});

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationUnloadImage(buffer);
    if (texture.generation > 0) SituationDestroyTexture(&texture);
    SituationShutdown();
    return 0;
}
