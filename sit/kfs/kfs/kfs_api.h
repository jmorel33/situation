/**
 * @file kfs/kfs_api.h
 * @brief KFS public API — types, status codes, and function declarations.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 *
 * Do not include directly; use kfs.h
 */
#ifndef KFS_API_H
#define KFS_API_H

#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

/* -- Version (canonical source: kfs_version.h — bump only there) -------- */
#include "kfs_version.h"
#include "kfs_mem.h"

#ifndef SQLITE_MAX_LENGTH
#define SQLITE_MAX_LENGTH 1000000000
#endif

// --- Status Codes ---
#define KFS_OK                  SQLITE_OK       // 0: Successful result
#define KFS_ERROR               SQLITE_ERROR    // 1: Generic SQL error or missing database
#define KFS_INTERNAL            SQLITE_INTERNAL // 2: Internal logic error
#define KFS_PERM                SQLITE_PERM     // 3: Access permission denied
#define KFS_ABORT               SQLITE_ABORT    // 4: Callback routine requested an abort
#define KFS_BUSY                SQLITE_BUSY     // 5: The database file is locked
#define KFS_LOCKED              SQLITE_LOCKED   // 6: A table in the database is locked
#define KFS_NOMEM               SQLITE_NOMEM    // 7: A malloc() failed
#define KFS_READONLY            SQLITE_READONLY // 8: Attempt to write a readonly database
#define KFS_INTERRUPT           SQLITE_INTERRUPT// 9: Operation terminated by sqlite3_interrupt()
#define KFS_IOERR               SQLITE_IOERR    // 10: Some kind of disk I/O error occurred
#define KFS_CORRUPT             SQLITE_CORRUPT  // 11: The database disk image is malformed
#define KFS_NOTFOUND            SQLITE_NOTFOUND // 12: Unknown opcode in sqlite3_file_control() OR Record not found
#define KFS_FULL                SQLITE_FULL     // 13: Insertion failed because database is full
#define KFS_CANTOPEN            SQLITE_CANTOPEN // 14: Unable to open the database file
#define KFS_PROTOCOL            SQLITE_PROTOCOL // 15: Database lock protocol error
#define KFS_EMPTY               SQLITE_EMPTY    // 16: Internal use only
#define KFS_SCHEMA              SQLITE_SCHEMA   // 17: The database schema changed
#define KFS_TOOBIG              SQLITE_TOOBIG   // 18: String or BLOB exceeds size limit
#define KFS_CONSTRAINT          SQLITE_CONSTRAINT//19: Abort due to constraint violation
#define KFS_MISMATCH            SQLITE_MISMATCH // 20: Data type mismatch
#define KFS_MISUSE              SQLITE_MISUSE   // 21: Library used incorrectly
#define KFS_NOLFS               SQLITE_NOLFS    // 22: Uses OS features not supported on host
#define KFS_AUTH                SQLITE_AUTH     // 23: Authorization denied
#define KFS_FORMAT              SQLITE_FORMAT   // 24: Auxiliary database format error
#define KFS_RANGE               SQLITE_RANGE    // 25: 2nd parameter to sqlite3_bind out of range
#define KFS_NOTADB              SQLITE_NOTADB   // 26: File opened that is not a database file
#define KFS_NOTICE              SQLITE_NOTICE   // 27: Notifications from sqlite3_log()
#define KFS_WARNING             SQLITE_WARNING  // 28: Warnings from sqlite3_log()
// Custom KFS Status Codes (Ensure they don't clash with potential future SQLite codes)
#define KFS_PERMISSION_DENIED   99  // Specific permission check failure
#define KFS_INVALID_ARGUMENT    100 // Function called with invalid parameters
#define KFS_VALIDATION_FAILED   101 // e.g., Script validation failure

/* Permission types (kfs_check_permission) */
#define KFS_PERM_READ           1
#define KFS_PERM_WRITE          2
#define KFS_PERM_DELETE         3

