#include <stdio.h>
#include "sit/situation_base_types.h"
typedef struct { Vector2 screen_size; Vector2 char_size; Vector2 grid_size; float time; union { uint32_t cursor_index; }; uint32_t cursor_blink_state; uint32_t text_blink_state; uint32_t sel_start; uint32_t sel_end; uint32_t sel_active; uint32_t mouse_cursor_index; uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr; uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle; uint64_t shader_config_addr; uint32_t atlas_cols; uint32_t vector_count; int sixel_y_offset; uint32_t grid_color; uint32_t conceal_char_code; uint32_t font_data_width; uint32_t font_data_height; } PC;
int main(){ printf("%zu\n", sizeof(PC)); return 0; }
