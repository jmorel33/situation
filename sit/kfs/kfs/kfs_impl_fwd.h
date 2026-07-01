/**
 * @file kfs_impl_fwd.h
 * @brief KFS implementation — cross-module static forward declarations.
 *
 * Included only via kfs_impl.h when KFS_IMPLEMENTATION is defined.
 * Do not include directly. No function bodies — declarations only.
 *
 * Each helper is defined in exactly one impl module (core, auth, or lc).
 */
#ifndef KFS_IMPL_FWD_H
#define KFS_IMPL_FWD_H

#ifdef KFS_IMPLEMENTATION

/* --- Static helper forward declarations (from kfs_impl.h:S2) --- */
static int check_group_admin_or_owner_perm(GameDB* db, uint64_t requesting_user_uuid, int target_group_actor_id);
static int kfs_can_bootstrap_admin_group_member(GameDB* db, uint64_t requesting_user_uuid, int group_actor_id, int member_actor_id);
static int get_active_actor_info_by_uuid(GameDB* db, uint64_t actor_uuid, int* actor_id, char** actor_type, char** name, int* is_admin);
static int kfs_get_topic_id_by_name(GameDB* db, int domain_id, const char* name, int* topic_id);
static int kfs_get_epic_id_by_name(GameDB* db, int domain_id, const char* name, int* epic_id);
static int kfs_save_asset(GameDB* db, const char* type, const char* name, const char* format, uint64_t creator_uuid, int owner_actor_id, int security_scheme_id, const void* data, size_t data_size, const char* text_data, const char* metadata, int* artifact_id_out);
static int kfs_load_asset_list(GameDB* db, sqlite3_stmt* stmt_ids, uint64_t requesting_user_uuid, KFS_Asset** results, int* result_count);
static int kfs_link_topic_to_artifact_by_name_internal(GameDB* db, int domain_id, int artifact_id, const char* topic_name);

#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_FWD_H */