/* Database handle struct */
typedef struct {
    sqlite3* artifacts_db;  /* Connection for artifacts.db (holds Assets table) */
    sqlite3* arch_db;       /* Connection for architecture.db (Topics, Epics, Notes, Links) */
    sqlite3* registry_db;   /* Connection for registry.db (Users, SecuritySchemes) */
} GameDB;

/* --- Core Data Structures --- */

/* Actor structure (replaces KFS_User) - Represents Users, Groups, Companies */
typedef struct {
    int id;                 // Internal DB ID (INTEGER PRIMARY KEY)
    uint64_t uuid;          // Globally Unique KFS UUID (INTEGER)
    char* actor_type;       // "USER", "GROUP", "COMPANY", "SYSTEM", etc. (TEXT)
    char* name;             // Username, Group name, Company name (TEXT)
    char* role;             // Specific role (e.g., "admin", "developer") - May only apply to USER type?
    int is_active;          // 1 for active, 0 for inactive (INTEGER)
    // --- Populated during load (optional, for groups) ---
    // KFS_Actor** members; // Array of member actors (if type is GROUP/COMPANY)
    // int member_count;
} KFS_Actor;

/* Note structure */
typedef struct {
    int id;
    int domain_id;
    uint64_t creator_uuid;
    int owner_actor_id;
    int security_scheme_id;
    char* content;
    char* created_at;
    char* updated_at;
} KFS_Note;

/* User structure */
typedef struct {
    int id;
    uint64_t uuid;          // CHANGED from char*
    char* username;
    char* role;
    int is_active;
} KFS_User;

/* Security Scheme structure */
typedef struct {
    int actor_id;
    uint64_t actor_uuid;
    char* actor_name;
    char* actor_type;
    int can_read;
    int can_write;
    int can_delete;
} KFS_AllowedActor;

// And KFS_SecurityScheme is updated to use it:
typedef struct {
    int id;
    int domain_id;
    char* name;
    uint64_t creator_uuid;
    int owner_actor_id;
    KFS_AllowedActor* allowed_actors; // Array of structs
    int allowed_actor_count;
    char* created_at; // Added for consistency
    char* updated_at; // Added for consistency
} KFS_SecurityScheme;

/* Asset structure (Represents a row in artifacts.db.Assets + loaded related data) */
typedef struct {
    int id;                 // The Artifact ID (matches in both DBs)
    // --- From architecture.db.Artifacts ---
    char* type;
    char* name;
    char* format;
    uint64_t creator_uuid;
    int owner_actor_id;     // ID of the owning actor for the Artifact metadata/entry
    int security_scheme_id; // Scheme applied to the Artifact metadata/entry
    // --- From artifacts.db.Assets ---
    void* data;
    size_t data_size;
    char* text_data;
    char* metadata;         // JSON metadata
    // Note: Ownership/Security on Assets table might be redundant if always tied to Artifacts entry?
    // Let's assume for now ownership/security applies primarily at the Artifacts level.
    // We might remove owner/security from Assets table later if analysis confirms redundancy.

    // --- Populated during load ---
    char** topics;
    int topic_count;
    KFS_Note** notes;
    int note_count;
} KFS_Asset;

typedef struct {
    int id;                     // Artifact ID
    int domain_id;              // Domain ID
    char* type;                 // Artifact type (allocated, caller must free)
    char* name;                 // Artifact name (allocated)
    char* format;               // Artifact format (allocated, may be NULL)
    uint64_t creator_uuid;      // Creator UUID
    int owner_actor_id;         // Owner actor ID
    int security_scheme_id;     // Security scheme ID (-1 if none)
    char* created_at;           // Creation timestamp (allocated)
    char* updated_at;           // Update timestamp (allocated)
    uint8_t* data;              // Asset binary data (allocated, may be NULL)
    size_t data_size;           // Size of binary data
    char* text_data;            // Asset text data (allocated, may be NULL)
    char* metadata;             // Asset metadata (allocated, may be NULL)
} KFS_Artifact;

