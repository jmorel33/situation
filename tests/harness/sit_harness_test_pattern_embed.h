#ifndef SIT_HARNESS_TEST_PATTERN_EMBED_H
#define SIT_HARNESS_TEST_PATTERN_EMBED_H

#include <stddef.h>

#if defined(SITUATION_USE_OPENGL)
#define SIT_HARNESS_TP_PREFIX sit_harness_tp_gl
#elif defined(SITUATION_USE_VULKAN)
#define SIT_HARNESS_TP_PREFIX sit_harness_tp_vk
#endif

#define SIT_HARNESS_TP_JOIN2(a, b) a##b
#define SIT_HARNESS_TP_JOIN(a, b) SIT_HARNESS_TP_JOIN2(a, b)

#define sit_harness_test_pattern_fs_spv SIT_HARNESS_TP_JOIN(SIT_HARNESS_TP_PREFIX, _test_pattern_fs_spv)
#define sit_harness_test_pattern_fs_spv_len SIT_HARNESS_TP_JOIN(SIT_HARNESS_TP_PREFIX, _test_pattern_fs_spv_len)
#define sit_harness_test_pattern_smpte_vd_fs_spv SIT_HARNESS_TP_JOIN(SIT_HARNESS_TP_PREFIX, _test_pattern_smpte_vd_fs_spv)
#define sit_harness_test_pattern_smpte_vd_fs_spv_len SIT_HARNESS_TP_JOIN(SIT_HARNESS_TP_PREFIX, _test_pattern_smpte_vd_fs_spv_len)

extern const unsigned char sit_harness_test_pattern_fs_spv[];
extern const size_t sit_harness_test_pattern_fs_spv_len;
extern const unsigned char sit_harness_test_pattern_smpte_vd_fs_spv[];
extern const size_t sit_harness_test_pattern_smpte_vd_fs_spv_len;

#endif /* SIT_HARNESS_TEST_PATTERN_EMBED_H */
