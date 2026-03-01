import re

with open("situation_impl.h", "r") as f:
    content = f.read()

# remove redundant render pass caches
content = re.sub(r"(\s*// Render Pass Cache\s*_SituationCachedRenderPass render_pass_cache\[32\];\s*uint32_t render_pass_cache_count;\s*)+",
"""
    // Render Pass Cache
    _SituationCachedRenderPass render_pass_cache[32];
    uint32_t render_pass_cache_count;
""", content)

with open("situation_impl.h", "w") as f:
    f.write(content)