/* Topic structure */
typedef struct {
    int id;
    int domain_id;
    uint64_t creator_uuid;
    int owner_actor_id;
    int security_scheme_id;
    char* name; // Allocated string
    char** epics; // Array of epic *names* (assuming strings)
    int epic_count;
    char** related_topics; // Array of related topic *names* (assuming strings)
    int* is_subtopic; // Array of flags
    int related_count;
    KFS_Note** notes; // Array of pointers to KFS_Note structs
    int note_count;
    // Timestamps if added
    char* created_at;
    char* updated_at;
} KFS_Topic;

/* Epic structure */
typedef struct {
    int id;
    int domain_id;
    uint64_t creator_uuid;
    int owner_actor_id;
    int security_scheme_id;
    char* name; // Allocated string
    char* description; // ADDED description, Allocated string
    KFS_Note** notes; // Array of pointers to KFS_Note structs
    int note_count;
    // Timestamps if added
    char* created_at;
    char* updated_at;
} KFS_Epic;

typedef struct {
    int id;
    uint64_t uuid;
    char* name;
    char* role;
    char* actor_type;
    int is_active;
    int* group_ids;
    char** group_names;
    int group_count;
    int* security_scheme_ids;
    char** security_scheme_names;
    int security_scheme_count;
    int* owned_artifact_ids;
    int* owned_note_ids;
    int* owned_topic_ids;
    int* owned_epic_ids;
    int owned_artifact_count;
    int owned_note_count;
    int owned_topic_count;
    int owned_epic_count;
    int* created_artifact_ids;
    int* created_note_ids;
    int* created_topic_ids;
    int* created_epic_ids;
    int created_artifact_count;
    int created_note_count;
    int created_topic_count;
    int created_epic_count;
    int user_file_epic_id;      // ID of the user file epic (0 if none or inaccessible)
    int* linked_epic_ids;       // IDs of linked domain-specific epics
    int linked_epic_count;      // Number of linked epics
} KFS_UserInfo;

/* Struct for returning list results */
typedef struct {
    int id;
    char* name; // Caller must free
    char* type; // Caller must free
} KFS_ArtifactInfo;

/* --- Function Declarations --- */

/**
 * @brief Returns the KFS library version as a human-readable string.
 *
 * Format: "MAJOR.MINOR.PATCH[REVISION] (DESCRIPTION)"
 * Example: "2.2.0 (Impl fragment layout (core/auth/lc); maintenance release from 2.1, behavior unchanged)"
 *
 * Do not parse this string programmatically — use the KFS_VERSION_* macros from
 * kfs_version.h for compile-time version checks instead.
 *
 * @return const char* Pointer to a static null-terminated version string.
 */
const char* kfs_get_version_string(void);

