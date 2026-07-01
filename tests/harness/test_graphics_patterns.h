#ifndef TEST_GRAPHICS_PATTERNS_H
#define TEST_GRAPHICS_PATTERNS_H

void test_pattern_smpte_vd_bar_color(void);
void test_pattern_smpte_full_bar_color(void);
void test_pattern_checkerboard_corner(void);
void test_pattern_grid_line(void);
void test_pattern_pluge_black_bar(void);
void test_pattern_crosshatch_center(void);
void test_pattern_convergence_moire_zone(void);
void test_pattern_multiburst_bands(void);
void test_pattern_cube_lit_faces(void);
void test_pattern_compose_checker_plus_smpte(void);
void test_pattern_zero_layers_noise(void);
void test_pattern_chroma_snow(void);
void test_pattern_config_defaults(void);
void test_pattern_layer_params_checker_tile(void);
void test_pattern_layer_params_convergence_stripe(void);
#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_ENABLE_SHADER_COMPILER)
void test_pattern_runtime_include_compile(void);
#endif

#endif /* TEST_GRAPHICS_PATTERNS_H */
