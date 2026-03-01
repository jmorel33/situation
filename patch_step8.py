import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# Fix the compile error sit_gs.current_frame -> sit_render.vk.current_frame_index
content = content.replace("uint32_t current_image_index = sit_render.vk.acquired_image_indices[sit_gs.current_frame];",
                          "uint32_t current_image_index = sit_render.vk.acquired_image_indices[sit_render.vk.current_frame_index];")

with open("situation_impl.h", "w") as f:
    f.write(content)