int kfs_ensure_db_file_exists(const char* db_path);
int kfs_delete_db_file(const char* db_path);
int kfs_close(GameDB* db);
int kfs_read_first_user(GameDB* db, uint64_t requesting_actor_uuid, KFS_Actor* actor, sqlite3_stmt** query_stmt);
int kfs_read_next_user(GameDB* db, KFS_Actor* actor, sqlite3_stmt** query_stmt);
int kfs_read_user(GameDB* db, uint64_t requesting_actor_uuid, uint64_t target_actor_uuid, KFS_UserInfo* user_info);
void kfs_user_info_free(KFS_UserInfo* info);
int kfs_list_user_files(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** epic_ids, uint64_t** user_uuids, int* file_count);
int kfs_init(GameDB** db_handle, const char* artifacts_path, const char* arch_path, const char* registry_path);
int kfs_create_god_user(GameDB* db, uint64_t requesting_user_uuid, const char* name, int is_active, uint64_t* actor_uuid, int* actor_id);
int kfs_delete_user(GameDB* db, uint64_t requesting_user_uuid, uint64_t target_actor_uuid);
int kfs_update_user_name(GameDB* db, uint64_t requesting_actor_uuid, uint64_t target_actor_uuid, const char* new_name);
int kfs_create_user_file(GameDB* db, uint64_t requesting_actor_uuid, uint64_t user_uuid, int domain_id, const char* epic_name, int* epic_id);
int kfs_link_epic_to_user_file(GameDB* db, uint64_t requesting_actor_uuid, int user_file_epic_id, int domain_epic_id);
int kfs_get_user_file_epics(GameDB* db, uint64_t requesting_actor_uuid, uint64_t user_uuid, int** epic_ids, int* epic_count);
int kfs_unlink_epic_from_user_file(GameDB* db, uint64_t requesting_actor_uuid, int user_file_epic_id, int domain_epic_id);
int kfs_add_actor(GameDB* db, uint64_t requesting_actor_uuid, const char* actor_type, const char* name, const char* role, int is_active, uint64_t* actor_uuid, int* actor_id);
int kfs_get_actor(GameDB* db, uint64_t requesting_actor_uuid, uint64_t actor_uuid, KFS_Actor* actor);
int kfs_get_actor_by_uuid(GameDB* db, uint64_t requesting_user_uuid, uint64_t target_actor_uuid, KFS_Actor* actor);
int kfs_get_actor_by_name(GameDB* db, uint64_t requesting_user_uuid, const char* name_to_find, KFS_Actor* actor);
int kfs_update_actor_role(GameDB* db, uint64_t requesting_user_uuid, int target_actor_id, const char* new_role);
int kfs_set_actor_active(GameDB* db, uint64_t requesting_actor_uuid, uint64_t actor_uuid, int is_active);
int kfs_deactivate_actor(GameDB* db, uint64_t requesting_user_uuid, uint64_t target_actor_uuid);
int kfs_reactivate_actor(GameDB* db, uint64_t requesting_user_uuid, uint64_t target_actor_uuid);
void kfs_actor_free_contents(KFS_Actor* actor);
void kfs_actor_free(KFS_Actor* actor);
int kfs_add_member_to_group(GameDB* db, uint64_t requesting_user_uuid, int group_actor_id, int member_actor_id);
int kfs_remove_member_from_group(GameDB* db, uint64_t requesting_user_uuid, int group_actor_id, int member_actor_id);
int kfs_is_member_of(GameDB* db, int potential_member_actor_id, int group_actor_id, int* is_member);
int kfs_add_actor_to_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id, int actor_id, int can_read, int can_write, int can_delete);
int kfs_remove_actor_from_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id, int actor_id);
int kfs_get_actor_info_by_uuid(GameDB* db, uint64_t requesting_user_uuid, int domain_id, uint64_t actor_uuid, int* actor_id, char** actor_type, char** name, int* is_active, int* is_admin);
int kfs_check_permission(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int permission_type);
int kfs_list_scheme_actors(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id, const char* actor_type, int** actor_ids, int** can_read, int** can_write, int** can_delete, int* actor_count);
int kfs_create_security_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int owner_actor_id, const char* name, int* scheme_id);
int kfs_get_security_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id, KFS_SecurityScheme* scheme);
int kfs_add_domain(GameDB* db, uint64_t requesting_user_uuid, const char* name, int owner_actor_id, const char* description, int* domain_id);
int kfs_delete_domain(GameDB* db, uint64_t requesting_actor_uuid, int domain_id);
int kfs_update_domain(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, const char* name, int owner_actor_id, const char* description);
int kfs_add_actor_to_domain(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int actor_id);
int kfs_remove_actor_from_domain(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int actor_id);
int kfs_list_domains(GameDB* db, uint64_t requesting_actor_uuid, int** domain_ids, char*** domain_names, int* domain_count);
int kfs_set_entity_owner(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int new_owner_actor_id);
int kfs_set_entity_security_scheme(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int new_security_scheme_id);
int kfs_add_user_to_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id, uint64_t user_uuid);
int kfs_remove_user_from_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id, uint64_t user_uuid);
int kfs_delete_security_scheme(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int scheme_id);
int kfs_add_user(GameDB* db, uint64_t requesting_actor_uuid, const char* name, const char* role, int is_active, uint64_t* actor_uuid, int* actor_id);
int kfs_get_user(GameDB* db, int user_id, KFS_User* user);
int kfs_get_user_by_name(GameDB* db, const char* username, KFS_User* user);
int kfs_add_epic(GameDB* db, uint64_t requesting_actor_uuid, int owner_actor_id, const char* name, const char* description, int security_scheme_id, int domain_id, int* epic_id);
int kfs_get_epic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int epic_id, KFS_Epic* epic);
int kfs_get_epic_by_name(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, const char* name, KFS_Epic* epic);
int kfs_delete_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int epic_id);
int kfs_list_epics(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** epic_ids, char*** epic_names, int* epic_count);
int kfs_update_epic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int epic_id, const char* name, const char* description, int owner_actor_id, int security_scheme_id);
int kfs_save_text(GameDB* db, const char* type, const char* name, const char* format, const char* text_data, const char* metadata, const char** topics, int topic_count, int owner_id, int creator_id, int security_scheme_id, int* artifact_id);
int kfs_save_script(GameDB* db, const char* name, const char* format, const char* script_code, const char* metadata, const char** topics, int topic_count, int owner_id, int creator_id, int security_scheme_id, int* artifact_id);
int kfs_save_file(GameDB* db, const char* type, const char* name, const char* format, const char* file_path, const char* metadata, const char** topics, int topic_count, int owner_id, int creator_id, int security_scheme_id, int* artifact_id);
int kfs_list_topics(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** topic_ids, char*** topic_names, int* topic_count);
int kfs_add_topic(GameDB* db, uint64_t requesting_actor_uuid, int owner_actor_id, const char* name, int security_scheme_id, int domain_id, int* topic_id);
int kfs_delete_topic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int topic_id);
int kfs_get_topic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int topic_id, int* owner_actor_id, char** name, int* security_scheme_id, uint64_t* creator_uuid, char** created_at, char** updated_at);
int kfs_link_related_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int related_topic_id, int is_subtopic);
int kfs_link_related_topic_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, const char* related_topic_name, int is_subtopic);
int kfs_unlink_related_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int related_topic_id);
int kfs_load_subtopics(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, KFS_Topic** results, int* result_count);
int kfs_update_topic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int topic_id, const char* name, int owner_actor_id, int security_scheme_id);
int kfs_add_note(GameDB* db, uint64_t requesting_actor_uuid, int owner_actor_id, const char* content, int security_scheme_id, int domain_id, int* note_id);
int kfs_list_notes(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** note_ids, char*** note_contents, int* note_count);
int kfs_load_note(GameDB* db, int note_id, KFS_Note* note);
int kfs_assign_note(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int note_id);
int kfs_unassign_note(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int note_id);
int kfs_update_note(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int note_id, const char* content, int owner_actor_id, int security_scheme_id);
int kfs_get_note(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int note_id, int* owner_actor_id, char** content, int* security_scheme_id, uint64_t* creator_uuid, char** created_at, char** updated_at);
int kfs_delete_note(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int note_id);
int kfs_assign_epic_to_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int epic_id);
int kfs_create_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* name, const char* type, const char* format, int owner_actor_id, int security_scheme_id, const uint8_t* data, size_t data_size, const char* text_data, const char* metadata, int* artifact_id);
int kfs_link_asset_to_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, const void* data, size_t data_size, const char* text_data, const char* metadata);
int kfs_unlink_asset_from_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id);
int kfs_delete_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id);
int kfs_create_artifact_with_existing_asset(GameDB* db, uint64_t requesting_user_uuid, int domain_id, uint64_t creator_uuid, int owner_actor_id, const char* type, const char* name, const char* format, int security_scheme_id, int asset_id, int* artifact_id);
int kfs_update_artifact(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int artifact_id, const char* type, const char* name, const char* format, int owner_actor_id, int security_scheme_id, const uint8_t* data, size_t data_size, const char* text_data, const char* metadata);
int kfs_update_artifact_name(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, const char* new_name);
int kfs_assign_topic_to_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, int topic_id);
int kfs_assign_topic_to_artifact_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id, const char* topic_name);
int kfs_remove_topic_from_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, int topic_id);
int kfs_create_artifact_and_asset(GameDB* db, uint64_t creator_uuid, int owner_actor_id, int security_scheme_id, const char* type, const char* name, const char* format, const void* data, size_t data_size, const char* metadata, const char** topics, int topic_count, int* artifact_id);
int kfs_get_artifact(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int artifact_id, int* owner_actor_id, char** type, char** name, char** format, int* security_scheme_id, uint64_t* creator_uuid, char** created_at, char** updated_at, int* has_asset);
int kfs_load_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id, char** type, char** name, char** format, uint64_t* creator_uuid, int* owner_actor_id, int* security_scheme_id, uint8_t** data, size_t* data_size, char** text_data, char** metadata, char*** topics, int* topic_count, KFS_Note*** notes, int* note_count);
int kfs_erase_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id);
void kfs_artifact_info_free_contents(KFS_ArtifactInfo* info);
int kfs_list_artifacts_begin(GameDB* db, uint64_t requesting_user_uuid, int domain_id, sqlite3_stmt** query_stmt, KFS_ArtifactInfo* first_artifact_info);
int kfs_list_artifacts_next(sqlite3_stmt* query_stmt, KFS_ArtifactInfo* next_artifact_info);
void kfs_list_artifacts_end(GameDB* db, sqlite3_stmt* query_stmt);
int kfs_list_artifacts(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** artifact_ids, char*** artifact_names, char*** artifact_types, int* artifact_count);
int kfs_get_asset_data(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int artifact_id, uint8_t** data, size_t* data_size, char** text_data, char** metadata);
int kfs_delete_asset(GameDB* db, uint64_t requesting_user_uuid, int artifact_id);
int kfs_get_topic_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* name, KFS_Topic* topic);
int kfs_load_by_topic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, KFS_Asset** results, int* result_count);
int kfs_assign_epic_to_topic_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, const char* epic_name);
int kfs_remove_epic_from_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int epic_id);
int kfs_load_by_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* epic_name, KFS_Asset** results, int* result_count);
int kfs_handle_orphaned_artifacts(GameDB* db, int deactivated_actor_id);
int kfs_load_scripts_by_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* epic_name, const char* format, KFS_Asset** results, int* result_count);
int kfs_validate_script(const char* format, const char* script_code, char** error_msg);
void kfs_entity_free(void* entity, const char* entity_type);
void kfs_user_free_contents(KFS_User* user);
void kfs_user_free(KFS_User* user);
void kfs_security_scheme_free_contents(KFS_SecurityScheme* scheme);
void kfs_security_scheme_free(KFS_SecurityScheme* scheme);
void kfs_note_free_contents(KFS_Note* note);
void kfs_note_free(KFS_Note* note);
void kfs_asset_free_contents(KFS_Asset* asset);
void kfs_asset_free(KFS_Asset* asset);
void kfs_assets_free(KFS_Asset* assets, int count);
void kfs_topic_free_contents(KFS_Topic* topic);
void kfs_topic_free(KFS_Topic* topic);
void kfs_topics_free(KFS_Topic* topics, int count);
void kfs_epic_free_contents(KFS_Epic* epic);
void kfs_epic_free(KFS_Epic* epic);
void kfs_epics_free(KFS_Epic* epics, int count);

#endif /* KFS_API_H */

