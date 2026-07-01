/**
 * @file kfs_impl_lc.h
 * @brief KFS implementation — linking & content (architecture.db + artifacts.db).
 *
 * LC = Linking & Content (not lifecycle — see kfs_impl_core.h).
 * Included only via kfs_impl.h when KFS_IMPLEMENTATION is defined.
 *
 * Split phase: S5 (extracted from kfs_impl.h).
 */
#ifndef KFS_IMPL_LC_H
#define KFS_IMPL_LC_H

#ifdef KFS_IMPLEMENTATION

/* SECTION: epics (both monolith banners) — S5.1 */
/* SECTION: topics — S5.2 */
/* SECTION: notes (both monolith banners) — S5.3 */
/* SECTION: linking + artifacts — S5.4 */
/* SECTION: advanced load (both monolith banners) — S5.5 */
/* SECTION: misc (validate_script, orphans) — S5.6 */

/* ============================================================================== */
/* ==                     EPIC MANAGEMENT w/ Permissions                     == */
/* ============================================================================== */

/**
 * @brief Adds a new epic to architecture.db.Epics in a specified domain.
 * Requires domain access and WRITE permission for the requesting user.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param owner_actor_id ID of the owning actor (user or group).
 * @param name Name of the epic.
 * @param description Optional description of the epic.
 * @param security_scheme_id ID of the security scheme (must be in the same domain).
 * @param domain_id ID of the domain.
 * @param epic_id Output parameter for the created epic ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_add_epic(GameDB* db, uint64_t requesting_actor_uuid, int owner_actor_id, const char* name, const char* description, int security_scheme_id, int domain_id, int* epic_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || owner_actor_id <= 0 || !name || domain_id <= 0 || !epic_id) {
        fprintf(stderr, "[ERROR] kfs_add_epic: Invalid arguments (requesting_actor_uuid=%llu, owner_actor_id=%d, name=%s, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, owner_actor_id, name ? name : "NULL", domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *epic_id = -1;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_add_epic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Get Requester Actor ID and Check Domain Access ---
    int requester_actor_id = -1;
    const char* sql_get_requester_id = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_get_requester_id, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_epic (get requester id) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)requesting_actor_uuid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        requester_actor_id = sqlite3_column_int(stmt, 0);
        int is_active = sqlite3_column_int(stmt, 1);
        if (!is_active) {
            fprintf(stderr, "[ERROR] kfs_add_epic: Requester UUID %llu is inactive.\n",
                    (unsigned long long)requesting_actor_uuid);
            rc = KFS_PERMISSION_DENIED;
        } else {
            rc = KFS_OK;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_epic: Requester UUID %llu not found.\n",
                (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_add_epic (get requester id) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != KFS_OK) {
        goto cleanup;
    }

    // Check domain access
    int has_domain_access = 0;
    const char* sql_check_domain = "SELECT 1 FROM DomainActors WHERE domain_id = ? AND actor_id = ? LIMIT 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_domain, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_epic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_int(stmt, 2, requester_actor_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_domain_access = 1;
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_epic (check domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (!has_domain_access) {
        const char* sql_check_group_domain = "SELECT DA.actor_id FROM DomainActors DA JOIN Actors A ON DA.actor_id = A.id "
                                            "WHERE DA.domain_id = ? AND A.actor_type IN ('GROUP', 'COMPANY');";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_group_domain, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_add_epic (check group domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, domain_id);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int group_id = sqlite3_column_int(stmt, 0);
            if (is_user_in_group(db, requester_actor_id, group_id)) {
                has_domain_access = 1;
                break;
            }
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_add_epic (check group domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!has_domain_access) {
            fprintf(stderr, "[ERROR] kfs_add_epic: Requester %llu lacks access to domain %d.\n",
                    (unsigned long long)requesting_actor_uuid, domain_id);
            rc = KFS_PERMISSION_DENIED;
            goto cleanup;
        }
        rc = KFS_OK;
    }

    // --- Verify Owner Exists ---
    const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_epic (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, owner_actor_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "[ERROR] kfs_add_epic: Owner actor ID %d not found.\n", owner_actor_id);
        rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Verify Security Scheme (if provided) ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_add_epic (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, security_scheme_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_add_epic: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
            rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    // --- Create Epic ---
    char* timestamp = get_current_timestamp();
    if (!timestamp) {
        fprintf(stderr, "[ERROR] kfs_add_epic: Memory allocation failed for timestamp.\n");
        goto cleanup;
    }

    const char* sql_insert = "INSERT INTO Epics (domain_id, name, description, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_epic (insert) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description ? description : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)requesting_actor_uuid);
    sqlite3_bind_int(stmt, 5, owner_actor_id);
    if (security_scheme_id >= 0) {
        sqlite3_bind_int(stmt, 6, security_scheme_id);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    sqlite3_bind_text(stmt, 7, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, timestamp, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *epic_id = (int)sqlite3_last_insert_rowid(db->arch_db);
    }
    sqlite3_finalize(stmt);
    kfs_mem_free(timestamp);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_epic (insert) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) {
            rc = KFS_CONSTRAINT;
        }
        goto cleanup;
    }

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_add_epic: Commit failed.\n");
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_add_epic: Successfully created epic '%s' with ID %d in domain %d.\n", name, *epic_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}


/**
 * @brief Retrieves an epic from a specified domain.
 * Requires READ permission on the epic and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the epic.
 * @param epic_id ID of the epic to retrieve.
 * @param epic Output parameter struct KFS_Epic to be filled (caller must free contents using kfs_epic_free_contents).
 *             Note: Populates basic fields, does not load associated notes by default here.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_get_epic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int epic_id, KFS_Epic* epic) {
    // --- Input Validation ---
     if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || epic_id <= 0 || !epic) {
        fprintf(stderr, "[ERROR] kfs_get_epic: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, epic_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, epic_id);
        return KFS_INVALID_ARGUMENT;
    }
    // Initialize output struct
    memset(epic, 0, sizeof(KFS_Epic));
    epic->id = epic_id;
    epic->domain_id = domain_id; // Store known IDs

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_epic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: READ on the Epic ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Epic", epic_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_epic: Permission check failed for epic %d (rc=%d).\n", epic_id, rc);
        goto cleanup;
    }

    // --- Verify Epic Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Epics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, epic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_get_epic: Epic ID %d does not belong to domain %d.\n", epic_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_get_epic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Fetch Epic Details ---
     // Note: Added description, created_at, updated_at to SELECT
    const char* sql_epic = "SELECT name, description, owner_actor_id, security_scheme_id, creator_uuid, created_at, updated_at "
                           "FROM Epics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_epic, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_get_epic (fetch) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, epic_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* epic_name = sqlite3_column_text(stmt, 0);
        const unsigned char* epic_desc = sqlite3_column_text(stmt, 1); // Description added
        const unsigned char* epic_created = sqlite3_column_text(stmt, 5); // created_at added
        const unsigned char* epic_updated = sqlite3_column_text(stmt, 6); // updated_at added

        epic->owner_actor_id = sqlite3_column_int(stmt, 2);
        epic->security_scheme_id = sqlite3_column_int(stmt, 3);
        epic->creator_uuid = (uint64_t)sqlite3_column_int64(stmt, 4);

        if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
            epic->security_scheme_id = -1;
        }

        // Allocate memory for output strings
        epic->name = epic_name ? KFS_STRDUP((const char*)epic_name) : NULL;
        epic->description = epic_desc ? KFS_STRDUP((const char*)epic_desc) : NULL; // Populate description
        // Populate timestamps if needed in struct (currently not in KFS_Epic definition)
        // epic->created_at = epic_created ? KFS_STRDUP((const char*)epic_created) : NULL;
        // epic->updated_at = epic_updated ? KFS_STRDUP((const char*)epic_updated) : NULL;

        // Check allocation failures
        if ((epic_name && !epic->name) || (epic_desc && !epic->description) /* || ... other checks ... */) {
            rc = KFS_NOMEM;
        } else {
            rc = KFS_OK; // Reset rc
        }
        // Note: Does not load notes here by default
        epic->notes = NULL;
        epic->note_count = 0;

    } else { // Should not happen due to permission check
        rc = KFS_NOTFOUND;
    }
    sqlite3_finalize(stmt); stmt = NULL;
    if (rc != KFS_OK) { goto cleanup; }


    // --- Commit Transactions ---
     if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_epic: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Free allocated memory
    }

    fprintf(stdout, "[INFO] kfs_get_epic: Successfully retrieved epic %d in domain %d.\n", epic_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    kfs_epic_free_contents(epic); // Free potentially partially allocated struct contents
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}


int kfs_get_epic_by_name(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, const char* name, KFS_Epic* epic) {
     if (!db || !db->arch_db || !name || !epic || requesting_actor_uuid == 0 || domain_id <= 0) return KFS_INVALID_ARGUMENT;
     int epic_id = -1;
     int rc = kfs_get_epic_id_by_name(db, domain_id, name, &epic_id);
     if (rc != KFS_OK) return rc;
     return kfs_get_epic(db, requesting_actor_uuid, domain_id, epic_id, epic);
}

/**
 * @brief Deletes an epic from a specified domain.
 * Requires DELETE permission on the Epic itself and domain access.
 * Handles cascading deletes for related items (EpicAssignments, RelatedEpics).
 * Manually deletes associated EntityNotes links.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the epic.
 * @param epic_id ID of the epic to delete.
 * @return KFS_OK on success (even if epic didn't exist), KFS_INVALID_ARGUMENT,
 *         KFS_PERMISSION_DENIED, or SQLite error.
 */
int kfs_delete_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int epic_id) {
    // --- Input Validation ---
     if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || epic_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_delete_epic: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, epic_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, epic_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_epic: Failed to begin transaction.\n");
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        exec_sql(db->registry_db, "ROLLBACK;", "registry");
        return KFS_ERROR;
    }

    // --- Permission Check: DELETE on the Epic ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Epic", epic_id, KFS_PERM_DELETE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
             fprintf(stderr, "[INFO] kfs_delete_epic: Epic ID %d not found or permission check failed with NOTFOUND, treating as success for delete.\n", epic_id);
             rc = KFS_OK; // Not found is OK for delete
             goto commit; // Skip actual deletion steps
        }
        fprintf(stderr, "[ERROR] kfs_delete_epic: Permission check failed for epic %d (rc=%d).\n", epic_id, rc);
        goto cleanup; // Permission denied or DB error
    }

     // --- Verify Epic Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Epics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, epic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_delete_epic: Epic ID %d does not belong to domain %d.\n", epic_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup; // Should be caught by perm check
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_epic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Perform Deletions ---

    // 1. Manually delete associated notes links
    const char* sql_del_notes = "DELETE FROM EntityNotes WHERE entity_type = 'Epic' AND entity_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_del_notes, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, epic_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_delete_epic: Failed deleting note links (rc=%d).\n", rc);
            goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_epic (del notes) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    // 2. Delete the Epic itself (Cascades handle EpicAssignments, RelatedEpics if set correctly in kfs_init)
    const char* sql_del_epic = "DELETE FROM Epics WHERE id = ? AND domain_id = ?;";
     rc = sqlite3_prepare_v2(db->arch_db, sql_del_epic, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, epic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            if (sqlite3_changes(db->arch_db) == 0) {
                 fprintf(stderr, "[WARN] kfs_delete_epic: Epic ID %d not found during delete (though permission check passed).\n", epic_id);
            }
             rc = KFS_OK; // Reset rc
        } else {
             fprintf(stderr, "[ERROR] kfs_delete_epic: Failed deleting epic record (rc=%d).\n", rc);
        }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_delete_epic (del epic) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    if (rc != KFS_OK) goto cleanup;


commit:
    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_epic: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

    if (rc == KFS_OK) { // Only log success if final rc is OK
         fprintf(stdout, "[INFO] kfs_delete_epic: Successfully processed delete for epic %d in domain %d by user %llu.\n",
                epic_id, domain_id, (unsigned long long)requesting_user_uuid);
    }
    return rc; // KFS_OK or KFS_ERROR if commit failed

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}


/* ============================================================================== */
/* ==                              EPIC MANAGEMENT FUNCTIONS                  == */
/* ============================================================================== */

/**
 * @brief Lists all epics in a specified domain that the requesting actor has READ permission for.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain to query.
 * @param epic_ids Output array of epic IDs (caller must free).
 * @param epic_names Output array of epic names (caller must free each string).
 * @param epic_count Output number of epics.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_list_epics(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** epic_ids, char*** epic_names, int* epic_count) {
    // --- Input Validation ---
     if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || !epic_ids || !epic_names || !epic_count) {
        fprintf(stderr, "[ERROR] kfs_list_epics: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *epic_ids = NULL; *epic_names = NULL; *epic_count = 0;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    int* temp_ids = NULL;
    char** temp_names = NULL;
    int count = 0;
    int capacity = 16;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_epics: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Check Domain READ Access ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Domain", domain_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_epics: Domain access check failed for domain %d (rc=%d).\n", domain_id, rc);
        goto cleanup;
    }

    // --- Fetch All Epics in Domain ---
    const char* sql_epics = "SELECT id, name FROM Epics WHERE domain_id = ? ORDER BY name;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_epics, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_list_epics (fetch) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, domain_id);

    // Allocate initial arrays
    temp_ids = KFS_MALLOC(capacity * sizeof(int));
    temp_names = KFS_MALLOC(capacity * sizeof(char*));
    if (!temp_ids || !temp_names) { rc = KFS_NOMEM; goto cleanup;}
    memset(temp_ids, 0, capacity * sizeof(int));
    memset(temp_names, 0, capacity * sizeof(char*));


    // --- Iterate and Check Permission for Each Epic ---
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int current_epic_id = sqlite3_column_int(stmt, 0);
        const unsigned char* current_epic_name_raw = sqlite3_column_text(stmt, 1);

        // Check READ permission on the specific epic
        int perm_rc = kfs_check_permission(db, requesting_actor_uuid, "Epic", current_epic_id, KFS_PERM_READ);

        if (perm_rc == KFS_OK) {
            // Permission granted, add to results
            if (count >= capacity) {
                capacity *= 2;
                int* new_ids = KFS_REALLOC(temp_ids, capacity * sizeof(int));
                char** new_names = KFS_REALLOC(temp_names, capacity * sizeof(char*));
                if (!new_ids || !new_names) {
                    kfs_mem_free(new_ids ? new_ids : temp_ids);
                    kfs_mem_free(new_names ? new_names : temp_names);
                    temp_ids = NULL;
                    temp_names = NULL;
                    rc = KFS_NOMEM;
                    break;
                }
                temp_ids = new_ids;
                temp_names = new_names;
                memset(temp_ids + count, 0, (capacity / 2) * sizeof(int));
                memset(temp_names + count, 0, (capacity / 2) * sizeof(char*));
            }

            temp_ids[count] = current_epic_id;
            temp_names[count] = current_epic_name_raw ? KFS_STRDUP((const char*)current_epic_name_raw) : NULL;
            if (current_epic_name_raw && !temp_names[count]) { rc = KFS_NOMEM; break; }
            count++;
        } else if (perm_rc == KFS_PERMISSION_DENIED || perm_rc == KFS_NOTFOUND) {
             fprintf(stderr, "[INFO] kfs_list_epics: Skipping epic %d due to permission check result %d.\n", current_epic_id, perm_rc);
        } else {
            fprintf(stderr, "[ERROR] kfs_list_epics: Error checking permission for epic %d (rc=%d).\n", current_epic_id, perm_rc);
            rc = perm_rc; break; // Exit loop
        }
    } // End while loop
    sqlite3_finalize(stmt); stmt = NULL;

    // Check final state of loop/permission checks
    if (rc != SQLITE_DONE && rc != KFS_OK && rc != KFS_NOMEM) { goto cleanup; }
    if (rc == KFS_NOMEM) { goto cleanup; }

    // --- Finalize Results ---
    if (count == 0) {
        kfs_mem_free(temp_ids); temp_ids = NULL;
        kfs_mem_free(temp_names); temp_names = NULL;
        fprintf(stderr, "[INFO] kfs_list_epics: No accessible epics found in domain %d for user %llu.\n", domain_id, (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND; // Signal no results found
        goto commit; // Still need to commit/rollback cleanly
    }

    *epic_ids = temp_ids;
    *epic_names = temp_names;
    *epic_count = count;
    rc = KFS_OK; // Set final status to OK

commit:
    // --- Commit Transactions ---
     if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_epics: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Free allocated results and rollback
    }

    if (rc == KFS_OK) { // Only log success if final rc is OK
         fprintf(stdout, "[INFO] kfs_list_epics: Successfully retrieved %d accessible epics in domain %d.\n", count, domain_id);
    }
    return rc; // KFS_OK or KFS_NOTFOUND

cleanup:
    // Free allocated memory if an error occurred before success
    sqlite3_finalize(stmt); // Ensure stmt finalized
    if (temp_ids) kfs_mem_free(temp_ids);
    if (temp_names) {
        for (int i = 0; i < count; i++) kfs_mem_free(temp_names[i]); // Free individual strings
        kfs_mem_free(temp_names);
    }
    // Reset output params on error
     *epic_ids = NULL; *epic_names = NULL; *epic_count = 0;
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code
}

/**
 * @brief Updates metadata for an epic in a specified domain.
 * Requires WRITE permission on the epic and domain access.
 * Validates new owner and scheme (must be in the same domain).
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the epic.
 * @param epic_id ID of the epic to update.
 * @param name New name (optional, NULL to keep unchanged).
 * @param description New description (optional, NULL to keep unchanged).
 * @param owner_actor_id New owner actor ID (optional, <= 0 to keep unchanged).
 * @param security_scheme_id New security scheme ID (optional, < 0 to remove/keep NULL, >= 0 to set/update).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_update_epic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int epic_id, const char* name, const char* description, int owner_actor_id, int security_scheme_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || epic_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_update_epic: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, epic_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, epic_id);
        return KFS_INVALID_ARGUMENT;
    }
    // Ensure at least one field is being updated
    if (!name && !description && owner_actor_id <= 0 && security_scheme_id < -1) {
         fprintf(stderr, "[INFO] kfs_update_epic: No update parameters provided for epic %d.\n", epic_id);
         return KFS_OK; // Nothing to do
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    char* timestamp = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_epic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Epic ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Epic", epic_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_epic: Permission check failed for epic %d (rc=%d).\n", epic_id, rc);
        goto cleanup;
    }

    // --- Verify Epic Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Epics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, epic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_update_epic: Epic ID %d does not belong to domain %d.\n", epic_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_update_epic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    // --- Verify New Owner (if provided) ---
     if (owner_actor_id > 0) {
        const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ? AND is_active = 1;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
         if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, owner_actor_id);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_update_epic: New owner actor ID %d not found or inactive.\n", owner_actor_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
            rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_update_epic (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Verify New Security Scheme (if setting/updating) ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
         if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, security_scheme_id);
            sqlite3_bind_int(stmt, 2, domain_id); // Ensure scheme is in the SAME domain
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_update_epic: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
             rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_update_epic (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Update Epic ---
    timestamp = get_current_timestamp();
    if (!timestamp) { rc = KFS_NOMEM; goto cleanup; }

    // Build SET clause dynamically
    char set_clause[512] = "";
    int param_index = 1;
    int needs_comma = 0;

    if (name) { strcat(set_clause, "name = ?"); needs_comma = 1; }
    if (description) { if(needs_comma) strcat(set_clause, ", "); strcat(set_clause, "description = ?"); needs_comma = 1; }
    if (owner_actor_id > 0) { if(needs_comma) strcat(set_clause, ", "); strcat(set_clause, "owner_actor_id = ?"); needs_comma = 1; }
    if (security_scheme_id >= -1) { if(needs_comma) strcat(set_clause, ", "); strcat(set_clause, "security_scheme_id = ?"); needs_comma = 1; }
    if(needs_comma) strcat(set_clause, ", ");
    strcat(set_clause, "updated_at = ?"); // Always update timestamp

    char sql_update[650];
    snprintf(sql_update, sizeof(sql_update), "UPDATE Epics SET %s WHERE id = ? AND domain_id = ?;", set_clause);

    rc = sqlite3_prepare_v2(db->arch_db, sql_update, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_update_epic (update) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    param_index = 1;
    if (name) sqlite3_bind_text(stmt, param_index++, name, -1, SQLITE_STATIC);
    if (description) sqlite3_bind_text(stmt, param_index++, description, -1, SQLITE_STATIC);
    if (owner_actor_id > 0) sqlite3_bind_int(stmt, param_index++, owner_actor_id);
    if (security_scheme_id >= -1) {
        if (security_scheme_id == -1) sqlite3_bind_null(stmt, param_index++);
        else sqlite3_bind_int(stmt, param_index++, security_scheme_id);
    }
    sqlite3_bind_text(stmt, param_index++, timestamp, -1, SQLITE_STATIC); // updated_at
    sqlite3_bind_int(stmt, param_index++, epic_id); // WHERE id = ?
    sqlite3_bind_int(stmt, param_index++, domain_id); // WHERE domain_id = ?

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_epic (update) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT_UNIQUE) rc = KFS_CONSTRAINT; // Name+Domain conflict
        else rc = KFS_ERROR;
        goto cleanup;
    }

    if (sqlite3_changes(db->arch_db) == 0) {
        // Should not happen due to earlier checks
        fprintf(stderr, "[ERROR] kfs_update_epic: Epic ID %d not found during update.\n", epic_id);
        rc = KFS_NOTFOUND;
        goto cleanup;
    }
     rc = KFS_OK; // Reset rc

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_epic: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup;
    }

    kfs_mem_free(timestamp); // Free timestamp only on success or commit failure
    fprintf(stdout, "[INFO] kfs_update_epic: Successfully updated epic %d in domain %d.\n", epic_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    kfs_mem_free(timestamp); // Free timestamp if allocated
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}


/**
 * @brief Saves a new artifact with TEXT data.
 * Creates corresponding entries in architecture.db.Artifacts and artifacts.db.Assets.
 * Assigns initial topics if provided. Manages transactions.
 *
 * @param db The GameDB handle.
 * @param type The artifact type string.
 * @param name The artifact name string.
 * @param format The artifact format string.
 * @param text_data The TEXT content for the artifact.
 * @param metadata Optional JSON metadata string (can be NULL).
 * @param topics Optional array of topic names to assign initially (can be NULL).
 * @param topic_count Number of topics in the array (must be 0 if topics is NULL).
 * @param owner_id User ID of the owner.
 * @param creator_id User ID of the creator.
 * @param security_scheme_id Security scheme ID (-1 or 0 for none).
 * @param artifact_id Output parameter for the ID of the newly created artifact.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_NOMEM, KFS_CONSTRAINT,
 *         KFS_NOTFOUND (if a topic name isn't found), or other SQLite error code.
 */
int kfs_save_text(GameDB* db,
                  const char* type, const char* name, const char* format,
                  const char* text_data, // Primary data is text
                  const char* metadata,
                  const char** topics, int topic_count,
                  int owner_id, int creator_id, int security_scheme_id,
                  int* artifact_id) // Output parameter
{
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !artifact_id ||
        !type || !name || // Require type and name
        // text_data can be NULL or empty, maybe? Let's allow NULL for now.
        owner_id <= 0 || creator_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_save_text: Invalid argument (NULL pointers or invalid IDs).\n");
        return KFS_INVALID_ARGUMENT;
    }
    if (topic_count < 0 || (topic_count > 0 && !topics)) {
         fprintf(stderr, "[ERROR] kfs_save_text: Invalid topic arguments (count=%d, topics_ptr=%p).\n", topic_count, (void*)topics);
        return KFS_INVALID_ARGUMENT;
    }
    *artifact_id = -1; // Reset output parameter

    int rc = KFS_OK;
    int current_artifact_id = -1;

    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK) return KFS_ERROR;
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK) {
        exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); return KFS_ERROR;
    }

    // --- Call internal save function (inserts into Artifacts and Assets) ---
    // Pass NULL for data/data_size, provide text_data
    rc = kfs_save_asset(db, type, name, format, creator_id, owner_id, security_scheme_id, NULL /* data */, 0 /* data_size */, text_data, metadata, &current_artifact_id);

    if (rc != KFS_OK) {
        goto save_text_rollback; // Error occurred during insert
    }
     if (current_artifact_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_save_text: Internal save succeeded but returned invalid artifact ID (%d).\n", current_artifact_id);
        rc = KFS_INTERNAL;
        goto save_text_rollback;
    }

    // --- Assign topics if provided ---
    for (int i = 0; i < topic_count; i++) {
        if (topics[i] && strlen(topics[i]) > 0) {
            rc = kfs_link_topic_to_artifact_by_name_internal(db, 0, current_artifact_id, topics[i]);
            if (rc != KFS_OK) {
                fprintf(stderr, "[ERROR] kfs_save_text: Failed during assignment of topic '%s' (Error: %d).\n", topics[i], rc);
                goto save_text_rollback; // Rollback transaction
            }
        } else {
             fprintf(stderr, "[WARN] kfs_save_text: Skipping empty or NULL topic name at index %d.\n", i);
        }
    }

    // --- Commit Transactions ---
    int commit_rc1 = exec_sql(db->artifacts_db, "COMMIT;", "artifacts");
    int commit_rc2 = exec_sql(db->arch_db, "COMMIT;", "architecture");

    if (commit_rc1 == KFS_OK && commit_rc2 == KFS_OK) {
        *artifact_id = current_artifact_id;
        return KFS_OK;
    } else {
        fprintf(stderr, "[ERROR] kfs_save_text: Commit failed (artifacts_rc=%d, arch_rc=%d). Attempting rollback.\n", commit_rc1, commit_rc2);
        exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); // Ignore errors
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");   // Ignore errors
        return KFS_ERROR;
    }

save_text_rollback:
    // An error occurred before commit, rollback both transactions
    fprintf(stderr, "[ERROR] kfs_save_text: Rolling back transaction due to error %d.\n", rc);
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); // Ignore errors
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");   // Ignore errors
    return rc; // Return the specific error code
}


/**
 * @brief Saves a new script artifact (convenience wrapper for kfs_save_text).
 * Sets the artifact type to "script".
 */
int kfs_save_script(GameDB* db,
                    const char* name, const char* format,
                    const char* script_code,
                    const char* metadata,
                    const char** topics, int topic_count,
                    int owner_id, int creator_id, int security_scheme_id,
                    int* artifact_id) // Output parameter
{
    // Simple wrapper - calls kfs_save_text with type="script"
    // Validation will be handled by kfs_save_text
    return kfs_save_text(db, "script", name, format, script_code, metadata,
                         topics, topic_count, owner_id, creator_id, security_scheme_id, artifact_id);
}


/**
 * @brief Saves a new artifact by reading content from a file (as BLOB).
 * Creates corresponding entries in architecture.db.Artifacts and artifacts.db.Assets.
 * Assigns initial topics if provided. Manages transactions.
 * NOTE: Reads the entire file into memory first.
 */
int kfs_save_file(GameDB* db,
                  const char* type, const char* name, const char* format,
                  const char* file_path,    // Path to the file to read data from
                  const char* metadata,
                  const char** topics, int topic_count,
                  int owner_id, int creator_id, int security_scheme_id,
                  int* artifact_id) // Output parameter
{
    // --- Input Validation ---
     if (!db || !db->artifacts_db || !db->arch_db || !artifact_id ||
        !type || !name || !file_path || // Require type, name, filepath
        owner_id <= 0 || creator_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_save_file: Invalid argument (NULL pointers or invalid IDs).\n");
        return KFS_INVALID_ARGUMENT;
    }
     if (topic_count < 0 || (topic_count > 0 && !topics)) {
         fprintf(stderr, "[ERROR] kfs_save_file: Invalid topic arguments (count=%d, topics_ptr=%p).\n", topic_count, (void*)topics);
        return KFS_INVALID_ARGUMENT;
    }
    *artifact_id = -1; // Reset output parameter

    FILE* fp = NULL;
    void* file_data = NULL;
    size_t file_size = 0;
    int rc = KFS_OK;
    int current_artifact_id = -1;

    // --- Read File Content ---
    fp = fopen(file_path, "rb"); // Open in binary read mode
    if (!fp) {
        fprintf(stderr, "[ERROR] kfs_save_file: Cannot open file '%s'.\n", file_path);
        // Consider using strerror(errno) for more specific file errors
        return KFS_CANTOPEN; // Or KFS_IOERR
    }

    fseek(fp, 0, SEEK_END);
    long file_size_long = ftell(fp);
    // Check for ftell error or excessive size upfront
    if (file_size_long < 0 || file_size_long > (long)SQLITE_MAX_LENGTH) {
         fprintf(stderr, "[ERROR] kfs_save_file: Invalid or too large file size (%ld) for '%s'.\n", file_size_long, file_path);
         fclose(fp);
         return KFS_TOOBIG;
    }
    file_size = (size_t)file_size_long;
    fseek(fp, 0, SEEK_SET); // Rewind

    if (file_size > 0) { // Only allocate/read if file is not empty
        file_data = KFS_MALLOC(file_size);
        if (!file_data) {
            fprintf(stderr, "[ERROR] kfs_save_file: Failed to allocate %zu bytes for file '%s'.\n", file_size, file_path);
            fclose(fp);
            return KFS_NOMEM;
        }
        size_t bytes_read = fread(file_data, 1, file_size, fp);
        if (bytes_read != file_size) {
            fprintf(stderr, "[ERROR] kfs_save_file: Failed to read full file content from '%s' (read %zu / %zu bytes).\n", file_path, bytes_read, file_size);
            kfs_mem_free(file_data); // Clean up allocated memory
            fclose(fp);
            return KFS_IOERR;
        }
    }
    fclose(fp); // Close file now that data is read (or if it was empty)
    fp = NULL; // Avoid double close in error paths


    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK) { kfs_mem_free(file_data); return KFS_ERROR; }
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK) {
        exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); kfs_mem_free(file_data); return KFS_ERROR;
    }

    // --- Call internal save function (inserts into Artifacts and Assets) ---
    // Pass the file_data buffer (which might be NULL if file_size was 0)
    rc = kfs_save_asset(db, type, name, format, creator_id, owner_id, security_scheme_id, file_data, file_size, NULL /* text_data */, metadata, &current_artifact_id);

    if (rc != KFS_OK) {
        goto save_file_rollback; // Error occurred during insert
    }
     if (current_artifact_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_save_file: Internal save succeeded but returned invalid artifact ID (%d).\n", current_artifact_id);
        rc = KFS_INTERNAL;
        goto save_file_rollback;
    }

    // --- Assign topics if provided ---
    for (int i = 0; i < topic_count; i++) {
        if (topics[i] && strlen(topics[i]) > 0) {
            rc = kfs_link_topic_to_artifact_by_name_internal(db, 0, current_artifact_id, topics[i]);
            if (rc != KFS_OK) {
                fprintf(stderr, "[ERROR] kfs_save_file: Failed during assignment of topic '%s' (Error: %d).\n", topics[i], rc);
                goto save_file_rollback; // Rollback transaction
            }
        } else {
             fprintf(stderr, "[WARN] kfs_save_file: Skipping empty or NULL topic name at index %d.\n", i);
        }
    }

    // --- Commit Transactions ---
    int commit_rc1 = exec_sql(db->artifacts_db, "COMMIT;", "artifacts");
    int commit_rc2 = exec_sql(db->arch_db, "COMMIT;", "architecture");

    if (commit_rc1 == KFS_OK && commit_rc2 == KFS_OK) {
        *artifact_id = current_artifact_id;
        rc = KFS_OK; // Set final return code to OK
    } else {
        fprintf(stderr, "[ERROR] kfs_save_file: Commit failed (artifacts_rc=%d, arch_rc=%d). Attempting rollback.\n", commit_rc1, commit_rc2);
        exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); // Ignore errors
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");   // Ignore errors
        rc = KFS_ERROR; // Set final return code to error
    }

    // --- Cleanup and Return ---
    kfs_mem_free(file_data); // Free the buffer allocated for file content
    return rc;


save_file_rollback:
    // An error occurred before commit, rollback both transactions
    fprintf(stderr, "[ERROR] kfs_save_file: Rolling back transaction due to error %d.\n", rc);
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); // Ignore errors
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");   // Ignore errors
    kfs_mem_free(file_data); // Free the buffer if allocated
    return rc; // Return the specific error code
}


/* ============================================================================== */
/* ==                     TOPIC MANAGEMENT w/ Permissions                    == */
/* ============================================================================== */

/**
 * @brief Lists all topics in a specified domain that the requesting actor has READ permission for.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain to query.
 * @param topic_ids Output array of topic IDs (caller must free).
 * @param topic_names Output array of topic names (caller must free each string).
 * @param topic_count Output number of topics.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_list_topics(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** topic_ids, char*** topic_names, int* topic_count) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || !topic_ids || !topic_names || !topic_count) {
        fprintf(stderr, "[ERROR] kfs_list_topics: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *topic_ids = NULL;
    *topic_names = NULL;
    *topic_count = 0;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    int* temp_ids = NULL;
    char** temp_names = NULL;
    int count = 0;
    int capacity = 16; // Initial capacity

    // --- Begin Transactions ---
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_topics: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Check Domain READ Access (Simpler Check - can user access domain at all?) ---
    // We can use kfs_check_permission with KFS_PERM_READ on the Domain itself.
    rc = kfs_check_permission(db, requesting_actor_uuid, "Domain", domain_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_topics: Domain access check failed for domain %d (rc=%d).\n", domain_id, rc);
        goto cleanup; // Permission denied, not found, or DB error
    }

    // --- Fetch All Topics in Domain ---
    const char* sql_topics = "SELECT id, name FROM Topics WHERE domain_id = ? ORDER BY name;"; // Order by name for consistency
    rc = sqlite3_prepare_v2(db->arch_db, sql_topics, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_list_topics (topics) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);

    // Allocate initial arrays
    temp_ids = KFS_MALLOC(capacity * sizeof(int));
    temp_names = KFS_MALLOC(capacity * sizeof(char*));
    if (!temp_ids || !temp_names) { rc = KFS_NOMEM; goto cleanup;}

    // --- Iterate and Check Permission for Each Topic ---
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int current_topic_id = sqlite3_column_int(stmt, 0);
        const unsigned char* current_topic_name_raw = sqlite3_column_text(stmt, 1);

        // Check READ permission on the specific topic
        int perm_rc = kfs_check_permission(db, requesting_actor_uuid, "Topic", current_topic_id, KFS_PERM_READ);

        if (perm_rc == KFS_OK) {
            // Permission granted, add to results
            if (count >= capacity) {
                capacity *= 2;
                int* new_ids = KFS_REALLOC(temp_ids, capacity * sizeof(int));
                char** new_names = KFS_REALLOC(temp_names, capacity * sizeof(char*));
                if (!new_ids || !new_names) {
                    kfs_mem_free(new_ids ? new_ids : temp_ids);
                    kfs_mem_free(new_names ? new_names : temp_names);
                    temp_ids = NULL;
                    temp_names = NULL;
                    rc = KFS_NOMEM;
                    break;
                } // Break loop on realloc failure
                temp_ids = new_ids;
                temp_names = new_names;
            }

            temp_ids[count] = current_topic_id;
            temp_names[count] = current_topic_name_raw ? KFS_STRDUP((const char*)current_topic_name_raw) : NULL;
            if (current_topic_name_raw && !temp_names[count]) { rc = KFS_NOMEM; break; } // Break loop on strdup failure
            count++;
        } else if (perm_rc == KFS_PERMISSION_DENIED || perm_rc == KFS_NOTFOUND) {
            // Skip this topic if permission denied or somehow not found after initial query
            fprintf(stderr, "[INFO] kfs_list_topics: Skipping topic %d due to permission check result %d.\n", current_topic_id, perm_rc);
        } else {
            // Propagate other errors from permission check
            fprintf(stderr, "[ERROR] kfs_list_topics: Error checking permission for topic %d (rc=%d).\n", current_topic_id, perm_rc);
            rc = perm_rc;
            break; // Exit loop on error
        }
    } // End while loop
    sqlite3_finalize(stmt); stmt = NULL;

    // Check final state of loop/permission checks
    if (rc != SQLITE_DONE && rc != KFS_OK && rc != KFS_NOMEM) { // If loop exited due to error other than DONE or NOMEM
        goto cleanup;
    }
     if (rc == KFS_NOMEM) { goto cleanup; } // Handle NOMEM from loop

    // --- Finalize Results ---
    if (count == 0) {
        kfs_mem_free(temp_ids); temp_ids = NULL;
        kfs_mem_free(temp_names); temp_names = NULL;
        fprintf(stderr, "[INFO] kfs_list_topics: No accessible topics found in domain %d for user %llu.\n", domain_id, (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND; // Signal no results found
        goto commit; // Still need to commit/rollback cleanly
    }

    *topic_ids = temp_ids;
    *topic_names = temp_names;
    *topic_count = count;
    rc = KFS_OK; // Set final status to OK

commit:
    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_topics: Commit failed.\n");
        // If commit fails, data might be inconsistent, but we might have already allocated results
        rc = KFS_ERROR; // Mark error
        goto cleanup; // Free allocated results and rollback (again, just in case)
    }

    if (rc == KFS_OK) { // Only log success if final rc is OK
         fprintf(stdout, "[INFO] kfs_list_topics: Successfully retrieved %d accessible topics in domain %d.\n", count, domain_id);
    }
    return rc; // Return KFS_OK or KFS_NOTFOUND

cleanup:
    // Free allocated memory if an error occurred before success
    sqlite3_finalize(stmt); // Ensure stmt is finalized
    if (temp_ids) kfs_mem_free(temp_ids);
    if (temp_names) {
        for (int i = 0; i < count; i++) kfs_mem_free(temp_names[i]); // Free individual strings
        kfs_mem_free(temp_names);
    }
    // Reset output params on error
     *topic_ids = NULL; *topic_names = NULL; *topic_count = 0;
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code
}

/**
 * @brief Adds a new topic to architecture.db.Topics in a specified domain.
 * Requires domain access and WRITE permission for the requesting user.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param owner_actor_id ID of the owning actor (user or group).
 * @param name Name of the topic.
 * @param security_scheme_id ID of the security scheme (must be in the same domain).
 * @param domain_id ID of the domain.
 * @param topic_id Output parameter for the created topic ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_add_topic(GameDB* db, uint64_t requesting_actor_uuid, int owner_actor_id, const char* name, int security_scheme_id, int domain_id, int* topic_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || owner_actor_id <= 0 || !name || domain_id <= 0 || !topic_id) {
        fprintf(stderr, "[ERROR] kfs_add_topic: Invalid arguments (requesting_actor_uuid=%llu, owner_actor_id=%d, name=%s, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, owner_actor_id, name ? name : "NULL", domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *topic_id = -1;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_add_topic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Get Requester Actor ID and Check Domain Access ---
    int requester_actor_id = -1;
    const char* sql_get_requester_id = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_get_requester_id, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_topic (get requester id) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)requesting_actor_uuid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        requester_actor_id = sqlite3_column_int(stmt, 0);
        int is_active = sqlite3_column_int(stmt, 1);
        if (!is_active) {
            fprintf(stderr, "[ERROR] kfs_add_topic: Requester UUID %llu is inactive.\n",
                    (unsigned long long)requesting_actor_uuid);
            rc = KFS_PERMISSION_DENIED;
        } else {
            rc = KFS_OK;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_topic: Requester UUID %llu not found.\n",
                (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_add_topic (get requester id) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != KFS_OK) {
        goto cleanup;
    }

    // Check domain access
    int has_domain_access = 0;
    const char* sql_check_domain = "SELECT 1 FROM DomainActors WHERE domain_id = ? AND actor_id = ? LIMIT 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_domain, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_topic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_int(stmt, 2, requester_actor_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_domain_access = 1;
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_topic (check domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (!has_domain_access) {
        const char* sql_check_group_domain = "SELECT DA.actor_id FROM DomainActors DA JOIN Actors A ON DA.actor_id = A.id "
                                            "WHERE DA.domain_id = ? AND A.actor_type IN ('GROUP', 'COMPANY');";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_group_domain, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_add_topic (check group domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, domain_id);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int group_id = sqlite3_column_int(stmt, 0);
            if (is_user_in_group(db, requester_actor_id, group_id)) {
                has_domain_access = 1;
                break;
            }
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_add_topic (check group domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!has_domain_access) {
            fprintf(stderr, "[ERROR] kfs_add_topic: Requester %llu lacks access to domain %d.\n",
                    (unsigned long long)requesting_actor_uuid, domain_id);
            rc = KFS_PERMISSION_DENIED;
            goto cleanup;
        }
        rc = KFS_OK;
    }

    // --- Verify Owner Exists ---
    const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_topic (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, owner_actor_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "[ERROR] kfs_add_topic: Owner actor ID %d not found.\n", owner_actor_id);
        rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Verify Security Scheme (if provided) ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_add_topic (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, security_scheme_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_add_topic: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
            rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    // --- Create Topic ---
    char* timestamp = get_current_timestamp();
    if (!timestamp) {
        fprintf(stderr, "[ERROR] kfs_add_topic: Memory allocation failed for timestamp.\n");
        goto cleanup;
    }

    const char* sql_insert = "INSERT INTO Topics (domain_id, name, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_topic (insert) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)requesting_actor_uuid);
    sqlite3_bind_int(stmt, 4, owner_actor_id);
    if (security_scheme_id >= 0) {
        sqlite3_bind_int(stmt, 5, security_scheme_id);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    sqlite3_bind_text(stmt, 6, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, timestamp, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *topic_id = (int)sqlite3_last_insert_rowid(db->arch_db);
    }
    sqlite3_finalize(stmt);
    kfs_mem_free(timestamp);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_topic (insert) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) {
            rc = KFS_CONSTRAINT;
        }
        goto cleanup;
    }

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_add_topic: Commit failed.\n");
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_add_topic: Successfully created topic '%s' with ID %d in domain %d.\n", name, *topic_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Deletes a topic from a specified domain.
 * Requires DELETE permission on the Topic itself and domain access.
 * Handles cascading deletes for related items (EpicAssignments, RelatedTopics, TopicAssignments).
 * Manually deletes associated EntityNotes links.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the topic.
 * @param topic_id ID of the topic to delete.
 * @return KFS_OK on success (even if topic didn't exist), KFS_INVALID_ARGUMENT,
 *         KFS_PERMISSION_DENIED, or SQLite error.
 */
int kfs_delete_topic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int topic_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || topic_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_delete_topic: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, topic_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, topic_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) { // Need registry for permission check
        fprintf(stderr, "[ERROR] kfs_delete_topic: Failed to begin transaction.\n");
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        exec_sql(db->registry_db, "ROLLBACK;", "registry");
        return KFS_ERROR;
    }

    // --- Permission Check: DELETE on the Topic ---
    // This also implicitly verifies the user has access to the domain containing the topic.
    rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_DELETE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_delete_topic: Topic ID %d not found or permission check failed with NOTFOUND, treating as success for delete.\n", topic_id);
            rc = KFS_OK; // Not found is OK for delete
            goto commit; // Skip actual deletion steps
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_delete_topic: Permission denied for user %llu to delete topic %d.\n",
                    (unsigned long long)requesting_user_uuid, topic_id);
        } else {
             fprintf(stderr, "[ERROR] kfs_delete_topic: Permission check failed with error %d.\n", rc);
        }
        goto cleanup; // Permission denied or DB error during check
    }

    // --- Verify Topic Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Topics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_delete_topic: Topic ID %d does not belong to domain %d (or was not found after permission check).\n", topic_id, domain_id);
            rc = KFS_NOTFOUND; // Or maybe KFS_INTERNAL if permission check passed but it's not in domain?
            goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_topic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    // --- Perform Deletions ---

    // 1. Manually delete associated notes links
    const char* sql_del_notes = "DELETE FROM EntityNotes WHERE entity_type = 'Topic' AND entity_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_del_notes, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        rc = sqlite3_step(stmt); // Use rc directly
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_delete_topic: Failed deleting note links (rc=%d).\n", rc);
            goto cleanup;
        }
        rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_topic (del notes) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    // 2. Delete the Topic itself (Cascades handle EpicAssignments, RelatedTopics, TopicAssignments if set correctly in kfs_init)
    const char* sql_del_topic = "DELETE FROM Topics WHERE id = ? AND domain_id = ?;"; // Add domain_id for safety
    rc = sqlite3_prepare_v2(db->arch_db, sql_del_topic, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt); // Use rc directly
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_delete_topic: Failed deleting topic record (rc=%d).\n", rc);
            goto cleanup;
        }
        if (sqlite3_changes(db->arch_db) == 0) {
             fprintf(stderr, "[WARN] kfs_delete_topic: Topic ID %d not found during delete (though permission check passed).\n", topic_id);
             // Continue to commit as cleanup might have occurred.
        }
        rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_topic (del topic) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


commit:
    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_topic: Commit failed.\n");
        rc = KFS_ERROR; // Mark error
        goto cleanup; // Attempt rollback
    }

    fprintf(stdout, "[INFO] kfs_delete_topic: Successfully processed delete request for topic %d in domain %d by user %llu.\n",
            topic_id, domain_id, (unsigned long long)requesting_user_uuid);
    return KFS_OK; // Return OK even if commit failed? No, return the error code.

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    // Rollback must be attempted if commit failed or error occurred before commit
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}

/**
 * @brief Retrieves a topic from a specified domain.
 * Requires READ permission on the topic and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the topic.
 * @param topic_id ID of the topic to retrieve.
 * @param owner_actor_id Output parameter for the owner actor ID.
 * @param name Output parameter for the topic name (caller must free).
 * @param security_scheme_id Output parameter for the security scheme ID (-1 if none).
 * @param creator_uuid Output parameter for the creator UUID.
 * @param created_at Output parameter for the creation timestamp (caller must free).
 * @param updated_at Output parameter for the update timestamp (caller must free).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_get_topic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int topic_id,
                  int* owner_actor_id, char** name, int* security_scheme_id,
                  uint64_t* creator_uuid, char** created_at, char** updated_at) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || topic_id <= 0 ||
        !owner_actor_id || !name || !security_scheme_id || !creator_uuid || !created_at || !updated_at) {
        fprintf(stderr, "[ERROR] kfs_get_topic: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, topic_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, topic_id);
        return KFS_INVALID_ARGUMENT;
    }
    // Initialize outputs
    *owner_actor_id = -1; *name = NULL; *security_scheme_id = -1;
    *creator_uuid = 0; *created_at = NULL; *updated_at = NULL;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_topic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: READ on the Topic ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Topic", topic_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_topic: Permission check failed for topic %d (rc=%d).\n", topic_id, rc);
        goto cleanup;
    }

    // --- Verify Topic Belongs to Domain (Safety Check) ---
     const char* sql_check_domain = "SELECT 1 FROM Topics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_get_topic: Topic ID %d does not belong to domain %d.\n", topic_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_get_topic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Fetch Topic Details ---
    const char* sql_topic = "SELECT name, owner_actor_id, security_scheme_id, creator_uuid, created_at, updated_at "
                            "FROM Topics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_topic, -1, &stmt, NULL);
     if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_get_topic (fetch) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, topic_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* topic_name = sqlite3_column_text(stmt, 0);
        *owner_actor_id = sqlite3_column_int(stmt, 1);
        *security_scheme_id = sqlite3_column_int(stmt, 2); // Get scheme ID
        *creator_uuid = (uint64_t)sqlite3_column_int64(stmt, 3);
        const unsigned char* topic_created_at = sqlite3_column_text(stmt, 4);
        const unsigned char* topic_updated_at = sqlite3_column_text(stmt, 5);

        if (sqlite3_column_type(stmt, 2) == SQLITE_NULL) {
            *security_scheme_id = -1; // Explicitly set -1 if scheme is NULL
        }

        // Allocate memory for output strings
        *name = topic_name ? KFS_STRDUP((const char*)topic_name) : NULL;
        *created_at = topic_created_at ? KFS_STRDUP((const char*)topic_created_at) : NULL;
        *updated_at = topic_updated_at ? KFS_STRDUP((const char*)topic_updated_at) : NULL;

        // Check for allocation failures
        if ((topic_name && !*name) || (topic_created_at && !*created_at) || (topic_updated_at && !*updated_at)) {
            rc = KFS_NOMEM;
        } else {
            rc = KFS_OK; // Reset rc
        }
    } else { // Should not happen due to permission check
        rc = KFS_NOTFOUND;
    }
    sqlite3_finalize(stmt); stmt = NULL;
    if (rc != KFS_OK) { goto cleanup; }


    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_topic: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Free allocated memory
    }

    fprintf(stdout, "[INFO] kfs_get_topic: Successfully retrieved topic %d in domain %d.\n", topic_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Free potentially allocated memory on error
    kfs_mem_free(*name); *name = NULL;
    kfs_mem_free(*created_at); *created_at = NULL;
    kfs_mem_free(*updated_at); *updated_at = NULL;
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Creates a relationship link between two topics.
 * Checks for WRITE permission on the source topic (topic_id).
 */
int kfs_link_related_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int related_topic_id, int is_subtopic) {
    if (!db || !db->arch_db || requesting_user_uuid == 0 || topic_id <= 0 || related_topic_id <= 0 || topic_id == related_topic_id) {
        return KFS_INVALID_ARGUMENT;
    }

    // --- Permission Check: WRITE on the source Topic ---
    int perm_rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_WRITE);
    if (perm_rc != KFS_OK) return perm_rc;

    // Optional: Check READ permission on related_topic_id? Depends on desired strictness.

    // --- Proceed with linking ---
    const char* sql = "INSERT OR IGNORE INTO RelatedTopics (topic_id, related_topic_id, is_subtopic) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc != KFS_OK) { fprintf(stderr, "[ERROR] kfs_link_related_topic - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); sqlite3_finalize(stmt); return rc; }
    sqlite3_bind_int(stmt, 1, topic_id);
    sqlite3_bind_int(stmt, 2, related_topic_id);
    sqlite3_bind_int(stmt, 3, (is_subtopic ? 1 : 0));
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { fprintf(stderr, "[ERROR] kfs_link_related_topic - Execute failed: %s\n", sqlite3_errmsg(db->arch_db)); return rc; }
    return KFS_OK;
}

/**
 * @brief Creates a relationship link between two topics using their names within a specific domain.
 * Checks for WRITE permission on the source topic (topic_name).
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id The ID of the domain where both topics reside.
 * @param topic_name The name of the source topic.
 * @param related_topic_name The name of the topic to link to.
 * @param is_subtopic Flag indicating if related_topic_name is a subtopic of topic_name.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND, or SQLite error.
 */
int kfs_link_related_topic_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, const char* related_topic_name, int is_subtopic) {
    // Input Validation
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !topic_name || !related_topic_name) {
         fprintf(stderr, "[ERROR] kfs_link_related_topic_by_name: Invalid arguments.\n");
        return KFS_INVALID_ARGUMENT;
    }
     if (strcmp(topic_name, related_topic_name) == 0) {
         fprintf(stderr, "[ERROR] kfs_link_related_topic_by_name: Cannot link a topic to itself ('%s').\n", topic_name);
         return KFS_INVALID_ARGUMENT;
     }

    int topic_id = -1, related_topic_id = -1, rc = KFS_OK;

    // Find Topic IDs within the domain
    rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &topic_id);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_link_related_topic_by_name: Failed to find source topic '%s' in domain %d (rc=%d).\n", topic_name, domain_id, rc);
        return rc;
    }
    rc = kfs_get_topic_id_by_name(db, domain_id, related_topic_name, &related_topic_id);
    if (rc != KFS_OK) {
         fprintf(stderr, "[ERROR] kfs_link_related_topic_by_name: Failed to find related topic '%s' in domain %d (rc=%d).\n", related_topic_name, domain_id, rc);
        return rc;
    }

    // Call ID-based function which performs the permission check on the source topic_id
    return kfs_link_related_topic(db, requesting_user_uuid, topic_id, related_topic_id, is_subtopic);
}


/**
 * @brief Removes a relationship link between two topics.
 * Checks for WRITE permission on the source topic.
 */
int kfs_unlink_related_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int related_topic_id) {
    if (!db || !db->arch_db || requesting_user_uuid == 0 || topic_id <= 0 || related_topic_id <= 0) {
        return KFS_INVALID_ARGUMENT;
    }

    // --- Permission Check: WRITE on the source Topic ---
    int perm_rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_WRITE);
    if (perm_rc != KFS_OK) return perm_rc;

    // --- Proceed with unlinking ---
     const char* sql = "DELETE FROM RelatedTopics WHERE topic_id = ? AND related_topic_id = ?;";
     sqlite3_stmt* stmt = NULL;
     int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
     if (rc != KFS_OK) { fprintf(stderr, "[ERROR] kfs_unlink_related_topic - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); sqlite3_finalize(stmt); return rc; }
     sqlite3_bind_int(stmt, 1, topic_id);
     sqlite3_bind_int(stmt, 2, related_topic_id);
     rc = sqlite3_step(stmt);
     sqlite3_finalize(stmt);
     if (rc != SQLITE_DONE) { fprintf(stderr, "[ERROR] kfs_unlink_related_topic - Execute failed: %s\n", sqlite3_errmsg(db->arch_db)); return rc; }
     return KFS_OK;
}


/**
 * @brief Loads all subtopics for a given topic name within a specific domain,
 * checking READ permission on the parent and each subtopic.
 * Allocates memory for the results array and internal structs/strings.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id The ID of the domain where the parent topic resides.
 * @param topic_name The name of the parent topic.
 * @param results Output array of KFS_Topic structs (caller must free with kfs_topics_free).
 * @param result_count Output number of accessible subtopics found.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_load_subtopics(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, KFS_Topic** results, int* result_count) {
    // Input Validation
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !topic_name || !results || !result_count) {
        fprintf(stderr, "[ERROR] kfs_load_subtopics: Invalid arguments.\n");
        return KFS_INVALID_ARGUMENT;
    }
    *results = NULL; *result_count = 0;

    int parent_topic_id = -1;
    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    KFS_Topic* temp_results = NULL;
    int count = 0;
    int capacity = 16;

    // --- Begin Transactions ---
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_load_subtopics: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // Find Parent Topic ID within the domain
    rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &parent_topic_id);
    if (rc != KFS_OK) {
         fprintf(stderr, "[ERROR] kfs_load_subtopics: Parent topic '%s' not found in domain %d (rc=%d).\n", topic_name, domain_id, rc);
         goto cleanup; // KFS_NOTFOUND or DB error
    }

    // --- Permission Check: READ on the Parent Topic ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Topic", parent_topic_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_load_subtopics: Permission check failed for parent topic %d (rc=%d).\n", parent_topic_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED or error
    }

    // --- Query subtopic IDs ---
    const char* sql = "SELECT related_topic_id FROM RelatedTopics WHERE topic_id = ? AND is_subtopic = 1 ORDER BY related_topic_id;";
    rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_load_subtopics (query subtopics) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    sqlite3_bind_int(stmt, 1, parent_topic_id);

    // Allocate initial array
    temp_results = KFS_MALLOC(capacity * sizeof(KFS_Topic));
    if (!temp_results) { rc = KFS_NOMEM; goto cleanup;}
    memset(temp_results, 0, capacity * sizeof(KFS_Topic));

    // --- Iterate, Check Permissions, and Load Subtopics ---
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int sub_topic_id = sqlite3_column_int(stmt, 0);

        // Check READ permission on the sub-topic
        int perm_rc = kfs_check_permission(db, requesting_user_uuid, "Topic", sub_topic_id, KFS_PERM_READ);
        if (perm_rc == KFS_OK) {
            // Reallocate if needed
             if (count >= capacity) {
                capacity *= 2;
                KFS_Topic* new_results = KFS_REALLOC(temp_results, capacity * sizeof(KFS_Topic));
                if (!new_results) { rc = KFS_NOMEM; break; }
                temp_results = new_results;
                memset(temp_results + count, 0, (capacity / 2) * sizeof(KFS_Topic));
             }

            // Load the subtopic details into the array slot
            // kfs_get_topic handles its own domain check internally now
            int get_rc = kfs_get_topic(db, requesting_user_uuid, domain_id, sub_topic_id, // Pass domain_id for consistency
                                     &temp_results[count].owner_actor_id, &temp_results[count].name,
                                     &temp_results[count].security_scheme_id, &temp_results[count].creator_uuid,
                                     &temp_results[count].created_at, &temp_results[count].updated_at);

            if (get_rc == KFS_OK) {
                temp_results[count].id = sub_topic_id; // Set the ID
                temp_results[count].domain_id = domain_id; // Set the domain ID
                // Reset related fields as kfs_get_topic doesn't load them
                temp_results[count].epics = NULL; temp_results[count].epic_count = 0;
                temp_results[count].related_topics = NULL; temp_results[count].related_count = 0;
                temp_results[count].is_subtopic = NULL;
                temp_results[count].notes = NULL; temp_results[count].note_count = 0;
                count++;
            } else {
                 fprintf(stderr, "[WARN] kfs_load_subtopics: Failed to get subtopic details for ID %d after permission check (rc=%d), skipping.\n", sub_topic_id, get_rc);
                 if (get_rc == KFS_NOMEM) { rc = KFS_NOMEM; break; } // Propagate memory errors
                 if (get_rc != KFS_NOTFOUND) { rc = get_rc; break;} // Propagate other DB errors
                  // If NOTFOUND, just skip
            }
        } else if (perm_rc != KFS_PERMISSION_DENIED && perm_rc != KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_load_subtopics: Error checking permission for subtopic %d (rc=%d).\n", sub_topic_id, perm_rc);
            rc = perm_rc; break; // Exit loop on error
        }
         // Skip if permission denied or subtopic not found
    } // End while loop
    sqlite3_finalize(stmt); stmt = NULL;

    // Check final loop/permission checks status
    if (rc != SQLITE_DONE && rc != KFS_OK && rc != KFS_NOMEM) { goto cleanup; }
    if (rc == KFS_NOMEM) { goto cleanup; }

    // --- Finalize Results ---
    if (count == 0) {
        kfs_mem_free(temp_results); temp_results = NULL;
        fprintf(stderr, "[INFO] kfs_load_subtopics: No accessible subtopics found for topic '%s' in domain %d.\n", topic_name, domain_id);
        rc = KFS_NOTFOUND;
        goto commit;
    }

     // Shrink array (optional)
     if (count < capacity) {
         KFS_Topic* final_results = KFS_REALLOC(temp_results, count * sizeof(KFS_Topic));
         if (final_results) temp_results = final_results;
     }

    *results = temp_results;
    *result_count = count;
    rc = KFS_OK;

commit:
    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_load_subtopics: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Free results if commit fails
    }

    if (rc == KFS_OK) {
        fprintf(stdout, "[INFO] kfs_load_subtopics: Successfully loaded %d accessible subtopics for '%s'.\n", count, topic_name);
    }
    return rc; // KFS_OK or KFS_NOTFOUND

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Free potentially allocated array and its contents
    if (temp_results) {
        // Need kfs_topics_free which calls kfs_topic_free_contents
        kfs_topics_free(temp_results, count);
    }
    *results = NULL; *result_count = 0; // Reset outputs
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the error code
}

/**
 * @brief Updates metadata for a topic in a specified domain.
 * Requires WRITE permission on the topic and domain access.
 * Validates new owner and scheme (must be in the same domain).
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the topic.
 * @param topic_id ID of the topic to update.
 * @param name New name (optional, NULL to keep unchanged).
 * @param owner_actor_id New owner actor ID (optional, <= 0 to keep unchanged).
 * @param security_scheme_id New security scheme ID (optional, < 0 to remove/keep NULL, >= 0 to set/update).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_update_topic(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int topic_id, const char* name, int owner_actor_id, int security_scheme_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || topic_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_update_topic: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, topic_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, topic_id);
        return KFS_INVALID_ARGUMENT;
    }
    // Ensure at least one field is being updated if calling function provides options
    if (!name && owner_actor_id <= 0 && security_scheme_id < -1) { // Allow -1 for explicit NULL setting
         fprintf(stderr, "[INFO] kfs_update_topic: No update parameters provided for topic %d.\n", topic_id);
         return KFS_OK; // Nothing to do
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    char* timestamp = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_topic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Topic ---
    // Also verifies user has access to the domain containing the topic
    rc = kfs_check_permission(db, requesting_actor_uuid, "Topic", topic_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_topic: Permission check failed for topic %d (rc=%d).\n", topic_id, rc);
        goto cleanup;
    }

    // --- Verify Topic Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Topics WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_update_topic: Topic ID %d does not belong to domain %d.\n", topic_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_update_topic (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Verify New Owner (if provided) ---
    if (owner_actor_id > 0) {
        const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ? AND is_active = 1;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
         if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, owner_actor_id);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_update_topic: New owner actor ID %d not found or inactive.\n", owner_actor_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
            rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_update_topic (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Verify New Security Scheme (if setting/updating) ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
         if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, security_scheme_id);
            sqlite3_bind_int(stmt, 2, domain_id); // Ensure scheme is in the SAME domain
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_update_topic: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
             rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_update_topic (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Update Topic ---
    timestamp = get_current_timestamp();
    if (!timestamp) { rc = KFS_NOMEM; goto cleanup; }

    // Build SET clause dynamically or use multiple UPDATEs for simplicity if needed
    char set_clause[512] = "";
    int param_index = 1;

    if (name) { strcat(set_clause, "name = ?, "); }
    if (owner_actor_id > 0) { strcat(set_clause, "owner_actor_id = ?, "); }
    if (security_scheme_id >= -1) { strcat(set_clause, "security_scheme_id = ?, "); } // Allow setting to NULL via -1
    strcat(set_clause, "updated_at = ?"); // Always update timestamp

    char sql_update[600];
    snprintf(sql_update, sizeof(sql_update), "UPDATE Topics SET %s WHERE id = ? AND domain_id = ?;", set_clause);

    rc = sqlite3_prepare_v2(db->arch_db, sql_update, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_update_topic (update) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    param_index = 1;
    if (name) sqlite3_bind_text(stmt, param_index++, name, -1, SQLITE_STATIC);
    if (owner_actor_id > 0) sqlite3_bind_int(stmt, param_index++, owner_actor_id);
    if (security_scheme_id >= -1) {
        if (security_scheme_id == -1) sqlite3_bind_null(stmt, param_index++);
        else sqlite3_bind_int(stmt, param_index++, security_scheme_id);
    }
    sqlite3_bind_text(stmt, param_index++, timestamp, -1, SQLITE_STATIC); // updated_at
    sqlite3_bind_int(stmt, param_index++, topic_id); // WHERE id = ?
    sqlite3_bind_int(stmt, param_index++, domain_id); // WHERE domain_id = ?

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_topic (update) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT_UNIQUE) rc = KFS_CONSTRAINT; // Name conflict
        else rc = KFS_ERROR;
        goto cleanup;
    }

    if (sqlite3_changes(db->arch_db) == 0) {
        // Should not happen due to earlier checks
        fprintf(stderr, "[ERROR] kfs_update_topic: Topic ID %d not found during update.\n", topic_id);
        rc = KFS_NOTFOUND;
        goto cleanup;
    }
    rc = KFS_OK; // Reset rc

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_topic: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup;
    }

    kfs_mem_free(timestamp); // Free timestamp only on success or commit failure
    fprintf(stdout, "[INFO] kfs_update_topic: Successfully updated topic %d in domain %d.\n", topic_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    kfs_mem_free(timestamp); // Free timestamp if allocated
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/* ============================================================================== */
/* ==                       NOTE MANAGEMENT FUNCTIONS                        == */
/* ============================================================================== */

/**
 * @brief Adds a new note to architecture.db.Notes in a specified domain.
 * Requires domain access and WRITE permission for the requesting user.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param owner_actor_id ID of the owning actor (user or group).
 * @param content Content of the note.
 * @param security_scheme_id ID of the security scheme (must be in the same domain).
 * @param domain_id ID of the domain.
 * @param note_id Output parameter for the created note ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_add_note(GameDB* db, uint64_t requesting_actor_uuid, int owner_actor_id, const char* content, int security_scheme_id, int domain_id, int* note_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || owner_actor_id <= 0 || !content || domain_id <= 0 || !note_id) {
        fprintf(stderr, "[ERROR] kfs_add_note: Invalid arguments (requesting_actor_uuid=%llu, owner_actor_id=%d, content=%s, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, owner_actor_id, content ? content : "NULL", domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *note_id = -1;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_add_note: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Get Requester Actor ID and Check Domain Access ---
    int requester_actor_id = -1;
    const char* sql_get_requester_id = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_get_requester_id, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_note (get requester id) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)requesting_actor_uuid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        requester_actor_id = sqlite3_column_int(stmt, 0);
        int is_active = sqlite3_column_int(stmt, 1);
        if (!is_active) {
            fprintf(stderr, "[ERROR] kfs_add_note: Requester UUID %llu is inactive.\n",
                    (unsigned long long)requesting_actor_uuid);
            rc = KFS_PERMISSION_DENIED;
        } else {
            rc = KFS_OK;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_note: Requester UUID %llu not found.\n",
                (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_add_note (get requester id) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != KFS_OK) {
        goto cleanup;
    }

    // Check domain access
    int has_domain_access = 0;
    const char* sql_check_domain = "SELECT 1 FROM DomainActors WHERE domain_id = ? AND actor_id = ? LIMIT 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_domain, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_note (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_int(stmt, 2, requester_actor_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_domain_access = 1;
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_note (check domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (!has_domain_access) {
        const char* sql_check_group_domain = "SELECT DA.actor_id FROM DomainActors DA JOIN Actors A ON DA.actor_id = A.id "
                                            "WHERE DA.domain_id = ? AND A.actor_type IN ('GROUP', 'COMPANY');";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_group_domain, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_add_note (check group domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, domain_id);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int group_id = sqlite3_column_int(stmt, 0);
            if (is_user_in_group(db, requester_actor_id, group_id)) {
                has_domain_access = 1;
                break;
            }
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_add_note (check group domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!has_domain_access) {
            fprintf(stderr, "[ERROR] kfs_add_note: Requester %llu lacks access to domain %d.\n",
                    (unsigned long long)requesting_actor_uuid, domain_id);
            rc = KFS_PERMISSION_DENIED;
            goto cleanup;
        }
        rc = KFS_OK;
    }

    // --- Verify Owner Exists ---
    const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_note (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, owner_actor_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "[ERROR] kfs_add_note: Owner actor ID %d not found.\n", owner_actor_id);
        rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Verify Security Scheme (if provided) ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_add_note (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, security_scheme_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_add_note: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
            rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    // --- Create Note ---
    char* timestamp = get_current_timestamp();
    if (!timestamp) {
        fprintf(stderr, "[ERROR] kfs_add_note: Memory allocation failed for timestamp.\n");
        goto cleanup;
    }

    const char* sql_insert = "INSERT INTO Notes (domain_id, content, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_add_note (insert) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)requesting_actor_uuid);
    sqlite3_bind_int(stmt, 4, owner_actor_id);
    if (security_scheme_id >= 0) {
        sqlite3_bind_int(stmt, 5, security_scheme_id);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    sqlite3_bind_text(stmt, 6, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, timestamp, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *note_id = (int)sqlite3_last_insert_rowid(db->arch_db);
    }
    sqlite3_finalize(stmt);
    kfs_mem_free(timestamp);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_add_note (insert) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) {
            rc = KFS_CONSTRAINT;
        }
        goto cleanup;
    }

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_add_note: Commit failed.\n");
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_add_note: Successfully created note with ID %d in domain %d.\n", *note_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Lists all notes in a specified domain that the requesting actor has READ permission for.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain to query.
 * @param note_ids Output array of note IDs (caller must free).
 * @param note_contents Output array of note contents (caller must free each string).
 * @param note_count Output number of notes.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_list_notes(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** note_ids, char*** note_contents, int* note_count) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || !note_ids || !note_contents || !note_count) {
        fprintf(stderr, "[ERROR] kfs_list_notes: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *note_ids = NULL; *note_contents = NULL; *note_count = 0;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    int* temp_ids = NULL;
    char** temp_contents = NULL;
    int count = 0;
    int capacity = 16;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_notes: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Check Domain READ Access ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Domain", domain_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_notes: Domain access check failed for domain %d (rc=%d).\n", domain_id, rc);
        goto cleanup;
    }

    // --- Fetch All Notes in Domain ---
    const char* sql_notes = "SELECT id, content FROM Notes WHERE domain_id = ? ORDER BY id;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_notes, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_list_notes (fetch) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, domain_id);

    // Allocate initial arrays
    temp_ids = KFS_MALLOC(capacity * sizeof(int));
    temp_contents = KFS_MALLOC(capacity * sizeof(char*));
    if (!temp_ids || !temp_contents) { rc = KFS_NOMEM; goto cleanup;}
    memset(temp_ids, 0, capacity * sizeof(int));
    memset(temp_contents, 0, capacity * sizeof(char*));


    // --- Iterate and Check Permission for Each Note ---
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int current_note_id = sqlite3_column_int(stmt, 0);
        const unsigned char* current_note_content_raw = sqlite3_column_text(stmt, 1);

        // Check READ permission on the specific note
        int perm_rc = kfs_check_permission(db, requesting_actor_uuid, "Note", current_note_id, KFS_PERM_READ);

        if (perm_rc == KFS_OK) {
            // Permission granted, add to results
            if (count >= capacity) {
                capacity *= 2;
                int* new_ids = KFS_REALLOC(temp_ids, capacity * sizeof(int));
                char** new_contents = KFS_REALLOC(temp_contents, capacity * sizeof(char*));
                if (!new_ids || !new_contents) {
                    kfs_mem_free(new_ids ? new_ids : temp_ids);
                    kfs_mem_free(new_contents ? new_contents : temp_contents);
                    temp_ids = NULL;
                    temp_contents = NULL;
                    rc = KFS_NOMEM;
                    break;
                } // Break loop on realloc failure
                temp_ids = new_ids;
                temp_contents = new_contents;
                 // Zero out newly allocated part
                memset(temp_ids + count, 0, (capacity / 2) * sizeof(int));
                memset(temp_contents + count, 0, (capacity / 2) * sizeof(char*));
            }

            temp_ids[count] = current_note_id;
            temp_contents[count] = current_note_content_raw ? KFS_STRDUP((const char*)current_note_content_raw) : NULL;
            if (current_note_content_raw && !temp_contents[count]) { rc = KFS_NOMEM; break; } // Break loop on strdup failure
            count++;
        } else if (perm_rc == KFS_PERMISSION_DENIED || perm_rc == KFS_NOTFOUND) {
            // Skip this note if permission denied or somehow not found
             fprintf(stderr, "[INFO] kfs_list_notes: Skipping note %d due to permission check result %d.\n", current_note_id, perm_rc);
        } else {
            // Propagate other errors
            fprintf(stderr, "[ERROR] kfs_list_notes: Error checking permission for note %d (rc=%d).\n", current_note_id, perm_rc);
            rc = perm_rc; break; // Exit loop
        }
    } // End while loop
    sqlite3_finalize(stmt); stmt = NULL;

    // Check final state of loop/permission checks
    if (rc != SQLITE_DONE && rc != KFS_OK && rc != KFS_NOMEM) { goto cleanup; }
    if (rc == KFS_NOMEM) { goto cleanup; } // Handle NOMEM from loop

    // --- Finalize Results ---
    if (count == 0) {
        kfs_mem_free(temp_ids); temp_ids = NULL;
        kfs_mem_free(temp_contents); temp_contents = NULL;
        fprintf(stderr, "[INFO] kfs_list_notes: No accessible notes found in domain %d for user %llu.\n", domain_id, (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND; // Signal no results found
        goto commit; // Still need to commit/rollback cleanly
    }

    *note_ids = temp_ids;
    *note_contents = temp_contents;
    *note_count = count;
    rc = KFS_OK; // Set final status to OK

commit:
    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_list_notes: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Free allocated results and rollback
    }

    if (rc == KFS_OK) { // Only log success if final rc is OK
         fprintf(stdout, "[INFO] kfs_list_notes: Successfully retrieved %d accessible notes in domain %d.\n", count, domain_id);
    }
    return rc; // KFS_OK or KFS_NOTFOUND

cleanup:
    // Free allocated memory if an error occurred before success
    sqlite3_finalize(stmt); // Ensure stmt finalized
    if (temp_ids) kfs_mem_free(temp_ids);
    if (temp_contents) {
        for (int i = 0; i < count; i++) kfs_mem_free(temp_contents[i]); // Free individual strings
        kfs_mem_free(temp_contents);
    }
    // Reset output params on error
     *note_ids = NULL; *note_contents = NULL; *note_count = 0;
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code
}

// --- Generic list loading helper (internal) ---
// T = KFS_Topic or KFS_Epic
// FT = kfs_get_topic or kfs_get_epic
// FTT = kfs_topics_free or kfs_epics_free
typedef int (*kfs_get_entity_func)(GameDB*, int, void*);
typedef void (*kfs_free_entities_func)(void*, int);

static int kfs_list_entities(GameDB* db, const char* table_name, void** results, int* result_count, size_t struct_size, kfs_get_entity_func get_func) {
    if (!db || !db->arch_db || !results || !result_count) return KFS_INVALID_ARGUMENT;
    *results = NULL;
    *result_count = 0;

    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT id FROM %s ORDER BY name;", table_name); // Assumes table_name is safe

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc != KFS_OK) { /* Handle prepare error */ return rc; }

    void* temp_results = NULL;
    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int entity_id = sqlite3_column_int(stmt, 0);

        // Allocate space for the struct within the results array (reallocating each time)
        void* new_results = KFS_REALLOC(temp_results, (count + 1) * struct_size);
        if (!new_results) { rc = KFS_NOMEM; break; }
        temp_results = new_results;

        // Get pointer to the newly allocated struct space
        void* current_entity_ptr = (char*)temp_results + (count * struct_size);

        // Call the specific get function to fill the struct
        int get_rc = get_func(db, entity_id, current_entity_ptr);
        if (get_rc == KFS_OK) {
            count++; // Only increment count if get succeeded
        } else {
            fprintf(stderr, "[WARN] kfs_list_entities: Failed to get %s ID %d (rc=%d), skipping.\n", table_name, entity_id, get_rc);
             if (get_rc == KFS_NOMEM) { rc = KFS_NOMEM; break; } // Propagate NOMEM
             // Don't propagate NOTFOUND, just skip
             if (get_rc != KFS_NOTFOUND) { rc = get_rc; break; } // Propagate other DB errors
             // If skipped, the realloc'd space is unused, which is slightly wasteful but avoids complex shrinking logic
        }
    }
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != KFS_NOMEM) { // Error during step or get_func
        // Need to free potentially partially populated array
        // Requires the corresponding free function, making this helper tricky.
        // Let's simplify for now and assume caller handles cleanup on error.
        // Or implement the specific free logic here based on type.
        fprintf(stderr, "[ERROR] kfs_list_entities: Error occurred (rc=%d).\n", rc);
        kfs_mem_free(temp_results); // Simple free, might leak contents if get_func failed mid-alloc
        return rc;
    }
     if (rc == KFS_NOMEM) {
        fprintf(stderr, "[ERROR] kfs_list_entities: Memory allocation failed.\n");
        kfs_mem_free(temp_results);
        return KFS_NOMEM;
     }

    *results = temp_results;
    *result_count = count;
    return KFS_OK;
}

/*
int kfs_load_note(GameDB* db, int note_id, KFS_Note* note) {
    const char* sql = "SELECT id, content, created_at, updated_at FROM Notes WHERE id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc != KFS_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db->arch_db));
        return rc;
    }
    sqlite3_bind_int(stmt, 1, note_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        note->id = sqlite3_column_int(stmt, 0);
        note->content = KFS_STRDUP((const char*)sqlite3_column_text(stmt, 1));
        note->created_at = KFS_STRDUP((const char*)sqlite3_column_text(stmt, 2));
        note->updated_at = KFS_STRDUP((const char*)sqlite3_column_text(stmt, 3));
    } else {
        sqlite3_finalize(stmt);
        return KFS_NOTFOUND;
    }
    sqlite3_finalize(stmt);
    return KFS_OK;
}
*/

/* ============================================================================== */
/* ==                     NOTE MANAGEMENT w/ Permissions                     == */
/* ============================================================================== */

// kfs_add_note: Signature already updated for creator_uuid, owner_actor_id. No permission check needed for creation itself (implicitly allowed if user can call function).

/**
 * @brief Assigns an existing note to an entity (Artifact, Topic, or Epic).
 * Requires WRITE permission on the target entity.
 * Verifies that both the note and the target entity exist and belong to the same domain.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param entity_type Type of the target entity ("Artifact", "Topic", or "Epic").
 * @param entity_id ID of the target entity.
 * @param note_id ID of the note to assign.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_MISMATCH (if domains differ), or SQLite error.
 */
int kfs_assign_note(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int note_id) {
    // --- Input Validation ---
     if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || !entity_type ||
        !(strcmp(entity_type, "Artifact") == 0 || strcmp(entity_type, "Topic") == 0 || strcmp(entity_type, "Epic") == 0) ||
        entity_id <= 0 || note_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_assign_note: Invalid arguments (entity_type=%s, entity_id=%d, note_id=%d).\n",
                 entity_type ? entity_type : "NULL", entity_id, note_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    int entity_domain_id = -1;
    int note_domain_id = -1;

    // --- Begin Transactions ---
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_assign_note: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the target entity ---
    // This also verifies the entity exists and the user has access to its domain.
    rc = kfs_check_permission(db, requesting_user_uuid, entity_type, entity_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_assign_note: Permission check failed for target entity %s %d (rc=%d).\n", entity_type, entity_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED, KFS_NOTFOUND, or DB error
    }

    // --- Verify Note Exists and Get its Domain ---
    const char* sql_get_note_domain = "SELECT domain_id FROM Notes WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_get_note_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, note_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            note_domain_id = sqlite3_column_int(stmt, 0);
            rc = KFS_OK; // Reset rc
        } else {
            fprintf(stderr, "[ERROR] kfs_assign_note: Note ID %d not found.\n", note_id);
            rc = KFS_NOTFOUND;
        }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_assign_note (get note domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    if (rc != KFS_OK) goto cleanup;


    // --- Get Target Entity Domain (redundant after permission check, but safer) ---
    char sql_get_entity_domain[128];
    snprintf(sql_get_entity_domain, sizeof(sql_get_entity_domain), "SELECT domain_id FROM %ss WHERE id = ?;", entity_type); // Assumes plural 's'
    rc = sqlite3_prepare_v2(db->arch_db, sql_get_entity_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, entity_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            entity_domain_id = sqlite3_column_int(stmt, 0);
            rc = KFS_OK; // Reset rc
        } else {
            // Should have been caught by permission check
            fprintf(stderr, "[ERROR] kfs_assign_note: Target entity %s %d not found (after permission check!).\n", entity_type, entity_id);
            rc = KFS_NOTFOUND;
        }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_assign_note (get entity domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    if (rc != KFS_OK) goto cleanup;


    // --- Check Domain Match ---
    if (entity_domain_id != note_domain_id) {
        fprintf(stderr, "[ERROR] kfs_assign_note: Domain mismatch - Entity %s %d (domain %d) and Note %d (domain %d).\n",
                entity_type, entity_id, entity_domain_id, note_id, note_domain_id);
        rc = KFS_MISMATCH; // Use MISMATCH for domain error
        goto cleanup;
    }

    // --- Proceed with Assignment ---
    const char* sql_insert = "INSERT OR IGNORE INTO EntityNotes (entity_type, entity_id, note_id) VALUES (?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_assign_note (insert) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_text(stmt, 1, entity_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, entity_id);
    sqlite3_bind_int(stmt, 3, note_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_assign_note (insert) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT; else rc = KFS_ERROR;
        goto cleanup;
    }
    if (sqlite3_changes(db->arch_db) == 0) {
         fprintf(stdout, "[INFO] kfs_assign_note: Link between %s %d and note %d already exists.\n", entity_type, entity_id, note_id);
    }
    rc = KFS_OK; // Reset rc

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_assign_note: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_assign_note: Successfully assigned note %d to %s %d.\n", note_id, entity_type, entity_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Unassigns a specific note from an entity (Artifact, Topic, or Epic).
 * Requires WRITE permission on the target entity from which the note is being removed.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param entity_type Type of the target entity ("Artifact", "Topic", or "Epic").
 * @param entity_id ID of the target entity.
 * @param note_id ID of the note to unassign.
 * @return KFS_OK on success (even if the link didn't exist), KFS_INVALID_ARGUMENT,
 *         KFS_PERMISSION_DENIED, KFS_NOTFOUND (if permission check fails), or SQLite error.
 */
int kfs_unassign_note(GameDB* db, uint64_t requesting_user_uuid, const char* entity_type, int entity_id, int note_id) {
     // --- Input Validation ---
     if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || !entity_type ||
        !(strcmp(entity_type, "Artifact") == 0 || strcmp(entity_type, "Topic") == 0 || strcmp(entity_type, "Epic") == 0) ||
        entity_id <= 0 || note_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_unassign_note: Invalid arguments (entity_type=%s, entity_id=%d, note_id=%d).\n",
                 entity_type ? entity_type : "NULL", entity_id, note_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
     // Need registry for permission checks
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_unassign_note: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the target entity ---
    // This verifies the entity exists and the user has rights to modify its links.
    rc = kfs_check_permission(db, requesting_user_uuid, entity_type, entity_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
         // If entity not found during permission check, the link cannot exist, treat as OK for unassign.
         if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_unassign_note: Target entity %s %d not found or permission check failed with NOTFOUND, treating as success for unassign.\n", entity_type, entity_id);
            rc = KFS_OK;
            goto commit; // Skip actual deletion
         }
        fprintf(stderr, "[ERROR] kfs_unassign_note: Permission check failed for target entity %s %d (rc=%d).\n", entity_type, entity_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED or DB error
    }

    // --- Proceed with Deletion from EntityNotes ---
    const char* sql_delete = "DELETE FROM EntityNotes WHERE entity_type = ? AND entity_id = ? AND note_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_delete, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, entity_type, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, entity_id);
        sqlite3_bind_int(stmt, 3, note_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
             fprintf(stderr, "[ERROR] kfs_unassign_note (delete) - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->arch_db), rc);
             goto cleanup;
        }
        if (sqlite3_changes(db->arch_db) == 0) {
            fprintf(stdout, "[INFO] kfs_unassign_note: Link between %s %d and note %d not found.\n", entity_type, entity_id, note_id);
        }
        rc = KFS_OK; // Reset rc, not finding the link is OK for unassign
    } else { fprintf(stderr, "[ERROR] kfs_unassign_note (delete) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


commit:
    // --- Commit Transactions ---
     if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_unassign_note: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

     if (rc == KFS_OK) { // Only log success if final rc is OK
        fprintf(stdout, "[INFO] kfs_unassign_note: Successfully processed unassign for note %d from %s %d by user %llu.\n",
               note_id, entity_type, entity_id, (unsigned long long)requesting_user_uuid);
     }
    return rc;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}

/**
 * @brief Updates a note's metadata in a specified domain.
 * Requires WRITE permission on the note and domain access.
 * Validates new owner and scheme (must be in the same domain).
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the note.
 * @param note_id ID of the note to update.
 * @param content New content (optional, NULL to keep unchanged).
 * @param owner_actor_id New owner actor ID (optional, <= 0 to keep unchanged).
 * @param security_scheme_id New security scheme ID (optional, < 0 to remove/keep NULL, >= 0 to set/update).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_update_note(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int note_id, const char* content, int owner_actor_id, int security_scheme_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || note_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_update_note: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, note_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, note_id);
        return KFS_INVALID_ARGUMENT;
    }
     // Ensure at least one field is being updated
    if (!content && owner_actor_id <= 0 && security_scheme_id < 0) {
        fprintf(stderr, "[INFO] kfs_update_note: No update parameters provided for note %d.\n", note_id);
        return KFS_OK; // Nothing to do
    }


    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    char* timestamp = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_note: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Note ---
    // Also verifies user has access to the domain containing the note
    rc = kfs_check_permission(db, requesting_user_uuid, "Note", note_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_note: Permission check failed for note %d (rc=%d).\n", note_id, rc);
        goto cleanup;
    }

    // --- Verify Note Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Notes WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, note_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_update_note: Note ID %d does not belong to domain %d.\n", note_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_update_note (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    // --- Verify New Owner (if provided) ---
    if (owner_actor_id > 0) {
        const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ? AND is_active = 1;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, owner_actor_id);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_update_note: New owner actor ID %d not found or inactive.\n", owner_actor_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
            rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_update_note (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Verify New Security Scheme (if provided and >= 0) ---
    if (security_scheme_id >= 0) { // Only check if we intend to set it (>=0 means set or update)
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, security_scheme_id);
            sqlite3_bind_int(stmt, 2, domain_id); // Crucial: Ensure scheme is in the SAME domain
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_update_note: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
             rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_update_note (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Update Note ---
    timestamp = get_current_timestamp();
    if (!timestamp) { rc = KFS_NOMEM; goto cleanup; }

    // Use COALESCE-like logic or build dynamic SQL. COALESCE is simpler here.
    // Note: Setting security_scheme_id requires handling NULL vs. a specific ID.
    // Using COALESCE directly might not work perfectly for setting to NULL.
    // Let's build the SET clause dynamically.

    char set_clause[512] = "";
    int param_index = 1;
    int needs_update = 0;

    if (content) { strcat(set_clause, "content = ?, "); needs_update = 1; }
    if (owner_actor_id > 0) { strcat(set_clause, "owner_actor_id = ?, "); needs_update = 1; }
    if (security_scheme_id >= -1) { // Allow -1 to mean "set to NULL" if needed, >=0 means set/update
         strcat(set_clause, "security_scheme_id = ?, ");
         needs_update = 1;
    }
    strcat(set_clause, "updated_at = ?"); // Always update timestamp

    char sql_update[600];
    snprintf(sql_update, sizeof(sql_update), "UPDATE Notes SET %s WHERE id = ? AND domain_id = ?;", set_clause);

    rc = sqlite3_prepare_v2(db->arch_db, sql_update, -1, &stmt, NULL);
     if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_update_note (update) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    param_index = 1;
    if (content) sqlite3_bind_text(stmt, param_index++, content, -1, SQLITE_STATIC);
    if (owner_actor_id > 0) sqlite3_bind_int(stmt, param_index++, owner_actor_id);
    if (security_scheme_id >= -1) { // Bind NULL if -1, otherwise bind the ID
        if (security_scheme_id == -1) sqlite3_bind_null(stmt, param_index++);
        else sqlite3_bind_int(stmt, param_index++, security_scheme_id);
    }
    sqlite3_bind_text(stmt, param_index++, timestamp, -1, SQLITE_STATIC); // updated_at
    sqlite3_bind_int(stmt, param_index++, note_id); // WHERE id = ?
    sqlite3_bind_int(stmt, param_index++, domain_id); // WHERE domain_id = ?

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_note (update) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT;
        goto cleanup;
    }

    if (sqlite3_changes(db->arch_db) == 0) {
        // This case should ideally not be reached due to earlier checks, but handle defensively.
        fprintf(stderr, "[ERROR] kfs_update_note: Note ID %d not found during update (though permission check passed).\n", note_id);
        rc = KFS_NOTFOUND;
        goto cleanup;
    }

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_note: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup;
    }

    kfs_mem_free(timestamp); // Free timestamp only on success or commit failure
    fprintf(stdout, "[INFO] kfs_update_note: Successfully updated note %d in domain %d.\n", note_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    kfs_mem_free(timestamp); // Free timestamp if allocated
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Retrieves a note from a specified domain.
 * Requires READ permission on the note and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the note.
 * @param note_id ID of the note to retrieve.
 * @param owner_actor_id Output parameter for the owner actor ID.
 * @param content Output parameter for the note content (caller must free).
 * @param security_scheme_id Output parameter for the security scheme ID (-1 if none).
 * @param creator_uuid Output parameter for the creator UUID.
 * @param created_at Output parameter for the creation timestamp (caller must free).
 * @param updated_at Output parameter for the update timestamp (caller must free).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_get_note(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int note_id,
                 int* owner_actor_id, char** content, int* security_scheme_id,
                 uint64_t* creator_uuid, char** created_at, char** updated_at) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || note_id <= 0 ||
        !owner_actor_id || !content || !security_scheme_id || !creator_uuid || !created_at || !updated_at) {
        fprintf(stderr, "[ERROR] kfs_get_note: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, note_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, note_id);
        return KFS_INVALID_ARGUMENT;
    }
    // Initialize outputs
    *owner_actor_id = -1; *content = NULL; *security_scheme_id = -1;
    *creator_uuid = 0; *created_at = NULL; *updated_at = NULL;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions (Read-only, but good for consistency) ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_note: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: READ on the Note ---
    // This implicitly checks domain access and note existence.
    rc = kfs_check_permission(db, requesting_actor_uuid, "Note", note_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_note: Permission check failed for note %d (rc=%d).\n", note_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED, KFS_NOTFOUND, or DB error
    }

    // --- Verify Note Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Notes WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, note_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_get_note: Note ID %d does not belong to domain %d.\n", note_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup; // Should be caught by perm check, but be safe
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_get_note (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Fetch Note Details ---
    const char* sql_note = "SELECT content, owner_actor_id, security_scheme_id, creator_uuid, created_at, updated_at "
                           "FROM Notes WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_note, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_get_note (fetch) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, note_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* note_content = sqlite3_column_text(stmt, 0);
        *owner_actor_id = sqlite3_column_int(stmt, 1);
        *security_scheme_id = sqlite3_column_int(stmt, 2); // Get scheme ID
        *creator_uuid = (uint64_t)sqlite3_column_int64(stmt, 3);
        const unsigned char* note_created_at = sqlite3_column_text(stmt, 4);
        const unsigned char* note_updated_at = sqlite3_column_text(stmt, 5);

        if (sqlite3_column_type(stmt, 2) == SQLITE_NULL) {
            *security_scheme_id = -1; // Explicitly set -1 if scheme is NULL
        }

        // Allocate memory for output strings
        *content = note_content ? KFS_STRDUP((const char*)note_content) : NULL;
        *created_at = note_created_at ? KFS_STRDUP((const char*)note_created_at) : NULL;
        *updated_at = note_updated_at ? KFS_STRDUP((const char*)note_updated_at) : NULL;

        // Check for allocation failures
        if ((note_content && !*content) || (note_created_at && !*created_at) || (note_updated_at && !*updated_at)) {
            rc = KFS_NOMEM;
        } else {
            rc = KFS_OK; // Reset rc
        }
    } else { // Should not happen due to permission check
        rc = KFS_NOTFOUND;
    }
    sqlite3_finalize(stmt); stmt = NULL;
    if (rc != KFS_OK) { goto cleanup; }


    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_note: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Free allocated memory
    }

    fprintf(stdout, "[INFO] kfs_get_note: Successfully retrieved note %d in domain %d.\n", note_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Free potentially allocated memory on error
    kfs_mem_free(*content); *content = NULL;
    kfs_mem_free(*created_at); *created_at = NULL;
    kfs_mem_free(*updated_at); *updated_at = NULL;
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Deletes a note from the Notes table.
 * Requires DELETE permission on the Note itself and domain access.
 * Cascade delete in EntityNotes table handles removing links from entities.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the note (for verification).
 * @param note_id ID of the note to delete.
 * @return KFS_OK on success (even if note didn't exist), KFS_INVALID_ARGUMENT,
 *         KFS_PERMISSION_DENIED, KFS_NOTFOUND (if permission check fails), or SQLite error.
 */
int kfs_delete_note(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int note_id) {
    // --- Input Validation ---
     if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || note_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_delete_note: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, note_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, note_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_note: Failed to begin transaction.\n");
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        exec_sql(db->registry_db, "ROLLBACK;", "registry");
        return KFS_ERROR;
    }

    // --- Permission Check: DELETE on the Note ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Note", note_id, KFS_PERM_DELETE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_delete_note: Note ID %d not found or permission check failed with NOTFOUND, treating as success for delete.\n", note_id);
            rc = KFS_OK; // Not found is OK for delete
            goto commit; // Skip actual deletion steps
        }
        fprintf(stderr, "[ERROR] kfs_delete_note: Permission check failed for note %d (rc=%d).\n", note_id, rc);
        goto cleanup; // Permission denied or DB error
    }

     // --- Verify Note Belongs to Domain (Safety Check) ---
    const char* sql_check_domain = "SELECT 1 FROM Notes WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, note_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_delete_note: Note ID %d does not belong to domain %d.\n", note_id, domain_id);
            rc = KFS_NOTFOUND; goto cleanup; // Should be caught by perm check
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_note (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Proceed with deletion ---
    // ON DELETE CASCADE in EntityNotes handles removing links.
    const char* sql = "DELETE FROM Notes WHERE id = ? AND domain_id = ?;"; // Add domain_id for safety
    rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, note_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
         if (rc == SQLITE_DONE) {
             if (sqlite3_changes(db->arch_db) == 0) {
                 fprintf(stderr, "[WARN] kfs_delete_note: Note %d not found during delete (though permission check passed).\n", note_id);
             }
              rc = KFS_OK; // Reset rc
         } else {
              fprintf(stderr, "[ERROR] kfs_delete_note (delete) - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->arch_db), rc);
         }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_delete_note (delete) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    if (rc != KFS_OK) goto cleanup;

commit:
    // --- Commit Transactions ---
     if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_note: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

     if (rc == KFS_OK) { // Only log success if final rc is OK
        fprintf(stdout, "[INFO] kfs_delete_note: Successfully processed delete for note %d in domain %d by user %llu.\n",
               note_id, domain_id, (unsigned long long)requesting_user_uuid);
     }
    return rc; // KFS_OK or KFS_ERROR if commit failed

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}

/**
 * @brief Assigns an existing epic to an existing topic.
 * Checks for WRITE permission on the Topic.
 */
int kfs_assign_epic_to_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int epic_id) {
    if (!db || !db->arch_db || requesting_user_uuid == 0 || topic_id <= 0 || epic_id <= 0) return KFS_INVALID_ARGUMENT;

    // --- Permission Check: WRITE on the Topic ---
    int perm_rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_WRITE);
    if (perm_rc != KFS_OK) return perm_rc;

    // Optional: Check if epic_id exists?

    // --- Proceed with assignment ---
    const char* sql = "INSERT OR IGNORE INTO EpicAssignments (topic_id, epic_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_assign_epic_to_topic - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); sqlite3_finalize(stmt); return rc; }
    sqlite3_bind_int(stmt, 1, topic_id);
    sqlite3_bind_int(stmt, 2, epic_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { fprintf(stderr, "[ERROR] kfs_assign_epic_to_topic - Execute failed: %s\n", sqlite3_errmsg(db->arch_db)); return rc; }
    return KFS_OK;
}

/* ============================================================================== */
/* ==                  LINKING/ASSIGNMENT w/ Permissions                     == */
/* ============================================================================== */
// Add permission checks to all assign/link/unlink functions.
// Requires careful thought about WHICH entity needs the WRITE permission check.
// Let's assume WRITE on the *first* entity listed is sufficient for now.

/**
 * @brief Creates an artifact in a specified domain with associated metadata and asset data.
 * Requires WRITE permission on the domain and validates the security scheme.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain.
 * @param name Name of the artifact.
 * @param type Type of the artifact (e.g., "document").
 * @param format Format of the asset (e.g., "pdf", NULL for none).
 * @param owner_actor_id ID of the owning actor (user or group).
 * @param security_scheme_id ID of the security scheme (optional, < 0 for none).
 * @param data Binary data for the asset (optional, NULL for none).
 * @param data_size Size of the binary data.
 * @param text_data Text data for the asset (optional, NULL for none).
 * @param metadata Metadata for the asset (optional, NULL for none).
 * @param artifact_id Output parameter for the created artifact ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_create_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* name, const char* type, const char* format,
                        int owner_actor_id, int security_scheme_id, const uint8_t* data, size_t data_size, const char* text_data, const char* metadata, int* artifact_id) {
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 ||
        !name || !type || owner_actor_id <= 0 || !artifact_id) {
        fprintf(stderr, "[ERROR] kfs_create_artifact: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, name=%s, type=%s, owner_actor_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, name ? name : "NULL", type ? type : "NULL", owner_actor_id);
        return KFS_INVALID_ARGUMENT;
    }
    if (data_size > 0 && !data) { return KFS_INVALID_ARGUMENT; }
    *artifact_id = -1;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    char* created_at = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_create_artifact: Failed to begin transaction.\n");
        return KFS_ERROR; // Cannot proceed
    }

    // --- Permission Check: WRITE on Domain ---
    // Note: kfs_check_permission handles domain existence check internally
    rc = kfs_check_permission(db, requesting_user_uuid, "Domain", domain_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_create_artifact: Permission check failed for domain %d (rc=%d).\n", domain_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED, KFS_NOTFOUND, or DB error
    }

    // --- Verify Owner Actor ---
    const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ? AND is_active = 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, owner_actor_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_create_artifact: Owner actor ID %d not found or inactive.\n", owner_actor_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
        rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_create_artifact (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }


    // --- Verify Security Scheme (if provided) belongs to the Domain ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, security_scheme_id);
            sqlite3_bind_int(stmt, 2, domain_id);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_create_artifact: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
             rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_create_artifact (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Create Artifact Metadata ---
    created_at = get_current_timestamp();
    if (!created_at) { rc = KFS_NOMEM; goto cleanup; }

    const char* sql_insert_artifact = "INSERT INTO Artifacts (domain_id, type, name, format, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at) "
                                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_create_artifact (insert artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, domain_id); // Bind domain_id
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, format ? format : "", -1, SQLITE_STATIC); // Handle NULL format
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)requesting_user_uuid); // Creator is requester
    sqlite3_bind_int(stmt, 6, owner_actor_id);
    if (security_scheme_id >= 0) sqlite3_bind_int(stmt, 7, security_scheme_id);
    else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_text(stmt, 8, created_at, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, created_at, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) { *artifact_id = (int)sqlite3_last_insert_rowid(db->arch_db); }
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_create_artifact (insert artifact) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT;
        goto cleanup;
    }

    // --- Create Asset Data (if provided) ---
    if (data || text_data || metadata) {
        const char* sql_insert_asset = "INSERT INTO Assets (id, data, text_data, metadata) VALUES (?, ?, ?, ?);";
        rc = sqlite3_prepare_v2(db->artifacts_db, sql_insert_asset, -1, &stmt, NULL);
        if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_create_artifact (insert asset) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db)); goto cleanup; }

        sqlite3_bind_int(stmt, 1, *artifact_id);
        if (data && data_size > 0) sqlite3_bind_blob(stmt, 2, data, (int)data_size, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
        if (text_data) sqlite3_bind_text(stmt, 3, text_data, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 3);
        if (metadata) sqlite3_bind_text(stmt, 4, metadata, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 4);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;

        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_create_artifact (insert asset) - Execute failed: %s\n", sqlite3_errmsg(db->artifacts_db));
            if (sqlite3_errcode(db->artifacts_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT;
            goto cleanup;
        }
    }

    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_create_artifact: Commit failed for artifact %d in domain %d.\n", *artifact_id, domain_id);
        rc = KFS_ERROR; // Mark error for cleanup
        goto cleanup;
    }

    kfs_mem_free(created_at); // Free timestamp only on success or commit failure
    fprintf(stdout, "[INFO] kfs_create_artifact: Successfully created artifact %d in domain %d by user %llu.\n",
            *artifact_id, domain_id, (unsigned long long)requesting_user_uuid);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    kfs_mem_free(created_at); // Free timestamp if allocated
    // Rollback is crucial here
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}

/**
 * @brief Links an asset to an existing artifact by inserting or updating a row in artifacts.db.Assets.
 * Requires WRITE permission on the artifact.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user making the request.
 * @param artifact_id ID of the artifact to link the asset to.
 * @param data Optional BLOB data for the asset.
 * @param data_size Size of the BLOB data.
 * @param text_data Optional TEXT data for the asset.
 * @param metadata Optional JSON metadata for the asset.
 * @return KFS_OK on success, KFS_PERMISSION_DENIED, KFS_NOTFOUND, KFS_INVALID_ARGUMENT,
 *         KFS_CONSTRAINT, or SQLite error.
 */
int kfs_link_asset_to_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id,
                               const void* data, size_t data_size, const char* text_data, const char* metadata) {
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || artifact_id <= 0) {
        return KFS_INVALID_ARGUMENT;
    }
    if (!data && data_size > 0) return KFS_INVALID_ARGUMENT; // Size without data
    if (!data && !text_data && !metadata) return KFS_INVALID_ARGUMENT; // Nothing to link

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK || // Needed for permission check
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_link_asset_to_artifact: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Artifact ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_link_asset_to_artifact: Permission check failed for artifact %d (rc=%d).\n", artifact_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED, KFS_NOTFOUND, or DB error
    }

    // --- Insert or Replace in artifacts.db.Assets ---
    const char* sql = "INSERT OR REPLACE INTO Assets (id, data, text_data, metadata) VALUES (?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_link_asset_to_artifact - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, artifact_id);
    if (data && data_size > 0) sqlite3_bind_blob(stmt, 2, data, (int)data_size, SQLITE_STATIC); else sqlite3_bind_null(stmt, 2);
    if (text_data) sqlite3_bind_text(stmt, 3, text_data, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 3);
    if (metadata) sqlite3_bind_text(stmt, 4, metadata, -1, SQLITE_STATIC); else sqlite3_bind_null(stmt, 4); // Allow NULL metadata


    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_link_asset_to_artifact - Execute failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        if (sqlite3_errcode(db->artifacts_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT;
        else rc = KFS_ERROR;
        goto cleanup;
    }
     rc = KFS_OK; // Reset rc

    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_link_asset_to_artifact: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

    fprintf(stdout, "[INFO] kfs_link_asset_to_artifact: Successfully linked asset to artifact %d.\n", artifact_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Unlinks an artifact from its associated asset by deleting the artifact metadata
 * from architecture.db.Artifacts, preserving the asset in artifacts.db.Assets.
 * Requires WRITE permission on the artifact. Cascades to linked TopicAssignments and EntityNotes.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user making the request.
 * @param artifact_id ID of the artifact to unlink (metadata to delete).
 * @return KFS_OK on success (even if no artifact was found), KFS_PERMISSION_DENIED,
 *         KFS_INVALID_ARGUMENT, KFS_NOTFOUND (if permission check fails), or SQLite error.
 */
int kfs_unlink_asset_from_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || artifact_id <= 0) { // Removed artifacts_db check as we only touch arch/registry
        fprintf(stderr, "[ERROR] kfs_unlink_asset_from_artifact: Invalid arguments (artifact_id=%d).\n", artifact_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_unlink_asset_from_artifact: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Artifact ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_unlink_asset_from_artifact: Artifact ID %d not found, treating as success.\n", artifact_id);
            rc = KFS_OK; // Not found is OK for unlink
            goto commit; // Skip actual deletion
        }
         fprintf(stderr, "[ERROR] kfs_unlink_asset_from_artifact: Permission check failed for artifact %d (rc=%d).\n", artifact_id, rc);
        goto cleanup; // KFS_PERMISSION_DENIED or DB error
    }

    // --- Delete from architecture.db.Artifacts (cascades handle links) ---
    const char* sql = "DELETE FROM Artifacts WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, artifact_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_unlink_asset_from_artifact - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->arch_db), rc);
            goto cleanup;
        }
         if (sqlite3_changes(db->arch_db) == 0) {
            fprintf(stderr, "[WARN] kfs_unlink_asset_from_artifact: No artifact found for ID %d during delete (though permission check passed).\n", artifact_id);
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_unlink_asset_from_artifact - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


commit:
    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_unlink_asset_from_artifact: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

    if (rc == KFS_OK) { // Only log success if final rc is OK
         fprintf(stdout, "[INFO] kfs_unlink_asset_from_artifact: Successfully processed unlink for artifact %d by user %llu. Asset preserved.\n",
                artifact_id, (unsigned long long)requesting_user_uuid);
    }
    return rc;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Deletes an artifact, including its metadata (architecture.db.Artifacts) and associated asset (artifacts.db.Assets).
 * Requires DELETE permission on the artifact and domain access.
 * Handles cascading deletes in architecture.db for linked items (TopicAssignments, EntityNotes).
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the artifact.
 * @param artifact_id ID of the artifact to delete.
 * @return KFS_OK on success (even if artifact didn't exist), KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED,
 *         KFS_NOTFOUND (if permission check fails due to not found), or SQLite error.
 */
int kfs_delete_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id) {
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || artifact_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_delete_artifact: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, artifact_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, artifact_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    int changes_arch = 0;
    int changes_assets = 0;

    // --- Begin Transactions ---
    // Wrap registry check within transactions as well for atomicity
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_artifact: Failed to begin transaction.\n");
        // Attempt rollback on partial begin is complex, better to just return error
        return KFS_ERROR;
    }

    // --- Permission Check: DELETE on the Artifact ---
    // This also implicitly verifies domain access and existence
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_DELETE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_delete_artifact: Artifact ID %d not found or permission check failed with NOTFOUND, treating as success for delete.\n", artifact_id);
            rc = KFS_OK; // Not found is OK for delete
            goto commit; // Skip actual deletion steps
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_delete_artifact: Permission denied for user %llu to delete artifact %d.\n",
                    (unsigned long long)requesting_user_uuid, artifact_id);
        } else {
            fprintf(stderr, "[ERROR] kfs_delete_artifact: Permission check failed with error %d.\n", rc);
        }
        goto cleanup; // Permission denied or DB error during check
    }

    // --- Verify Artifact Belongs to Domain (Safety Check, although permission check implies it) ---
     const char* sql_check_domain = "SELECT 1 FROM Artifacts WHERE id = ? AND domain_id = ?;";
     rc = sqlite3_prepare_v2(db->arch_db, sql_check_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, artifact_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_delete_artifact: Artifact ID %d does not belong to domain %d (or was not found after permission check).\n", artifact_id, domain_id);
            rc = KFS_NOTFOUND; // Treat as NOTFOUND if it vanished
            goto cleanup;
        }
        rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_artifact (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


    // --- Delete from architecture.db.Artifacts (Cascades should handle links) ---
    const char* sql_del_meta = "DELETE FROM Artifacts WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_del_meta, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, artifact_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            changes_arch = sqlite3_changes(db->arch_db);
            rc = KFS_OK; // Reset rc
        } else {
             fprintf(stderr, "[ERROR] kfs_delete_artifact (metadata) - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->arch_db), rc);
        }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_delete_artifact (metadata) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    if (rc != KFS_OK) goto cleanup;


    // --- Delete from artifacts.db.Assets ---
    const char* sql_del_asset = "DELETE FROM Assets WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql_del_asset, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, artifact_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            changes_assets = sqlite3_changes(db->artifacts_db);
            rc = KFS_OK; // Reset rc
        } else {
             fprintf(stderr, "[ERROR] kfs_delete_artifact (asset) - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->artifacts_db), rc);
        }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_delete_artifact (asset) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db)); goto cleanup; }
     if (rc != KFS_OK) goto cleanup;


    // --- Log Consistency Info ---
    if (changes_arch == 0 && changes_assets > 0) {
        fprintf(stderr, "[WARN] kfs_delete_artifact: Inconsistency - asset deleted but metadata not found for ID %d in domain %d.\n", artifact_id, domain_id);
    } else if (changes_arch > 0 && changes_assets == 0) {
        fprintf(stdout, "[INFO] kfs_delete_artifact: Artifact %d metadata deleted, no associated asset found in domain %d.\n", artifact_id, domain_id);
    } else if (changes_arch == 0 && changes_assets == 0) {
        fprintf(stdout, "[INFO] kfs_delete_artifact: No artifact or asset found during delete attempt for ID %d in domain %d.\n", artifact_id, domain_id);
    }

commit:
    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_artifact: Commit failed.\n");
        rc = KFS_ERROR; // Mark error
        goto cleanup; // Attempt rollback
    }

    if (rc == KFS_OK) { // Only log success if final rc is OK
        fprintf(stdout, "[INFO] kfs_delete_artifact: Successfully processed delete for artifact %d in domain %d by user %llu.\n",
                artifact_id, domain_id, (unsigned long long)requesting_user_uuid);
    }
    return rc; // Return KFS_OK or error code

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    // Rollback must be attempted if commit failed or error occurred before commit
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}

/**
 * @brief Creates a new artifact metadata entry in a domain and links it to an *existing* asset
 * by updating the asset's ID in artifacts.db.Assets to match the new artifact's ID.
 * Requires WRITE permission on the domain. If the asset is already linked to an artifact,
 * requires WRITE permission on that existing artifact as well.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user creating the artifact link.
 * @param domain_id ID of the domain where the new artifact metadata will reside.
 * @param creator_uuid UUID of the user creating the artifact metadata.
 * @param owner_actor_id Internal ID of the owning actor for the new artifact metadata.
 * @param type Artifact type (e.g., "script", "image").
 * @param name Artifact name for the new metadata entry.
 * @param format Artifact format (e.g., "python", "png").
 * @param security_scheme_id Optional security scheme ID (-1 for none, must be in domain_id).
 * @param asset_id ID of the *existing* asset to link to the new artifact metadata.
 * @param artifact_id Output parameter for the new artifact metadata ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_NOTFOUND, KFS_PERMISSION_DENIED,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_create_artifact_with_existing_asset(GameDB* db, uint64_t requesting_user_uuid, int domain_id,
                                            uint64_t creator_uuid, int owner_actor_id,
                                            const char* type, const char* name, const char* format,
                                            int security_scheme_id, int asset_id, int* artifact_id) {
    // --- Input Validation ---
     if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 ||
        creator_uuid == 0 || owner_actor_id <= 0 || !type || !name || asset_id <= 0 || !artifact_id) {
        fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, asset_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, asset_id);
        return KFS_INVALID_ARGUMENT;
    }
    *artifact_id = -1;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    char* timestamp = NULL;
    int current_linked_artifact_id = -1; // ID of artifact currently linked to asset_id, if any

    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on Domain ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Domain", domain_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
         fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Domain permission check failed for domain %d (rc=%d).\n", domain_id, rc);
        goto cleanup;
    }

    // --- Verify Asset Exists ---
    const char* sql_check_asset = "SELECT 1 FROM Assets WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql_check_asset, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, asset_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Asset ID %d not found.\n", asset_id);
            rc = KFS_NOTFOUND; goto cleanup;
        }
        rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (check asset) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db)); goto cleanup; }


    // --- Check if Asset is Currently Linked and Check Permissions ---
    const char* sql_check_linked = "SELECT id FROM Artifacts WHERE id = ?;"; // Check if asset ID is used as an artifact ID
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_linked, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, asset_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            current_linked_artifact_id = sqlite3_column_int(stmt, 0);
        } else if (rc != SQLITE_DONE) {
             fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (check linked artifact) - Step failed: %s\n", sqlite3_errmsg(db->arch_db));
             sqlite3_finalize(stmt); stmt = NULL; goto cleanup;
        }
        sqlite3_finalize(stmt); stmt = NULL;
        rc = KFS_OK; // Reset rc
     } else { fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (check linked artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    if (current_linked_artifact_id > 0) {
        // Asset is already linked, need WRITE permission on that *existing* artifact
        rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", current_linked_artifact_id, KFS_PERM_WRITE);
        if (rc != KFS_OK) {
             fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Permission denied on currently linked artifact %d for asset %d (rc=%d).\n",
                    current_linked_artifact_id, asset_id, rc);
            goto cleanup;
        }
    }

    // --- Verify Owner Actor ---
    const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ? AND is_active = 1;";
     rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, owner_actor_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_ROW) {
             fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Owner actor ID %d not found or inactive.\n", owner_actor_id);
             rc = KFS_NOTFOUND; goto cleanup;
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }


    // --- Verify Security Scheme (if provided) ---
    if (security_scheme_id >= 0) { // Use >= 0 to allow scheme 0 if valid
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
         if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, security_scheme_id);
            sqlite3_bind_int(stmt, 2, domain_id); // Ensure scheme is in the correct domain
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt); stmt = NULL;
            if (rc != SQLITE_ROW) {
                fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
                rc = KFS_NOTFOUND; goto cleanup;
            }
             rc = KFS_OK; // Reset rc
        } else { fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db)); goto cleanup; }
    }

    // --- Insert into architecture.db.Artifacts ---
    timestamp = get_current_timestamp();
    if (!timestamp) { rc = KFS_NOMEM; goto cleanup; }

    const char* sql_insert_artifact = "INSERT INTO Artifacts (domain_id, type, name, format, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at) "
                                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (metadata) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, format ? format : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)creator_uuid);
    sqlite3_bind_int(stmt, 6, owner_actor_id);
    if (security_scheme_id >= 0) sqlite3_bind_int(stmt, 7, security_scheme_id); else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_text(stmt, 8, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, timestamp, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) { *artifact_id = (int)sqlite3_last_insert_rowid(db->arch_db); }
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (metadata) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT; else rc = KFS_ERROR;
        goto cleanup;
    }
     rc = KFS_OK; // Reset rc


    // --- Update Asset ID in artifacts.db.Assets ---
    // *** CRITICAL: This changes the PK of the asset row ***
    const char* sql_update_asset = "UPDATE Assets SET id = ? WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql_update_asset, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (asset update) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, *artifact_id); // New ID
    sqlite3_bind_int(stmt, 2, asset_id);    // Old ID
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset (asset update) - Execute failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        // Could be CONSTRAINT if *artifact_id somehow already exists in Assets (shouldn't happen)
        if (sqlite3_errcode(db->artifacts_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT; else rc = KFS_ERROR;
        goto cleanup;
    }

    if (sqlite3_changes(db->artifacts_db) == 0) {
        fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Asset ID %d not found during update (consistency issue?).\n", asset_id);
        rc = KFS_INTERNAL; // Indicates a problem if asset existed before but not now
        goto cleanup;
    }
     rc = KFS_OK; // Reset rc


    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_create_artifact_with_existing_asset: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

    kfs_mem_free(timestamp); // Free timestamp only on success or commit failure
    fprintf(stdout, "[INFO] kfs_create_artifact_with_existing_asset: Successfully created artifact %d linked to asset %d (orig ID) with name '%s'.\n",
            *artifact_id, asset_id, name);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    kfs_mem_free(timestamp); // Free timestamp if allocated
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Updates metadata and optionally the associated asset for an artifact in a specified domain.
 * Requires WRITE permission and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain.
 * @param artifact_id ID of the artifact to update.
 * @param type New type (optional, NULL to keep unchanged).
 * @param name New name (optional, NULL to keep unchanged).
 * @param format New format (optional, NULL to keep unchanged).
 * @param owner_actor_id New owner actor ID (optional, <= 0 to keep unchanged).
 * @param security_scheme_id New security scheme ID (optional, < 0 to keep unchanged, must be in same domain).
 * @param data New binary data for the asset (optional, NULL to keep unchanged).
 * @param data_size Size of the binary data.
 * @param text_data New text data for the asset (optional, NULL to keep unchanged).
 * @param metadata New metadata for the asset (optional, NULL to keep unchanged).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error.
 */
int kfs_update_artifact(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int artifact_id, const char* type, const char* name, const char* format, int owner_actor_id, int security_scheme_id, const uint8_t* data, size_t data_size, const char* text_data, const char* metadata) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || !db->artifacts_db || requesting_actor_uuid == 0 || domain_id <= 0 || artifact_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, artifact_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, artifact_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK ||
        exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Get Requester Actor ID and Check Domain Access ---
    int requester_actor_id = -1;
    const char* sql_get_requester_id = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_get_requester_id, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact (get requester id) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)requesting_actor_uuid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        requester_actor_id = sqlite3_column_int(stmt, 0);
        int is_active = sqlite3_column_int(stmt, 1);
        if (!is_active) {
            fprintf(stderr, "[ERROR] kfs_update_artifact: Requester UUID %llu is inactive.\n",
                    (unsigned long long)requesting_actor_uuid);
            rc = KFS_PERMISSION_DENIED;
        } else {
            rc = KFS_OK;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Requester UUID %llu not found.\n",
                (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_update_artifact (get requester id) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != KFS_OK) {
        goto cleanup;
    }

    // Check domain access
    int has_domain_access = 0;
    const char* sql_check_domain = "SELECT 1 FROM DomainActors WHERE domain_id = ? AND actor_id = ? LIMIT 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_domain, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_int(stmt, 2, requester_actor_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_domain_access = 1;
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_artifact (check domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (!has_domain_access) {
        const char* sql_check_group_domain = "SELECT DA.actor_id FROM DomainActors DA JOIN Actors A ON DA.actor_id = A.id "
                                            "WHERE DA.domain_id = ? AND A.actor_type IN ('GROUP', 'COMPANY');";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_group_domain, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_update_artifact (check group domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, domain_id);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int group_id = sqlite3_column_int(stmt, 0);
            if (is_user_in_group(db, requester_actor_id, group_id)) {
                has_domain_access = 1;
                break;
            }
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_update_artifact (check group domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!has_domain_access) {
            fprintf(stderr, "[ERROR] kfs_update_artifact: Requester %llu lacks access to domain %d.\n",
                    (unsigned long long)requesting_actor_uuid, domain_id);
            rc = KFS_PERMISSION_DENIED;
            goto cleanup;
        }
    }

    // --- Check WRITE Permission on Artifact ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_update_artifact: Artifact ID %d not found.\n", artifact_id);
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_update_artifact: Requester UUID %llu lacks WRITE permission for artifact %d.\n",
                    (unsigned long long)requesting_actor_uuid, artifact_id);
        }
        goto cleanup;
    }

    // --- Verify Artifact Exists in Domain ---
    const char* sql_check_artifact = "SELECT 1 FROM Artifacts WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact (check artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Artifact ID %d not found in domain %d.\n", artifact_id, domain_id);
        rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Verify New Owner (if provided) ---
    if (owner_actor_id > 0) {
        const char* sql_check_owner = "SELECT 1 FROM Actors WHERE id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_owner, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_update_artifact (check owner) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, owner_actor_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_update_artifact: New owner actor ID %d not found.\n", owner_actor_id);
            rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    // --- Verify New Security Scheme (if provided) ---
    if (security_scheme_id >= 0) {
        const char* sql_check_scheme = "SELECT 1 FROM SecuritySchemes WHERE id = ? AND domain_id = ?;";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_scheme, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_update_artifact (check scheme) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, security_scheme_id);
        sqlite3_bind_int(stmt, 2, domain_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "[ERROR] kfs_update_artifact: Security scheme ID %d not found in domain %d.\n", security_scheme_id, domain_id);
            rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    // --- Update Artifact Metadata ---
    char* timestamp = get_current_timestamp();
    if (!timestamp) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Memory allocation failed for timestamp.\n");
        rc = KFS_NOMEM;
        goto cleanup;
    }

    const char* sql_update_artifact = "UPDATE Artifacts SET type = COALESCE(?, type), name = COALESCE(?, name), format = COALESCE(?, format), "
                                      "owner_actor_id = COALESCE(?, owner_actor_id), security_scheme_id = COALESCE(?, security_scheme_id), "
                                      "updated_at = ? WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_update_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact (update artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    if (type) {
        sqlite3_bind_text(stmt, 1, type, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    if (name) {
        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    if (format) {
        sqlite3_bind_text(stmt, 3, format, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    if (owner_actor_id > 0) {
        sqlite3_bind_int(stmt, 4, owner_actor_id);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    if (security_scheme_id >= 0) {
        sqlite3_bind_int(stmt, 5, security_scheme_id);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    sqlite3_bind_text(stmt, 6, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, artifact_id);
    sqlite3_bind_int(stmt, 8, domain_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_artifact (update artifact) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) {
            rc = KFS_CONSTRAINT;
        }
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    if (sqlite3_changes(db->arch_db) == 0) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Artifact ID %d not found in domain %d.\n", artifact_id, domain_id);
        rc = KFS_NOTFOUND;
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    // --- Update or Insert Asset Data (if provided) ---
    if (data || text_data || metadata) {
        const char* sql_update_asset = "INSERT OR REPLACE INTO Assets (id, data, text_data, metadata) VALUES (?, ?, ?, ?);";
        rc = sqlite3_prepare_v2(db->artifacts_db, sql_update_asset, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_update_artifact (update asset) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db));
            kfs_mem_free(timestamp);
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, artifact_id);
        if (data && data_size > 0) {
            sqlite3_bind_blob(stmt, 2, data, (int)data_size, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        sqlite3_bind_text(stmt, 3, text_data ? text_data : "", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, metadata ? metadata : "", -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_update_artifact (update asset) - Execute failed: %s\n", sqlite3_errmsg(db->artifacts_db));
            if (sqlite3_errcode(db->artifacts_db) == SQLITE_CONSTRAINT) {
                rc = KFS_CONSTRAINT;
            }
            kfs_mem_free(timestamp);
            goto cleanup;
        }
    }

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK ||
        exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact: Commit failed.\n");
        kfs_mem_free(timestamp);
        goto cleanup;
    }

    kfs_mem_free(timestamp);
    fprintf(stdout, "[INFO] kfs_update_artifact: Successfully updated artifact %d in domain %d.\n", artifact_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    return rc;
}

/**
 * @brief Updates the name of an artifact in the Artifacts table.
 * Requires WRITE permission on the artifact. Updates the updated_at timestamp.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user making the request.
 * @param artifact_id ID of the artifact to update.
 * @param new_name New name for the artifact (must be non-empty).
 * @return KFS_OK on success, KFS_PERMISSION_DENIED, KFS_NOTFOUND, KFS_INVALID_ARGUMENT,
 *         KFS_CONSTRAINT, KFS_NOMEM, or SQLite error code.
 */
int kfs_update_artifact_name(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, const char* new_name) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || artifact_id <= 0 || !new_name || strlen(new_name) == 0) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name: Invalid arguments (artifact_id=%d, new_name=%s).\n",
                artifact_id, new_name ? new_name : "NULL");
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Permission Check: WRITE on the Artifact ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_update_artifact_name: Artifact ID %d not found.\n", artifact_id);
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_update_artifact_name: Permission denied for user %llu to update artifact %d.\n",
                    (unsigned long long)requesting_user_uuid, artifact_id);
        }
        return rc; // KFS_PERMISSION_DENIED, KFS_NOTFOUND, or DB error
    }

    // --- Begin Transaction ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Update Artifact Name and Timestamp ---
    char* timestamp = get_current_timestamp();
    if (!timestamp) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name: Memory allocation failed for timestamp.\n");
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        return KFS_NOMEM;
    }

    const char* sql = "UPDATE Artifacts SET name = ?, updated_at = ? WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        kfs_mem_free(timestamp);
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        return rc;
    }

    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, artifact_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    kfs_mem_free(timestamp);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) {
            rc = KFS_CONSTRAINT; // Possible if a UNIQUE constraint exists on name
        }
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        return rc;
    }

    if (sqlite3_changes(db->arch_db) == 0) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name: Artifact ID %d not found.\n", artifact_id);
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        return KFS_NOTFOUND; // Should be rare due to permission check
    }

    // --- Commit Transaction ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_update_artifact_name: Commit failed.\n");
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        return KFS_ERROR;
    }

    fprintf(stdout, "[INFO] kfs_update_artifact_name: Successfully updated name for artifact %d to '%s'.\n",
            artifact_id, new_name);
    return KFS_OK;
}

/**
 * @brief Assigns an existing topic to an existing artifact.
 * Requires WRITE permission on the artifact.
 * Verifies both entities exist and belong to the same domain.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param artifact_id ID of the artifact.
 * @param topic_id ID of the topic to assign.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED,
 *         KFS_NOTFOUND, KFS_MISMATCH (if domains differ), or SQLite error.
 */
int kfs_assign_topic_to_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, int topic_id) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || artifact_id <= 0 || topic_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Invalid arguments (artifact_id=%d, topic_id=%d).\n", artifact_id, topic_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    int artifact_domain_id = -1;
    int topic_domain_id = -1;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Artifact ---
    // This also verifies artifact exists and user can access its domain
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Permission check failed for artifact %d (rc=%d).\n", artifact_id, rc);
        goto cleanup;
    }

    // --- Verify Topic Exists and Get its Domain ---
    const char* sql_get_topic_domain = "SELECT domain_id FROM Topics WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_get_topic_domain, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            topic_domain_id = sqlite3_column_int(stmt, 0);
            rc = KFS_OK; // Reset rc
        } else {
            fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Topic ID %d not found.\n", topic_id);
            rc = KFS_NOTFOUND;
        }
        sqlite3_finalize(stmt); stmt = NULL;
    } else { fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact (get topic domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
    if (rc != KFS_OK) goto cleanup;


    // --- Get Artifact Domain (redundant but safe) ---
    const char* sql_get_artifact_domain = "SELECT domain_id FROM Artifacts WHERE id = ?;";
     rc = sqlite3_prepare_v2(db->arch_db, sql_get_artifact_domain, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, artifact_id);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            artifact_domain_id = sqlite3_column_int(stmt, 0);
            rc = KFS_OK; // Reset rc
        } else {
            // Should have been caught by permission check, but handle defensively
            fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Artifact ID %d not found (after permission check!).\n", artifact_id);
            rc = KFS_NOTFOUND;
        }
        sqlite3_finalize(stmt); stmt = NULL;
     } else { fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact (get artifact domain) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }
     if (rc != KFS_OK) goto cleanup;

    // --- Check Domain Match ---
    if (artifact_domain_id != topic_domain_id) {
        fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Domain mismatch - Artifact %d (domain %d) and Topic %d (domain %d).\n",
                artifact_id, artifact_domain_id, topic_id, topic_domain_id);
        rc = KFS_MISMATCH; // Use MISMATCH for domain error
        goto cleanup;
    }

    // --- Proceed with Assignment ---
    const char* sql_insert = "INSERT OR IGNORE INTO TopicAssignments (artifact_id, topic_id) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact (insert) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, topic_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact (insert) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        if (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) rc = KFS_CONSTRAINT;
        goto cleanup;
    }
     if (sqlite3_changes(db->arch_db) == 0) {
         fprintf(stdout, "[INFO] kfs_assign_topic_to_artifact: Link between artifact %d and topic %d already exists.\n", artifact_id, topic_id);
     }

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_assign_topic_to_artifact: Successfully assigned topic %d to artifact %d.\n", topic_id, artifact_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt is finalized if error occurred mid-operation
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Assigns an existing topic to an existing artifact using the topic's name.
 * Requires WRITE permission on the artifact. Verifies topic exists in the specified domain.
 * Assumes artifact and topic should be in the same domain (checked by kfs_assign_topic_to_artifact).
 *
 * @param db The GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id The ID of the domain where the topic and artifact reside.
 * @param artifact_id The ID of the artifact.
 * @param topic_name The name of the topic to assign.
 * @return KFS_OK on success, KFS_NOTFOUND if topic name not found in domain,
 *         KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, or other SQLite error code.
 */
int kfs_assign_topic_to_artifact_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id, const char* topic_name) {
    // Input Validation
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || artifact_id <= 0 || !topic_name || strlen(topic_name) == 0) {
         fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact_by_name: Invalid argument (domain=%d, artifact_id=%d, topic_name=%s).\n",
                 domain_id, artifact_id, topic_name ? topic_name : "NULL");
        return KFS_INVALID_ARGUMENT;
    }

    int topic_id = -1;
    int rc = KFS_OK;

    // --- Lookup topic_id from topic_name within the domain ---
    rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &topic_id);
    if (rc != KFS_OK) {
         if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact_by_name: Topic '%s' not found in domain %d.\n", topic_name, domain_id);
         } else {
            fprintf(stderr, "[ERROR] kfs_assign_topic_to_artifact_by_name: Error finding topic ID for '%s' (rc=%d).\n", topic_name, rc);
         }
        return rc; // KFS_NOTFOUND or DB error
    }

    // Call the ID-based assignment function which handles permissions and domain matching
    return kfs_assign_topic_to_artifact(db, requesting_user_uuid, artifact_id, topic_id);
}

/**
 * @brief Removes the assignment link between a specific artifact and a specific topic.
 * Operates on the TopicAssignments table in architecture.db.
 *
 * @param db The GameDB handle.
 * @param artifact_id The ID of the artifact.
 * @param topic_id The ID of the topic to unassign.
 * @return KFS_OK on success (even if the link didn't exist), KFS_INVALID_ARGUMENT,
 *         or other SQLite error code.
 */
int kfs_remove_topic_from_artifact(GameDB* db, uint64_t requesting_user_uuid, int artifact_id, int topic_id) {
     if (!db || !db->arch_db || requesting_user_uuid == 0 || artifact_id <= 0 || topic_id <= 0) return KFS_INVALID_ARGUMENT;
    int perm_rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (perm_rc != KFS_OK && perm_rc != KFS_NOTFOUND) return perm_rc; // Allow if artifact not found? Maybe not. Let check handle NOTFOUND.
     if (perm_rc != KFS_OK) return perm_rc;
    if (!db || !db->arch_db || artifact_id <= 0 || topic_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_remove_topic_from_artifact: Invalid argument (artifact_id=%d, topic_id=%d).\n", artifact_id, topic_id);
        return KFS_INVALID_ARGUMENT;
    }

    const char* sql = "DELETE FROM TopicAssignments WHERE artifact_id = ? AND topic_id = ?;";
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_remove_topic_from_artifact - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        sqlite3_finalize(stmt);
        return rc;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, topic_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt); // Finalize statement after step

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_remove_topic_from_artifact - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        return rc; // Return specific SQLite error
    }

    // Check if any rows were actually deleted (optional info)
    // if (sqlite3_changes(db->arch_db) == 0) {
    //     fprintf(stderr, "[WARN] kfs_remove_topic_from_artifact: Link between artifact %d and topic %d not found (or already removed).\n", artifact_id, topic_id);
    // }

    // Return OK even if the link didn't exist
    return KFS_OK;
}

/**
 * @brief Saves a new artifact with BLOB data (REVISED for Actor Model).
 * Requires creator's UUID and owner's internal Actor ID.
 */
int kfs_create_artifact_and_asset(GameDB* db,
             uint64_t creator_uuid,     // Creator's KFS UUID
             int owner_actor_id,        // Owner's internal Actor ID
             int security_scheme_id,    // Optional Scheme ID
             const char* type, const char* name, const char* format,
             const void* data, size_t data_size,
             const char* metadata,
             const char** topics, int topic_count,
             int* artifact_id) // Output parameter
{
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !artifact_id ||
        creator_uuid == 0 || owner_actor_id <= 0 || // Validate new params
        !type || !name ) {
        return KFS_INVALID_ARGUMENT;
    }
    if (topic_count < 0 || (topic_count > 0 && !topics)) return KFS_INVALID_ARGUMENT;
    *artifact_id = -1;

    int rc = KFS_OK;
    int current_artifact_id = -1;

    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK) return KFS_ERROR;
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK) {
        exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts"); return KFS_ERROR;
    }

    // --- Call internal save function (inserts into Artifacts and Assets) ---
    rc = kfs_save_asset(db, type, name, format, creator_uuid, owner_actor_id, security_scheme_id, data, data_size, NULL /* text_data */, metadata, &current_artifact_id);

    if (rc != KFS_OK) { goto save_rollback; }
    if (current_artifact_id <= 0) { rc = KFS_INTERNAL; goto save_rollback; }

    // --- Assign topics if provided ---
    for (int i = 0; i < topic_count; i++) {
        if (topics[i] && strlen(topics[i]) > 0) {
            rc = kfs_link_topic_to_artifact_by_name_internal(db, 0, current_artifact_id, topics[i]);
            if (rc != KFS_OK) { goto save_rollback; }
        }
    }

    // --- Commit Transactions ---
    int commit_rc1 = exec_sql(db->artifacts_db, "COMMIT;", "artifacts");
    int commit_rc2 = exec_sql(db->arch_db, "COMMIT;", "architecture");
    if (commit_rc1 == KFS_OK && commit_rc2 == KFS_OK) {
        *artifact_id = current_artifact_id;
        return KFS_OK;
    } else { /* Rollback */ exec_sql(db->artifacts_db,"ROLLBACK;","artifacts"); exec_sql(db->arch_db,"ROLLBACK;","architecture"); return KFS_ERROR; }

save_rollback:
    // Rollback
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    return rc;
}

/**
 * @brief Retrieves an artifact from a specified domain.
 * Requires READ permission and domain access. Indicates if an associated asset exists.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain.
 * @param artifact_id ID of the artifact to retrieve.
 * @param owner_actor_id Output parameter for the owner actor ID.
 * @param type Output parameter for the artifact type (caller must free).
 * @param name Output parameter for the artifact name (caller must free).
 * @param format Output parameter for the artifact format (caller must free, may be NULL).
 * @param security_scheme_id Output parameter for the security scheme ID (-1 if none).
 * @param creator_uuid Output parameter for the creator UUID.
 * @param created_at Output parameter for the creation timestamp (caller must free).
 * @param updated_at Output parameter for the update timestamp (caller must free).
 * @param has_asset Output parameter indicating if an asset exists (1 if true, 0 if false).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_get_artifact(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int artifact_id, int* owner_actor_id, char** type, char** name, char** format, int* security_scheme_id, uint64_t* creator_uuid, char** created_at, char** updated_at, int* has_asset) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || !db->artifacts_db || requesting_actor_uuid == 0 || domain_id <= 0 || artifact_id <= 0 ||
        !owner_actor_id || !type || !name || !format || !security_scheme_id || !creator_uuid || !created_at || !updated_at || !has_asset) {
        fprintf(stderr, "[ERROR] kfs_get_artifact: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, artifact_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, artifact_id);
        return KFS_INVALID_ARGUMENT;
    }
    *owner_actor_id = -1;
    *type = NULL;
    *name = NULL;
    *format = NULL;
    *security_scheme_id = -1;
    *creator_uuid = 0;
    *created_at = NULL;
    *updated_at = NULL;
    *has_asset = 0;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK ||
        exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_artifact: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Get Requester Actor ID and Check Domain Access ---
    int requester_actor_id = -1;
    const char* sql_get_requester_id = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_get_requester_id, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_artifact (get requester id) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)requesting_actor_uuid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        requester_actor_id = sqlite3_column_int(stmt, 0);
        int is_active = sqlite3_column_int(stmt, 1);
        if (!is_active) {
            fprintf(stderr, "[ERROR] kfs_get_artifact: Requester UUID %llu is inactive.\n",
                    (unsigned long long)requesting_actor_uuid);
            rc = KFS_PERMISSION_DENIED;
        } else {
            rc = KFS_OK;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_get_artifact: Requester UUID %llu not found.\n",
                (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_get_artifact (get requester id) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != KFS_OK) {
        goto cleanup;
    }

    // Check domain access
    int has_domain_access = 0;
    const char* sql_check_domain = "SELECT 1 FROM DomainActors WHERE domain_id = ? AND actor_id = ? LIMIT 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_domain, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_artifact (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_int(stmt, 2, requester_actor_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_domain_access = 1;
        rc = KFS_OK;
    } else if (rc == SQLITE_DONE) {
        rc = KFS_OK;
    } else {
        fprintf(stderr, "[ERROR] kfs_get_artifact (check domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (!has_domain_access) {
        const char* sql_check_group_domain = "SELECT DA.actor_id FROM DomainActors DA JOIN Actors A ON DA.actor_id = A.id "
                                            "WHERE DA.domain_id = ? AND A.actor_type IN ('GROUP', 'COMPANY');";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_group_domain, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_get_artifact (check group domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, domain_id);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int group_id = sqlite3_column_int(stmt, 0);
            if (is_user_in_group(db, requester_actor_id, group_id)) {
                has_domain_access = 1;
                break;
            }
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_get_artifact (check group domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!has_domain_access) {
            fprintf(stderr, "[ERROR] kfs_get_artifact: Requester %llu lacks access to domain %d.\n",
                    (unsigned long long)requesting_actor_uuid, domain_id);
            rc = KFS_PERMISSION_DENIED;
            goto cleanup;
        }
        rc = KFS_OK;
    }

    // --- Check READ Permission on Artifact ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Artifact", artifact_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_get_artifact: Artifact ID %d not found.\n", artifact_id);
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_get_artifact: Requester UUID %llu lacks READ permission for artifact %d.\n",
                    (unsigned long long)requesting_actor_uuid, artifact_id);
        }
        goto cleanup;
    }

    // --- Fetch Artifact Details ---
    const char* sql_artifact = "SELECT type, name, format, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at "
                              "FROM Artifacts WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_artifact (artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* artifact_type = sqlite3_column_text(stmt, 0);
        const unsigned char* artifact_name = sqlite3_column_text(stmt, 1);
        const unsigned char* artifact_format = sqlite3_column_text(stmt, 2);
        *creator_uuid = (uint64_t)sqlite3_column_int64(stmt, 3);
        *owner_actor_id = sqlite3_column_int(stmt, 4);
        *security_scheme_id = sqlite3_column_int(stmt, 5);
        const unsigned char* artifact_created_at = sqlite3_column_text(stmt, 6);
        const unsigned char* artifact_updated_at = sqlite3_column_text(stmt, 7);

        if (sqlite3_column_type(stmt, 5) == SQLITE_NULL) {
            *security_scheme_id = -1;
        }

        *type = artifact_type ? KFS_STRDUP((const char*)artifact_type) : NULL;
        *name = artifact_name ? KFS_STRDUP((const char*)artifact_name) : NULL;
        *format = artifact_format ? KFS_STRDUP((const char*)artifact_format) : NULL;
        *created_at = artifact_created_at ? KFS_STRDUP((const char*)artifact_created_at) : NULL;
        *updated_at = artifact_updated_at ? KFS_STRDUP((const char*)artifact_updated_at) : NULL;

        if ((artifact_type && !*type) || (artifact_name && !*name) || (artifact_format && !*format) ||
            (artifact_created_at && !*created_at) || (artifact_updated_at && !*updated_at)) {
            kfs_mem_free(*type);
            kfs_mem_free(*name);
            kfs_mem_free(*format);
            kfs_mem_free(*created_at);
            kfs_mem_free(*updated_at);
            *type = NULL;
            *name = NULL;
            *format = NULL;
            *created_at = NULL;
            *updated_at = NULL;
            sqlite3_finalize(stmt);
            fprintf(stderr, "[ERROR] kfs_get_artifact: Memory allocation failed for artifact details.\n");
            rc = KFS_NOMEM;
            goto cleanup;
        }
        rc = KFS_OK;
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_get_artifact: Artifact ID %d not found in domain %d.\n", artifact_id, domain_id);
        rc = KFS_NOTFOUND;
        sqlite3_finalize(stmt);
        goto cleanup;
    } else {
        fprintf(stderr, "[ERROR] kfs_get_artifact (artifact) - Step failed: %s\n", sqlite3_errmsg(db->arch_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Check for Associated Asset ---
    const char* sql_asset = "SELECT 1 FROM Assets WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql_asset, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_artifact (asset check) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        kfs_mem_free(*type);
        kfs_mem_free(*name);
        kfs_mem_free(*format);
        kfs_mem_free(*created_at);
        kfs_mem_free(*updated_at);
        *type = NULL;
        *name = NULL;
        *format = NULL;
        *created_at = NULL;
        *updated_at = NULL;
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *has_asset = 1;
        rc = KFS_OK;
    } else if (rc == SQLITE_DONE) {
        rc = KFS_OK;
    } else {
        fprintf(stderr, "[ERROR] kfs_get_artifact (asset check) - Step failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        kfs_mem_free(*type);
        kfs_mem_free(*name);
        kfs_mem_free(*format);
        kfs_mem_free(*created_at);
        kfs_mem_free(*updated_at);
        *type = NULL;
        *name = NULL;
        *format = NULL;
        *created_at = NULL;
        *updated_at = NULL;
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK ||
        exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_artifact: Commit failed.\n");
        kfs_mem_free(*type);
        kfs_mem_free(*name);
        kfs_mem_free(*format);
        kfs_mem_free(*created_at);
        kfs_mem_free(*updated_at);
        *type = NULL;
        *name = NULL;
        *format = NULL;
        *created_at = NULL;
        *updated_at = NULL;
        rc = KFS_ERROR;
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_get_artifact: Successfully retrieved artifact %d in domain %d (has_asset=%d).\n", artifact_id, domain_id, *has_asset);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    return rc;
}

/**
 * @brief Loads an artifact by its ID within a specified domain, merging data from architecture and artifacts databases.
 * Includes associated topics and notes. Checks READ permission and Domain access before loading.
 * Allocates memory for output strings/arrays, which the caller must free.
 *
 * @param db The GameDB handle.
 * @param requesting_actor_uuid The 64-bit KFS UUID of the user making the request.
 * @param domain_id ID of the domain containing the artifact.
 * @param artifact_id ID of the artifact to load.
 * @param type Output parameter for artifact type (caller must free).
 * @param name Output parameter for artifact name (caller must free).
 * @param format Output parameter for artifact format (caller must free, may be NULL).
 * @param creator_uuid Output parameter for creator UUID.
 * @param owner_actor_id Output parameter for owner actor ID.
 * @param security_scheme_id Output parameter for security scheme ID (-1 if none).
 * @param data Output parameter for asset binary data (caller must free, may be NULL).
 * @param data_size Output parameter for size of binary data.
 * @param text_data Output parameter for asset text data (caller must free, may be NULL).
 * @param metadata Output parameter for asset metadata (caller must free, may be NULL).
 * @param topics Output array of topic names (caller must free array and strings).
 * @param topic_count Output number of topics.
 * @param notes Output array of KFS_Note structs (caller must free array and structs using kfs_note_free).
 * @param note_count Output number of notes.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, KFS_INTERNAL, or SQLite error.
 */
int kfs_load_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id,
                      char** type, char** name, char** format, uint64_t* creator_uuid, int* owner_actor_id, int* security_scheme_id,
                      uint8_t** data, size_t* data_size, char** text_data, char** metadata,
                      char*** topics, int* topic_count, KFS_Note*** notes, int* note_count) {
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || artifact_id <= 0 ||
        !type || !name || !format || !creator_uuid || !owner_actor_id || !security_scheme_id ||
        !data || !data_size || !text_data || !metadata || !topics || !topic_count || !notes || !note_count) {
        fprintf(stderr, "[ERROR] kfs_load_artifact: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, artifact_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, artifact_id);
        return KFS_INVALID_ARGUMENT;
    }
    // Initialize all output pointers to NULL/0
    *type = NULL; *name = NULL; *format = NULL; *creator_uuid = 0; *owner_actor_id = -1; *security_scheme_id = -1;
    *data = NULL; *data_size = 0; *text_data = NULL; *metadata = NULL;
    *topics = NULL; *topic_count = 0; *notes = NULL; *note_count = 0;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;
    char *created_at_temp = NULL, *updated_at_temp = NULL; // Temp vars for get_artifact
    int has_asset = 0;

    // --- Begin Transactions (Read-only, but good practice for consistency) ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_load_artifact: Failed to begin transaction.\n");
        goto cleanup_error; // Use separate error label
    }

    // --- Load Artifact Metadata & Check Permission ---
    // kfs_get_artifact internally performs the READ permission check on the artifact
    rc = kfs_get_artifact(db, requesting_user_uuid, domain_id, artifact_id,
                          owner_actor_id, type, name, format, security_scheme_id,
                          creator_uuid, &created_at_temp, &updated_at_temp, &has_asset);
    kfs_mem_free(created_at_temp); // Don't need these timestamps here
    kfs_mem_free(updated_at_temp);
    if (rc != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_load_artifact: Failed to get artifact metadata or permission denied (rc=%d).\n", rc);
        goto cleanup_error; // Permission denied, not found, or DB error
    }

    // --- Load Asset Data (if exists) ---
    if (has_asset) {
        // kfs_get_asset_data also implicitly checks permissions via its own call to kfs_check_permission
        rc = kfs_get_asset_data(db, requesting_user_uuid, domain_id, artifact_id, data, data_size, text_data, metadata);
        if (rc != KFS_OK && rc != KFS_NOTFOUND) { // Treat NOTFOUND as non-fatal here
            fprintf(stderr, "[ERROR] kfs_load_artifact: Failed to load asset data (rc=%d).\n", rc);
            goto cleanup_error;
        }
        // If rc == KFS_NOTFOUND, data/text_data/metadata remain NULL/0, which is fine.
        rc = KFS_OK; // Reset rc if it was KFS_NOTFOUND
    }

    // --- Load Associated Topics ---
    const char* sql_topic = "SELECT T.id, T.name FROM TopicAssignments TA JOIN Topics T ON TA.topic_id = T.id "
                           "WHERE TA.artifact_id = ? AND T.domain_id = ? ORDER BY T.name;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_topic, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { /* Handle error */ goto cleanup_error; }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int topic_id = sqlite3_column_int(stmt, 0);
        const unsigned char* topic_name_raw = sqlite3_column_text(stmt, 1);

        // Check READ permission on the specific Topic
        int perm_rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_READ);
        if (perm_rc == KFS_OK) {
            if (!topic_name_raw) continue; // Skip NULL names
            char* topic_name_dup = KFS_STRDUP((const char*)topic_name_raw);
            if (!topic_name_dup) { rc = KFS_NOMEM; break; }

            char** temp_topics = KFS_REALLOC(*topics, (*topic_count + 1) * sizeof(char*));
            if (!temp_topics) { kfs_mem_free(topic_name_dup); rc = KFS_NOMEM; break; }
            *topics = temp_topics;
            (*topics)[*topic_count] = topic_name_dup;
            (*topic_count)++;
        } else if (perm_rc != KFS_PERMISSION_DENIED && perm_rc != KFS_NOTFOUND) {
            // Propagate real errors
            rc = perm_rc; break;
        }
        // Skip topic if permission denied or not found
    }
    sqlite3_finalize(stmt); stmt = NULL;
    if (rc != SQLITE_DONE && rc != SQLITE_ROW && rc != KFS_NOMEM) { goto cleanup_error; } // Check for step errors
    if (rc == KFS_NOMEM) { goto cleanup_error; }
    rc = KFS_OK; // Reset rc


    // --- Load Associated Notes ---
    const char* sql_notes = "SELECT note_id FROM EntityNotes WHERE entity_type = 'Artifact' AND entity_id = ? ORDER BY note_id;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_notes, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { /* Handle error */ goto cleanup_error; }

    sqlite3_bind_int(stmt, 1, artifact_id);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int note_id = sqlite3_column_int(stmt, 0);
        if (note_id <= 0) continue; // Skip invalid IDs

        // kfs_get_note will perform its own permission check internally
        KFS_Note* current_note = KFS_MALLOC(sizeof(KFS_Note));
        if (!current_note) { rc = KFS_NOMEM; break; }
        memset(current_note, 0, sizeof(KFS_Note));

        int get_note_rc = kfs_get_note(db, requesting_user_uuid, domain_id, note_id,
                                     &current_note->owner_actor_id, &current_note->content,
                                     &current_note->security_scheme_id, &current_note->creator_uuid,
                                     &current_note->created_at, &current_note->updated_at);

        if (get_note_rc == KFS_OK) {
            current_note->id = note_id; // Set ID explicitly
            current_note->domain_id = domain_id; // Set domain ID

            KFS_Note** temp_notes = KFS_REALLOC(*notes, (*note_count + 1) * sizeof(KFS_Note*));
            if (!temp_notes) { kfs_note_free(current_note); rc = KFS_NOMEM; break; }
            *notes = temp_notes;
            (*notes)[*note_count] = current_note; // Ownership transferred
            (*note_count)++;
        } else {
            kfs_mem_free(current_note); // Free the allocated struct if get failed
            if (get_note_rc != KFS_PERMISSION_DENIED && get_note_rc != KFS_NOTFOUND) {
                rc = get_note_rc; // Propagate real errors
                break;
            }
            // Skip note if permission denied or not found
        }
    }
    sqlite3_finalize(stmt); stmt = NULL;
    if (rc != SQLITE_DONE && rc != SQLITE_ROW && rc != KFS_NOMEM) { goto cleanup_error; } // Check step errors
    if (rc == KFS_NOMEM) { goto cleanup_error; }


    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_load_artifact: Commit failed.\n");
        rc = KFS_ERROR; // Mark error
        goto cleanup_error;
    }

    fprintf(stdout, "[INFO] kfs_load_artifact: Successfully loaded artifact %d in domain %d.\n", artifact_id, domain_id);
    return KFS_OK;

cleanup_error:
    // Free any partially allocated memory before returning error
    kfs_mem_free(*type); kfs_mem_free(*name); kfs_mem_free(*format); kfs_mem_free(*data); kfs_mem_free(*text_data); kfs_mem_free(*metadata);
    for (int i = 0; i < *topic_count; i++) kfs_mem_free((*topics)[i]); kfs_mem_free(*topics);
    for (int i = 0; i < *note_count; i++) kfs_note_free((*notes)[i]); kfs_mem_free(*notes);
    // Reset pointers
    *type = NULL; *name = NULL; *format = NULL; *data = NULL; *text_data = NULL; *metadata = NULL;
    *topics = NULL; *topic_count = 0; *notes = NULL; *note_count = 0;

    sqlite3_finalize(stmt); // Finalize stmt if error happened mid-loop
    // Rollback transactions
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the error code that caused the jump to cleanup
}

/**
 * @brief Deletes an artifact, including its metadata (architecture.db.Artifacts) and associated asset (artifacts.db.Assets).
 * Requires DELETE permission on the artifact and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain containing the artifact.
 * @param artifact_id ID of the artifact to delete.
 * @return KFS_OK on success (even if artifact didn't exist), KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED,
 *         KFS_NOTFOUND, or SQLite error.
 */
int kfs_erase_artifact(GameDB* db, uint64_t requesting_user_uuid, int domain_id, int artifact_id) {
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || artifact_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact: Invalid arguments (requesting_user_uuid=%llu, domain_id=%d, artifact_id=%d).\n",
                (unsigned long long)requesting_user_uuid, domain_id, artifact_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact: Failed to begin transaction.\n");
        exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
        exec_sql(db->arch_db, "ROLLBACK;", "architecture");
        exec_sql(db->registry_db, "ROLLBACK;", "registry");
        return KFS_ERROR;
    }

    // --- Permission Check: DELETE on the Artifact ---
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_DELETE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_erase_artifact: Artifact ID %d not found in domain %d, treating as success.\n", artifact_id, domain_id);
            rc = KFS_OK; // Not found is OK for delete
            goto commit;
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_erase_artifact: Permission denied for user %llu to delete artifact %d in domain %d.\n",
                    (unsigned long long)requesting_user_uuid, artifact_id, domain_id);
        }
        goto cleanup;
    }

    // --- Verify Artifact in Domain ---
    const char* sql_check_artifact = "SELECT 1 FROM Artifacts WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact (check artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "[INFO] kfs_erase_artifact: Artifact ID %d not found in domain %d, treating as success.\n", artifact_id, domain_id);
        rc = KFS_OK; // Not found is OK for delete
        sqlite3_finalize(stmt);
        goto commit;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Delete from architecture.db.Artifacts (cascades to TopicAssignments, EntityNotes) ---
    const char* sql_del_meta = "DELETE FROM Artifacts WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_del_meta, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact (metadata) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact (metadata) - Execute failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    int changes_arch = sqlite3_changes(db->arch_db);

    // --- Delete from artifacts.db.Assets ---
    const char* sql_del_asset = "DELETE FROM Assets WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql_del_asset, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact (asset) - Execute failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact (asset) - Execute failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        goto cleanup;
    }

    int changes_assets = sqlite3_changes(db->artifacts_db);

    // --- Consistency Check ---
    if (changes_arch == 0 && changes_assets > 0) {
        fprintf(stderr, "[WARN] kfs_erase_artifact: Inconsistency - asset deleted but metadata not found for ID %d in domain %d.\n", artifact_id, domain_id);
    } else if (changes_arch > 0 && changes_assets == 0) {
        fprintf(stderr, "[INFO] kfs_erase_artifact: Artifact %d metadata deleted, no associated asset found in domain %d.\n", artifact_id, domain_id);
    } else if (changes_arch == 0 && changes_assets == 0) {
        fprintf(stderr, "[INFO] kfs_erase_artifact: No artifact or asset found for ID %d in domain %d.\n", artifact_id, domain_id);
    }

commit:
    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_erase_artifact: Commit failed for artifact %d in domain %d.\n", artifact_id, domain_id);
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_erase_artifact: Successfully deleted artifact %d in domain %d by user %llu.\n",
            artifact_id, domain_id, (unsigned long long)requesting_user_uuid);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/**
 * @brief Frees the contents of a KFS_ArtifactInfo struct.
 */
void kfs_artifact_info_free_contents(KFS_ArtifactInfo* info) {
    if (!info) return;
    kfs_mem_free(info->name); info->name = NULL;
    kfs_mem_free(info->type); info->type = NULL;
    info->id = 0;
}

/**
 * @brief Begins an iteration over artifacts in a domain that the user has permission to read.
 * Prepares a query and fetches the first result. The caller MUST call kfs_list_artifacts_end
 * to free resources, even if this function fails or finds no results.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user.
 * @param domain_id The domain to search within.
 * @param query_stmt Output parameter for the iterator state (a prepared statement handle).
 * @param first_artifact_info A struct to be filled with the first artifact's info.
 * @return KFS_OK if at least one artifact is found, KFS_NOTFOUND if the list is empty,
 *         or another error code on failure.
 */
int kfs_list_artifacts_begin(GameDB* db, uint64_t requesting_user_uuid, int domain_id, sqlite3_stmt** query_stmt, KFS_ArtifactInfo* first_artifact_info) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !query_stmt || !first_artifact_info) {
        return KFS_INVALID_ARGUMENT;
    }
    *query_stmt = NULL;
    memset(first_artifact_info, 0, sizeof(KFS_ArtifactInfo));

    int rc = KFS_OK;
    int requester_actor_id = -1;
    int is_requester_admin = 0;

    // --- Begin Read Transaction ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        return KFS_ERROR;
    }

    // --- Get Requester Info & Admin Status ---
    rc = get_active_actor_info_by_uuid(db, requesting_user_uuid, &requester_actor_id, NULL, NULL, &is_requester_admin);
    if (rc != KFS_OK) {
        goto cleanup;
    }

    const char* sql_query;

    if (is_requester_admin) {
        // --- ADMIN QUERY (Simple: Get all artifacts in the domain) ---
        sql_query = "SELECT id, name, type FROM Artifacts WHERE domain_id = ? ORDER BY id;";
        rc = sqlite3_prepare_v2(db->arch_db, sql_query, -1, query_stmt, NULL);
        if (rc != SQLITE_OK) goto cleanup;
        sqlite3_bind_int(*query_stmt, 1, domain_id);
    } else {
        // --- NON-ADMIN QUERY (Complex: Filter based on ownership and schemes) ---
        sql_query = 
            "SELECT DISTINCT a.id, a.name, a.type FROM Artifacts a "
            // JOINs for group-based scheme checks
            "LEFT JOIN SchemeAllowedActors saa ON a.security_scheme_id = saa.security_scheme_id "
            "LEFT JOIN GroupMembers gm ON saa.actor_id = gm.group_actor_id "
            "WHERE a.domain_id = ? AND ( "
            // 1. Direct Ownership
            "    a.owner_actor_id = ? "
            // 2. Group Ownership
            "    OR a.owner_actor_id IN (SELECT group_actor_id FROM GroupMembers WHERE member_actor_id = ?) "
            // 3. Direct Scheme Grant (Read)
            "    OR a.security_scheme_id IN (SELECT security_scheme_id FROM SchemeAllowedActors WHERE actor_id = ? AND can_read = 1) "
            // 4. Group-based Scheme Grant (Read) via the JOINs
            "    OR (gm.member_actor_id = ? AND saa.can_read = 1) "
            ") ORDER BY a.id;";

        rc = sqlite3_prepare_v2(db->arch_db, sql_query, -1, query_stmt, NULL);
        if (rc != SQLITE_OK) goto cleanup;
        
        sqlite3_bind_int(*query_stmt, 1, domain_id);
        sqlite3_bind_int(*query_stmt, 2, requester_actor_id);
        sqlite3_bind_int(*query_stmt, 3, requester_actor_id);
        sqlite3_bind_int(*query_stmt, 4, requester_actor_id);
        sqlite3_bind_int(*query_stmt, 5, requester_actor_id);
    }

    // --- Fetch the first result ---
    rc = kfs_list_artifacts_next(*query_stmt, first_artifact_info);
    if (rc == KFS_OK) {
        return KFS_OK; // First item found and returned.
    } else {
        // If KFS_NOTFOUND, the list is empty. If it's another error, propagate it.
        kfs_list_artifacts_end(db, *query_stmt); // Clean up immediately.
        *query_stmt = NULL;
        return rc; // Will be KFS_NOTFOUND or an error code.
    }

cleanup:
    kfs_list_artifacts_end(db, *query_stmt);
    *query_stmt = NULL;
    return (rc == KFS_OK) ? KFS_ERROR : rc;
}

/**
 * @brief Fetches the next artifact in an ongoing iteration.
 *
 * @param query_stmt The iterator state handle from kfs_list_artifacts_begin.
 * @param next_artifact_info A struct to be filled with the next artifact's info.
 * @return KFS_OK if another artifact was found, KFS_NOTFOUND when the list is exhausted.
 */
int kfs_list_artifacts_next(sqlite3_stmt* query_stmt, KFS_ArtifactInfo* next_artifact_info) {
    if (!query_stmt || !next_artifact_info) {
        return KFS_INVALID_ARGUMENT;
    }
    memset(next_artifact_info, 0, sizeof(KFS_ArtifactInfo));

    int rc = sqlite3_step(query_stmt);

    if (rc == SQLITE_ROW) {
        next_artifact_info->id = sqlite3_column_int(query_stmt, 0);
        const unsigned char* name_raw = sqlite3_column_text(query_stmt, 1);
        const unsigned char* type_raw = sqlite3_column_text(query_stmt, 2);

        next_artifact_info->name = name_raw ? KFS_STRDUP((const char*)name_raw) : NULL;
        next_artifact_info->type = type_raw ? KFS_STRDUP((const char*)type_raw) : NULL;

        if ((name_raw && !next_artifact_info->name) || (type_raw && !next_artifact_info->type)) {
            kfs_artifact_info_free_contents(next_artifact_info);
            return KFS_NOMEM;
        }
        return KFS_OK;
    } else if (rc == SQLITE_DONE) {
        return KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_list_artifacts_next: sqlite3_step failed: %s\n", sqlite3_errmsg(sqlite3_db_handle(query_stmt)));
        return rc;
    }
}

/**
 * @brief Ends an artifact iteration, finalizes the statement, and releases resources.
 * MUST be called after finishing an iteration started with kfs_list_artifacts_begin.
 *
 * @param db GameDB handle.
 * @param query_stmt The iterator state handle to finalize.
 */
void kfs_list_artifacts_end(GameDB* db, sqlite3_stmt* query_stmt) {
    if (query_stmt) {
        sqlite3_finalize(query_stmt);
    }
    // Always try to end the transactions
    exec_sql(db->arch_db, "COMMIT;", "architecture");
    exec_sql(db->registry_db, "COMMIT;", "registry");
}

/**
 * @brief Lists all artifacts in a specified domain that the requesting actor has READ permission for.
 * This function uses an efficient iterator pattern internally to build the final result arrays,
 * avoiding the N+1 query problem and reducing memory churn.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain to query.
 * @param artifact_ids Output array of artifact IDs (caller must free).
 * @param artifact_names Output array of artifact names (caller must free each string).
 * @param artifact_types Output array of artifact types (caller must free each string).
 * @param artifact_count Output number of artifacts.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_list_artifacts(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int** artifact_ids, char*** artifact_names, char*** artifact_types, int* artifact_count) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || requesting_actor_uuid == 0 || domain_id <= 0 || !artifact_ids || !artifact_names || !artifact_types || !artifact_count) {
        fprintf(stderr, "[ERROR] kfs_list_artifacts: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id);
        return KFS_INVALID_ARGUMENT;
    }
    *artifact_ids = NULL; *artifact_names = NULL; *artifact_types = NULL; *artifact_count = 0;

    sqlite3_stmt* stmt = NULL;
    KFS_ArtifactInfo artifact_info;
    int rc = KFS_OK;

    int* temp_ids = NULL;
    char** temp_names = NULL;
    char** temp_types = NULL;
    int count = 0;
    int capacity = 16; // Initial allocation size

    // --- Begin the iteration ---
    rc = kfs_list_artifacts_begin(db, requesting_actor_uuid, domain_id, &stmt, &artifact_info);

    if (rc == KFS_NOTFOUND) {
        // No artifacts found, which is a valid result, not an error.
        return KFS_NOTFOUND;
    } else if (rc != KFS_OK) {
        // An actual error occurred during initialization.
        return rc;
    }

    // --- Allocate initial arrays ---
    temp_ids = KFS_MALLOC(capacity * sizeof(int));
    temp_names = KFS_MALLOC(capacity * sizeof(char*));
    temp_types = KFS_MALLOC(capacity * sizeof(char*));
    if (!temp_ids || !temp_names || !temp_types) {
        kfs_artifact_info_free_contents(&artifact_info); // Free the first fetched item
        kfs_list_artifacts_end(db, stmt);
        rc = KFS_NOMEM;
        goto cleanup;
    }

    // --- Loop through all accessible artifacts ---
    // A do-while loop is perfect because _begin already fetched the first item.
    do {
        // Check if reallocation is needed
        if (count >= capacity) {
            capacity *= 2;
            int* new_ids = KFS_REALLOC(temp_ids, capacity * sizeof(int));
            char** new_names = KFS_REALLOC(temp_names, capacity * sizeof(char*));
            char** new_types = KFS_REALLOC(temp_types, capacity * sizeof(char*));
            if (!new_ids || !new_names || !new_types) {
                kfs_mem_free(new_ids ? new_ids : temp_ids);
                kfs_mem_free(new_names ? new_names : temp_names);
                kfs_mem_free(new_types ? new_types : temp_types);
                temp_ids = NULL;
                temp_names = NULL;
                temp_types = NULL;
                rc = KFS_NOMEM;
                kfs_artifact_info_free_contents(&artifact_info); // Free the current item
                break; // Exit loop on memory failure
            }
            temp_ids = new_ids;
            temp_names = new_names;
            temp_types = new_types;
        }

        // Add the current artifact to our temporary arrays.
        // The strdup'd strings are now owned by our temp arrays.
        temp_ids[count] = artifact_info.id;
        temp_names[count] = artifact_info.name;
        temp_types[count] = artifact_info.type;
        count++;

    } while ((rc = kfs_list_artifacts_next(stmt, &artifact_info)) == KFS_OK);
    
    // --- Finalize the iteration ---
    kfs_list_artifacts_end(db, stmt);

    // After the loop, rc will be KFS_NOTFOUND if we reached the end, or an error code.
    if (rc != KFS_NOTFOUND && rc != KFS_OK) {
        // An error occurred during the _next call (e.g., KFS_NOMEM).
        goto cleanup;
    }

    // Success. Assign the populated arrays to the output parameters.
    *artifact_ids = temp_ids;
    *artifact_names = temp_names;
    *artifact_types = temp_types;
    *artifact_count = count;
    
    return KFS_OK;

cleanup:
    // This block is only reached on error, typically KFS_NOMEM.
    if (temp_ids) kfs_mem_free(temp_ids);
    if (temp_names) {
        for (int i = 0; i < count; i++) kfs_mem_free(temp_names[i]);
        kfs_mem_free(temp_names);
    }
    if (temp_types) {
        for (int i = 0; i < count; i++) kfs_mem_free(temp_types[i]);
        kfs_mem_free(temp_types);
    }
    *artifact_ids = NULL;
    *artifact_names = NULL;
    *artifact_types = NULL;
    *artifact_count = 0;
    return rc;
}

/* ============================================================================== */
/* ==                       ASSET MANAGEMENT FUNCTIONS                       == */
/* ============================================================================== */

/**
 * @brief Retrieves asset data for an artifact from a specified domain.
 * Requires READ permission on the artifact and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_actor_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain.
 * @param artifact_id ID of the artifact whose asset data is to be retrieved.
 * @param data Output parameter for binary data (caller must free, NULL if no data).
 * @param data_size Output parameter for size of binary data.
 * @param text_data Output parameter for text data (caller must free, NULL if no data).
 * @param metadata Output parameter for metadata (caller must free, NULL if no data).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_get_asset_data(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, int artifact_id, uint8_t** data, size_t* data_size, char** text_data, char** metadata) {
    // --- Input Validation ---
    if (!db || !db->arch_db || !db->registry_db || !db->artifacts_db || requesting_actor_uuid == 0 || domain_id <= 0 || artifact_id <= 0 ||
        !data || !data_size || !text_data || !metadata) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data: Invalid arguments (requesting_actor_uuid=%llu, domain_id=%d, artifact_id=%d).\n",
                (unsigned long long)requesting_actor_uuid, domain_id, artifact_id);
        return KFS_INVALID_ARGUMENT;
    }
    *data = NULL;
    *data_size = 0;
    *text_data = NULL;
    *metadata = NULL;

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK ||
        exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Get Requester Actor ID and Check Domain Access ---
    int requester_actor_id = -1;
    const char* sql_get_requester_id = "SELECT id, is_active FROM Actors WHERE uuid = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_get_requester_id, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (get requester id) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)requesting_actor_uuid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        requester_actor_id = sqlite3_column_int(stmt, 0);
        int is_active = sqlite3_column_int(stmt, 1);
        if (!is_active) {
            fprintf(stderr, "[ERROR] kfs_get_asset_data: Requester UUID %llu is inactive.\n",
                    (unsigned long long)requesting_actor_uuid);
            rc = KFS_PERMISSION_DENIED;
        } else {
            rc = KFS_OK;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data: Requester UUID %llu not found.\n",
                (unsigned long long)requesting_actor_uuid);
        rc = KFS_NOTFOUND;
    } else {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (get requester id) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != KFS_OK) {
        goto cleanup;
    }

    // Check domain access
    int has_domain_access = 0;
    const char* sql_check_domain = "SELECT 1 FROM DomainActors WHERE domain_id = ? AND actor_id = ? LIMIT 1;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check_domain, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (check domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, domain_id);
    sqlite3_bind_int(stmt, 2, requester_actor_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_domain_access = 1;
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (check domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (!has_domain_access) {
        const char* sql_check_group_domain = "SELECT DA.actor_id FROM DomainActors DA JOIN Actors A ON DA.actor_id = A.id "
                                            "WHERE DA.domain_id = ? AND A.actor_type IN ('GROUP', 'COMPANY');";
        rc = sqlite3_prepare_v2(db->registry_db, sql_check_group_domain, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[ERROR] kfs_get_asset_data (check group domain) - Prepare failed: %s\n", sqlite3_errmsg(db->registry_db));
            goto cleanup;
        }

        sqlite3_bind_int(stmt, 1, domain_id);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int group_id = sqlite3_column_int(stmt, 0);
            if (is_user_in_group(db, requester_actor_id, group_id)) {
                has_domain_access = 1;
                break;
            }
        }

        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] kfs_get_asset_data (check group domain) - Step failed: %s\n", sqlite3_errmsg(db->registry_db));
            sqlite3_finalize(stmt);
            goto cleanup;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!has_domain_access) {
            fprintf(stderr, "[ERROR] kfs_get_asset_data: Requester %llu lacks access to domain %d.\n",
                    (unsigned long long)requesting_actor_uuid, domain_id);
            rc = KFS_PERMISSION_DENIED;
            goto cleanup;
        }
    }

    // --- Check READ Permission on Artifact ---
    rc = kfs_check_permission(db, requesting_actor_uuid, "Artifact", artifact_id, KFS_PERM_READ);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_get_asset_data: Artifact ID %d not found.\n", artifact_id);
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_get_asset_data: Requester UUID %llu lacks READ permission for artifact %d.\n",
                    (unsigned long long)requesting_actor_uuid, artifact_id);
        }
        goto cleanup;
    }

    // --- Verify Artifact Exists in Domain ---
    const char* sql_check_artifact = "SELECT 1 FROM Artifacts WHERE id = ? AND domain_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_check_artifact, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (check artifact) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, domain_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data: Artifact ID %d not found in domain %d.\n", artifact_id, domain_id);
        rc = (rc == SQLITE_DONE) ? KFS_NOTFOUND : rc;
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Fetch Asset Data ---
    const char* sql_asset = "SELECT data, text_data, metadata FROM Assets WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql_asset, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (asset) - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        goto cleanup;
    }

    sqlite3_bind_int(stmt, 1, artifact_id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const void* asset_data = sqlite3_column_blob(stmt, 0);
        int asset_data_size = sqlite3_column_bytes(stmt, 0);
        const unsigned char* asset_text_data = sqlite3_column_text(stmt, 1);
        const unsigned char* asset_metadata = sqlite3_column_text(stmt, 2);

        if (asset_data && asset_data_size > 0) {
            *data = KFS_MALLOC(asset_data_size);
            if (!*data) {
                sqlite3_finalize(stmt);
                fprintf(stderr, "[ERROR] kfs_get_asset_data: Memory allocation failed for asset data.\n");
                rc = KFS_NOMEM;
                goto cleanup;
            }
            memcpy(*data, asset_data, asset_data_size);
            *data_size = asset_data_size;
        }

        *text_data = asset_text_data ? KFS_STRDUP((const char*)asset_text_data) : NULL;
        *metadata = asset_metadata ? KFS_STRDUP((const char*)asset_metadata) : NULL;

        if ((asset_text_data && !*text_data) || (asset_metadata && !*metadata)) {
            kfs_mem_free(*data);
            kfs_mem_free(*text_data);
            kfs_mem_free(*metadata);
            *data = NULL;
            *text_data = NULL;
            *metadata = NULL;
            sqlite3_finalize(stmt);
            fprintf(stderr, "[ERROR] kfs_get_asset_data: Memory allocation failed for asset text_data/metadata.\n");
            rc = KFS_NOMEM;
            goto cleanup;
        }
    } else if (rc == SQLITE_DONE) {
        fprintf(stderr, "[INFO] kfs_get_asset_data: No asset data found for artifact %d in domain %d.\n", artifact_id, domain_id);
        rc = KFS_NOTFOUND;
        sqlite3_finalize(stmt);
        goto cleanup;
    } else {
        fprintf(stderr, "[ERROR] kfs_get_asset_data (asset) - Step failed: %s\n", sqlite3_errmsg(db->artifacts_db));
        sqlite3_finalize(stmt);
        goto cleanup;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // --- Commit Transactions ---
    if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK ||
        exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_get_asset_data: Commit failed.\n");
        kfs_mem_free(*data);
        kfs_mem_free(*text_data);
        kfs_mem_free(*metadata);
        *data = NULL;
        *text_data = NULL;
        *metadata = NULL;
        goto cleanup;
    }

    fprintf(stdout, "[INFO] kfs_get_asset_data: Successfully retrieved asset data for artifact %d in domain %d.\n", artifact_id, domain_id);
    return KFS_OK;

cleanup:
    sqlite3_finalize(stmt);
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    return rc;
}

/**
 * @brief Internal helper to save artifact (REVISED for Actor Model).
 * Inserts metadata into architecture.db.Artifacts and data into artifacts.db.Assets.
 * Assumes caller manages transactions.
 *
 * @param db The GameDB handle.
 * @param type Artifact type.
 * @param name Artifact name.
 * @param format Artifact format.
 * @param creator_uuid KFS UUID of the creating actor.
 * @param owner_actor_id Internal ID of the owning actor.
 * @param security_scheme_id Optional security scheme ID (-1 for none).
 * @param data Pointer to BLOB data (or NULL).
 * @param data_size Size of BLOB data.
 * @param text_data Pointer to TEXT data (or NULL).
 * @param metadata Optional JSON metadata string.
 * @param artifact_id_out Output parameter for the new artifact ID.
 * @return KFS_OK on success, SQLite error code otherwise.
 */
static int kfs_save_asset(GameDB* db, const char* type, const char* name, const char* format,
                             uint64_t creator_uuid, int owner_actor_id, int security_scheme_id,
                             const void* data, size_t data_size, const char* text_data, const char* metadata,
                             int* artifact_id_out)
{
    int rc = KFS_OK;
    sqlite3_stmt* stmt_meta = NULL;
    sqlite3_stmt* stmt_data = NULL;
    char* timestamp = get_current_timestamp();
    int generated_id = -1;

    if (!timestamp) return KFS_NOMEM;
    // Basic validation
    if (creator_uuid == 0 || owner_actor_id <= 0) { kfs_mem_free(timestamp); return KFS_INVALID_ARGUMENT; }

    // Prepare insert for architecture.db.Artifacts
    const char* sql_meta = "INSERT INTO Artifacts (type, name, format, creator_uuid, owner_actor_id, security_scheme_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_meta, -1, &stmt_meta, NULL);
    if (rc != SQLITE_OK) { /* Handle error */ goto save_internal_cleanup; }

    // Prepare insert for artifacts.db.Assets (ID and data only)
    const char* sql_data = "INSERT INTO Assets (id, data, text_data, metadata) VALUES (?, ?, ?, ?);";
     rc = sqlite3_prepare_v2(db->artifacts_db, sql_data, -1, &stmt_data, NULL);
    if (rc != SQLITE_OK) { /* Handle error */ goto save_internal_cleanup; }

    // --- Execute within caller's transaction ---

    // 1. Insert metadata into architecture.db
    sqlite3_bind_text(stmt_meta, 1, type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_meta, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_meta, 3, format, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt_meta, 4, (sqlite3_int64)creator_uuid); // Bind creator UUID
    sqlite3_bind_int(stmt_meta, 5, owner_actor_id);                // Bind owner ID
    if (security_scheme_id > 0) sqlite3_bind_int(stmt_meta, 6, security_scheme_id);
    else sqlite3_bind_null(stmt_meta, 6);
    sqlite3_bind_text(stmt_meta, 7, timestamp, -1, SQLITE_STATIC); // created_at
    sqlite3_bind_text(stmt_meta, 8, timestamp, -1, SQLITE_STATIC); // updated_at

    rc = sqlite3_step(stmt_meta);
    if (rc != SQLITE_DONE) { /* Handle error, check constraints */ goto save_internal_cleanup; }
    generated_id = (int)sqlite3_last_insert_rowid(db->arch_db);
    sqlite3_finalize(stmt_meta); stmt_meta = NULL;

    // 2. Insert data into artifacts.db using the generated ID
    sqlite3_bind_int(stmt_data, 1, generated_id); // Use the ID from Artifacts insert
    // ... (bind data/text_data/metadata as before) ...
    if (data && data_size > 0) { sqlite3_bind_blob(stmt_data, 2, data, (int)data_size, SQLITE_TRANSIENT); sqlite3_bind_null(stmt_data, 3); }
    else if (text_data) { sqlite3_bind_null(stmt_data, 2); sqlite3_bind_text(stmt_data, 3, text_data, -1, SQLITE_TRANSIENT); }
    else { sqlite3_bind_null(stmt_data, 2); sqlite3_bind_null(stmt_data, 3); }
    sqlite3_bind_text(stmt_data, 4, metadata ? metadata : "{}", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt_data);
    if (rc != SQLITE_DONE) { /* Handle error */ goto save_internal_cleanup; }

    // Success
    *artifact_id_out = generated_id;
    rc = KFS_OK;

save_internal_cleanup:
    sqlite3_finalize(stmt_meta);
    sqlite3_finalize(stmt_data);
    kfs_mem_free(timestamp);
    return rc;
}

/**
 * @brief Deletes the asset associated with an artifact from artifacts.db.Assets.
 * Requires WRITE permission on the artifact. The artifact metadata remains intact.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user making the request.
 * @param artifact_id ID of the artifact whose asset is to be deleted.
 * @return KFS_OK on success (even if no asset was found), KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_INVALID_ARGUMENT, or SQLite error.
 */
int kfs_delete_asset(GameDB* db, uint64_t requesting_user_uuid, int artifact_id) {
    // --- Input Validation ---
    if (!db || !db->artifacts_db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || artifact_id <= 0) {
        fprintf(stderr, "[ERROR] kfs_delete_asset: Invalid arguments (artifact_id=%d).\n", artifact_id);
        return KFS_INVALID_ARGUMENT;
    }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    // Need all 3 DBs for permission check
     if (exec_sql(db->artifacts_db, "BEGIN IMMEDIATE;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_asset: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Artifact ---
    // This verifies the artifact exists and the user has rights to modify it (including its asset)
    rc = kfs_check_permission(db, requesting_user_uuid, "Artifact", artifact_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
         if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[ERROR] kfs_delete_asset: Artifact ID %d not found.\n", artifact_id);
        } else if (rc == KFS_PERMISSION_DENIED) {
            fprintf(stderr, "[ERROR] kfs_delete_asset: Permission denied for user %llu to modify/delete asset for artifact %d.\n",
                    (unsigned long long)requesting_user_uuid, artifact_id);
        }
        goto cleanup; // KFS_PERMISSION_DENIED, KFS_NOTFOUND, or DB error
    }

    // --- Delete from artifacts.db.Assets ---
    const char* sql = "DELETE FROM Assets WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->artifacts_db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, artifact_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
             fprintf(stderr, "[ERROR] kfs_delete_asset - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->artifacts_db), rc);
             goto cleanup;
        }
        if (sqlite3_changes(db->artifacts_db) == 0) {
             fprintf(stderr, "[INFO] kfs_delete_asset: No asset found for artifact %d, nothing to delete.\n", artifact_id);
        }
         rc = KFS_OK; // Reset rc
    } else { fprintf(stderr, "[ERROR] kfs_delete_asset - Prepare failed: %s\n", sqlite3_errmsg(db->artifacts_db)); goto cleanup; }


    // --- Commit Transactions ---
    if (exec_sql(db->artifacts_db, "COMMIT;", "artifacts") != KFS_OK ||
        exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_delete_asset: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

     if (rc == KFS_OK) { // Only log success if final rc is OK
        fprintf(stdout, "[INFO] kfs_delete_asset: Successfully processed delete for asset associated with artifact %d by user %llu.\n",
               artifact_id, (unsigned long long)requesting_user_uuid);
     }
    return rc;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    exec_sql(db->artifacts_db, "ROLLBACK;", "artifacts");
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc;
}

/* ============================================================================== */
/* ==                   ADVANCED LOADING FUNCTIONS                           == */
/* ============================================================================== */

/**
 * @brief Internal helper to load assets based on a list of artifact IDs retrieved from a prepared statement.
 * Calls kfs_load_artifact for each ID, which performs permission checks.
 *
 * @param db GameDB handle.
 * @param stmt_ids Prepared statement yielding artifact IDs in the first column.
 * @param requesting_user_uuid UUID of the user requesting the load.
 * @param results Output array of loaded KFS_Asset structs (caller must free with kfs_assets_free).
 * @param result_count Output number of successfully loaded assets.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_NOMEM, or SQLite error from underlying operations.
 */
static int kfs_load_asset_list(GameDB* db, sqlite3_stmt* stmt_ids, uint64_t requesting_user_uuid, KFS_Asset** results, int* result_count) {
     if (!db || !stmt_ids || !results || !result_count || requesting_user_uuid == 0) {
         return KFS_INVALID_ARGUMENT;
     }
     *results = NULL;
     *result_count = 0;

     KFS_Asset* temp_results = NULL;
     int count = 0;
     int capacity = 16; // Initial allocation size
     int rc_step;

     // Initial allocation
     temp_results = KFS_MALLOC(capacity * sizeof(KFS_Asset));
     if (!temp_results) { return KFS_NOMEM; }
     memset(temp_results, 0, capacity * sizeof(KFS_Asset)); // Zero out memory

     while ((rc_step = sqlite3_step(stmt_ids)) == SQLITE_ROW) {
         int artifact_id = sqlite3_column_int(stmt_ids, 0);

         // Check if reallocation is needed BEFORE loading the next asset
         if (count >= capacity) {
             capacity *= 2;
             KFS_Asset* new_results = KFS_REALLOC(temp_results, capacity * sizeof(KFS_Asset));
             if (!new_results) {
                 kfs_assets_free(temp_results, count); // Free already loaded assets
                 return KFS_NOMEM;
             }
             temp_results = new_results;
             // Zero out the newly allocated part
             memset(temp_results + count, 0, (capacity / 2) * sizeof(KFS_Asset));
         }

         // Pointer to the current slot (where the loaded asset will go)
         KFS_Asset* current_asset_ptr = &temp_results[count];

         // Load the artifact - kfs_load_artifact handles domain/permission checks
         // We need all output params for kfs_load_artifact signature
         char *type, *name, *format, *text_data, *metadata;
         uint64_t creator_uuid;
         int owner_actor_id, security_scheme_id;
         uint8_t* data;
         size_t data_size;
         char** topics; int topic_count;
         KFS_Note** notes; int note_count;

         // We need the domain_id for kfs_load_artifact. How to get it efficiently here?
         // Option 1: Add domain_id to the stmt_ids query (Best if possible)
         // Option 2: Query domain_id separately for each artifact_id (Inefficient)
         // Let's assume Option 1 is infeasible for this helper, and do Option 2.
         // This makes the helper less efficient but functional.
         int domain_id = -1;
         sqlite3_stmt* stmt_domain = NULL;
         int rc_domain = sqlite3_prepare_v2(db->arch_db, "SELECT domain_id FROM Artifacts WHERE id = ?", -1, &stmt_domain, NULL);
         if (rc_domain == SQLITE_OK) {
             sqlite3_bind_int(stmt_domain, 1, artifact_id);
             if (sqlite3_step(stmt_domain) == SQLITE_ROW) {
                 domain_id = sqlite3_column_int(stmt_domain, 0);
             }
             sqlite3_finalize(stmt_domain);
         } else {
              fprintf(stderr, "[ERROR] kfs_load_asset_list: Failed prepare getting domain for artifact %d (rc=%d).\n", artifact_id, rc_domain);
              continue; // Skip this artifact if domain lookup fails
         }

         if(domain_id <= 0) {
              fprintf(stderr, "[ERROR] kfs_load_asset_list: Could not find domain for artifact %d.\n", artifact_id);
              continue; // Skip artifact if domain not found
         }


         int load_rc = kfs_load_artifact(db, requesting_user_uuid, domain_id, artifact_id,
                                 &type, &name, &format, &creator_uuid, &owner_actor_id, &security_scheme_id,
                                 &data, &data_size, &text_data, &metadata,
                                 &topics, &topic_count, &notes, &note_count);

         if (load_rc == KFS_OK) {
             // Manually copy loaded data into the array slot
             // (kfs_load_artifact allocates, we need to manage that memory within KFS_Asset)
             current_asset_ptr->id = artifact_id;
             current_asset_ptr->type = type;
             current_asset_ptr->name = name;
             current_asset_ptr->format = format;
             current_asset_ptr->creator_uuid = creator_uuid;
             current_asset_ptr->owner_actor_id = owner_actor_id;
             current_asset_ptr->security_scheme_id = security_scheme_id;
             current_asset_ptr->data = data;
             current_asset_ptr->data_size = data_size;
             current_asset_ptr->text_data = text_data;
             current_asset_ptr->metadata = metadata;
             current_asset_ptr->topics = topics;
             current_asset_ptr->topic_count = topic_count;
             current_asset_ptr->notes = notes;
             current_asset_ptr->note_count = note_count;
             count++; // Successfully loaded and permission granted
         } else {
             // Free any memory allocated by kfs_load_artifact before skipping
             kfs_mem_free(type); kfs_mem_free(name); kfs_mem_free(format); kfs_mem_free(data); kfs_mem_free(text_data); kfs_mem_free(metadata);
             for (int i = 0; i < topic_count; i++) kfs_mem_free(topics[i]); kfs_mem_free(topics);
             for (int i = 0; i < note_count; i++) kfs_note_free(notes[i]); kfs_mem_free(notes);

             if (load_rc == KFS_PERMISSION_DENIED || load_rc == KFS_NOTFOUND) {
                 fprintf(stderr, "[INFO] kfs_load_asset_list: Skipping artifact %d (rc=%d).\n", artifact_id, load_rc);
                 // Slot in temp_results remains unused and zeroed out.
             } else {
                 // Actual error occurred during load
                 fprintf(stderr, "[ERROR] kfs_load_asset_list: Failed to load artifact %d (rc=%d).\n", artifact_id, load_rc);
                 kfs_assets_free(temp_results, count); // Free successfully loaded assets
                 return load_rc; // Propagate the error
             }
         }
     } // end while loop

     // Check final status of sqlite3_step
     if (rc_step != SQLITE_DONE) {
         fprintf(stderr, "[ERROR] kfs_load_asset_list: Error stepping through artifact IDs: %s\n", sqlite3_errmsg(sqlite3_db_handle(stmt_ids)));
         kfs_assets_free(temp_results, count);
         return rc_step; // Return the SQLite error
     }

     // Shrink array if needed (optional optimization)
     if (count > 0 && count < capacity) {
         KFS_Asset* final_results = KFS_REALLOC(temp_results, count * sizeof(KFS_Asset));
         if (final_results) { // Keep original block if realloc fails
             temp_results = final_results;
         } // else: Keep the larger block, it's still valid
     } else if (count == 0) {
         kfs_mem_free(temp_results); // Free initial allocation if nothing was loaded
         temp_results = NULL;
     }


     *results = temp_results;
     *result_count = count;
     return KFS_OK;
}

/**
 * @brief Retrieves a topic by its name within a specified domain.
 * Requires READ permission on the topic and domain access.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id ID of the domain where the topic resides.
 * @param name The name of the topic to retrieve.
 * @param topic Output parameter struct KFS_Topic to be filled (caller must free contents).
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_get_topic_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* name, KFS_Topic* topic) {
    // Input validation
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !name || strlen(name) == 0 || !topic) {
         fprintf(stderr, "[ERROR] kfs_get_topic_by_name: Invalid arguments.\n");
        return KFS_INVALID_ARGUMENT;
    }

    int topic_id = -1;
    int rc = KFS_OK;

    // Find the topic ID using the domain-scoped helper
    rc = kfs_get_topic_id_by_name(db, domain_id, name, &topic_id);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
             fprintf(stderr, "[INFO] kfs_get_topic_by_name: Topic '%s' not found in domain %d.\n", name, domain_id);
        } else {
             fprintf(stderr, "[ERROR] kfs_get_topic_by_name: Error finding topic ID for '%s' (rc=%d).\n", name, rc);
        }
        return rc; // KFS_NOTFOUND or DB error
    }

    // Correctly call the ID-based get function, which handles permission checks.
    // kfs_get_topic populates the members of the struct you provide.
    int get_rc = kfs_get_topic(db, requesting_user_uuid, domain_id, topic_id,
                               &topic->owner_actor_id, &topic->name, &topic->security_scheme_id,
                               &topic->creator_uuid, &topic->created_at, &topic->updated_at);

    if (get_rc == KFS_OK) {
        // The get function succeeded, so we also need to set the IDs we already know.
        topic->id = topic_id;
        topic->domain_id = domain_id;
    }

    // Note: kfs_get_topic doesn't fill the related arrays (epics, related_topics, notes).
    // This is expected behavior for a "get" function. A "load" function would be more comprehensive.
    return get_rc;
}

/**
 * @brief Internal helper: Gets the ID of a topic by its name within a specific domain.
 * Topic names must be unique within a domain based on the schema.
 *
 * @param db GameDB handle.
 * @param domain_id ID of the domain to search within.
 * @param name The name of the topic.
 * @param topic_id Output parameter for the topic's ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_NOTFOUND, or SQLite error.
 */
static int kfs_get_topic_id_by_name(GameDB* db, int domain_id, const char* name, int* topic_id) {
     // Input Validation
     if (!db || !db->arch_db || domain_id <= 0 || !name || strlen(name) == 0 || !topic_id) {
         if(topic_id) *topic_id = -1;
         return KFS_INVALID_ARGUMENT;
     }
     *topic_id = -1; // Initialize output

     // Query using both name and domain_id
     const char* sql = "SELECT id FROM Topics WHERE name = ? AND domain_id = ?;";
     sqlite3_stmt* stmt = NULL;
     int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);

     if (rc != SQLITE_OK) {
         fprintf(stderr, "[ERROR] kfs_get_topic_id_by_name - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
         sqlite3_finalize(stmt);
         return rc;
     }

     sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
     sqlite3_bind_int(stmt, 2, domain_id);

     rc = sqlite3_step(stmt);
     if (rc == SQLITE_ROW) {
         *topic_id = sqlite3_column_int(stmt, 0);
         rc = KFS_OK; // Found
     } else if (rc == SQLITE_DONE) {
         // Topic not found in this domain
         rc = KFS_NOTFOUND;
     } else {
         // DB error during step
         fprintf(stderr, "[ERROR] kfs_get_topic_id_by_name - Step failed: %s\n", sqlite3_errmsg(db->arch_db));
         // rc holds the error code
     }

     sqlite3_finalize(stmt);
     return rc;
}

/**
 * @brief Links a topic to an artifact by name without permission checks.
 * Used by legacy save helpers that already hold arch_db transactions.
 * @param domain_id Domain for topic lookup; if <= 0, resolves topic name globally (legacy).
 */
static int kfs_link_topic_to_artifact_by_name_internal(GameDB* db, int domain_id, int artifact_id, const char* topic_name) {
    if (!db || !db->arch_db || artifact_id <= 0 || !topic_name || strlen(topic_name) == 0) {
        return KFS_INVALID_ARGUMENT;
    }

    int topic_id = -1;
    int rc = KFS_OK;

    if (domain_id > 0) {
        rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &topic_id);
    } else {
        const char* sql = "SELECT id FROM Topics WHERE name = ? LIMIT 1;";
        sqlite3_stmt* lookup = NULL;
        rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &lookup, NULL);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(lookup);
            return rc;
        }
        sqlite3_bind_text(lookup, 1, topic_name, -1, SQLITE_STATIC);
        rc = sqlite3_step(lookup);
        if (rc == SQLITE_ROW) {
            topic_id = sqlite3_column_int(lookup, 0);
            rc = KFS_OK;
        } else if (rc == SQLITE_DONE) {
            rc = KFS_NOTFOUND;
        }
        sqlite3_finalize(lookup);
    }
    if (rc != KFS_OK) return rc;

    sqlite3_stmt* stmt = NULL;
    const char* sql_insert = "INSERT OR IGNORE INTO TopicAssignments (artifact_id, topic_id) VALUES (?, ?);";
    rc = sqlite3_prepare_v2(db->arch_db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_bind_int(stmt, 1, artifact_id);
    sqlite3_bind_int(stmt, 2, topic_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return (sqlite3_errcode(db->arch_db) == SQLITE_CONSTRAINT) ? KFS_CONSTRAINT : KFS_ERROR;
    }
    return KFS_OK;
}

/* ============================================================================== */
/* ==                   ADVANCED LOADING FUNCTIONS                           == */
/* ============================================================================== */

/**
 * @brief Loads all artifacts associated with a given topic name that the user can READ.
 * Allocates memory for the results array and internal structs/strings.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param topic_name Name of the topic.
 * @param results Output array of KFS_Asset structs (caller must free with kfs_assets_free).
 * @param result_count Output number of assets found.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_load_by_topic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, KFS_Asset** results, int* result_count) {
     // Input Validation
    if (!db || !db->arch_db || !db->artifacts_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !topic_name || !results || !result_count) {
        return KFS_INVALID_ARGUMENT;
    }
    *results = NULL; *result_count = 0;

    int rc = KFS_OK;
    int topic_id = -1;
    sqlite3_stmt* stmt_ids = NULL;

    // Get Topic ID
    rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &topic_id);
    if (rc != KFS_OK) return rc;

    // Permission Check: READ on the Topic itself
    rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_READ);
    if (rc != KFS_OK) return rc;

    // Query artifact IDs associated with this topic
    const char* sql_ids = "SELECT artifact_id FROM TopicAssignments WHERE topic_id = ? ORDER BY artifact_id;";

    rc = sqlite3_prepare_v2(db->arch_db, sql_ids, -1, &stmt_ids, NULL);
    if (rc != KFS_OK) { return rc; }
    sqlite3_bind_int(stmt_ids, 1, topic_id);

    // Use the updated helper
    rc = kfs_load_asset_list(db, stmt_ids, requesting_user_uuid, results, result_count);

    sqlite3_finalize(stmt_ids);
    return rc;
}

/**
 * @brief Internal helper: Gets the ID of an epic by its name within a specific domain.
 *
 * @param db GameDB handle.
 * @param domain_id ID of the domain to search within.
 * @param name The name of the epic.
 * @param epic_id Output parameter for the epic's ID.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_NOTFOUND, or SQLite error.
 */
static int kfs_get_epic_id_by_name(GameDB* db, int domain_id, const char* name, int* epic_id) {
     // Input Validation
     if (!db || !db->arch_db || domain_id <= 0 || !name || strlen(name) == 0 || !epic_id) {
         // Don't log error here as it's an internal helper, let caller handle bad args
         if(epic_id) *epic_id = -1;
         return KFS_INVALID_ARGUMENT;
     }
     *epic_id = -1; // Initialize output

     const char* sql = "SELECT id FROM Epics WHERE name = ? AND domain_id = ?;";
     sqlite3_stmt* stmt = NULL;
     int rc = sqlite3_prepare_v2(db->arch_db, sql, -1, &stmt, NULL);

     if (rc != SQLITE_OK) {
         fprintf(stderr, "[ERROR] kfs_get_epic_id_by_name - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
         sqlite3_finalize(stmt);
         return rc;
     }

     sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
     sqlite3_bind_int(stmt, 2, domain_id);

     rc = sqlite3_step(stmt);
     if (rc == SQLITE_ROW) {
         *epic_id = sqlite3_column_int(stmt, 0);
         rc = KFS_OK; // Found
     } else if (rc == SQLITE_DONE) {
         // Epic not found in this domain
         rc = KFS_NOTFOUND;
     } else {
         // DB error during step
         fprintf(stderr, "[ERROR] kfs_get_epic_id_by_name - Step failed: %s\n", sqlite3_errmsg(db->arch_db));
         // rc holds the error code
     }

     sqlite3_finalize(stmt);
     return rc;
}

/**
 * @brief Assigns an existing epic to an existing topic using their names within a specific domain.
 * Requires WRITE permission on the Topic.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param domain_id The ID of the domain where both the topic and epic reside.
 * @param topic_name The name of the topic.
 * @param epic_name The name of the epic to assign.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND, or SQLite error.
 */
int kfs_assign_epic_to_topic_by_name(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, const char* epic_name) {
     // Input Validation
     if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !topic_name || !epic_name) {
         fprintf(stderr, "[ERROR] kfs_assign_epic_to_topic_by_name: Invalid arguments.\n");
         return KFS_INVALID_ARGUMENT;
     }
     if (strcmp(topic_name, epic_name) == 0) { // Should compare names if needed, but IDs are better
        // Note: This check might be misleading if a topic and epic can have the same name
        // It's better to rely on the ID check later if needed. Let's remove this check.
     }

     int topic_id = -1, epic_id = -1, rc = KFS_OK;

     // Find Topic ID within the specified domain
     rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &topic_id);
     if (rc != KFS_OK) {
         fprintf(stderr, "[ERROR] kfs_assign_epic_to_topic_by_name: Failed to find topic '%s' in domain %d (rc=%d).\n", topic_name, domain_id, rc);
         return rc; // KFS_NOTFOUND or DB error
     }

     // Find Epic ID within the specified domain
     rc = kfs_get_epic_id_by_name(db, domain_id, epic_name, &epic_id);
      if (rc != KFS_OK) {
         fprintf(stderr, "[ERROR] kfs_assign_epic_to_topic_by_name: Failed to find epic '%s' in domain %d (rc=%d).\n", epic_name, domain_id, rc);
         return rc; // KFS_NOTFOUND or DB error
     }

     // Call the ID-based assignment function which includes permission checks
     // kfs_assign_epic_to_topic should verify domain consistency again internally for safety.
     return kfs_assign_epic_to_topic(db, requesting_user_uuid, topic_id, epic_id);
}

/**
 * @brief Removes the assignment link between a specific topic and a specific epic.
 * Requires WRITE permission on the Topic.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param topic_id ID of the topic from which the epic link will be removed.
 * @param epic_id ID of the epic to unassign.
 * @return KFS_OK on success (even if the link didn't exist), KFS_INVALID_ARGUMENT,
 *         KFS_PERMISSION_DENIED, KFS_NOTFOUND (if permission check fails), or SQLite error.
 */
int kfs_remove_epic_from_topic(GameDB* db, uint64_t requesting_user_uuid, int topic_id, int epic_id) {
     // Input Validation
     if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || topic_id <= 0 || epic_id <= 0) {
         fprintf(stderr, "[ERROR] kfs_remove_epic_from_topic: Invalid arguments (topic_id=%d, epic_id=%d).\n", topic_id, epic_id);
         return KFS_INVALID_ARGUMENT;
     }

    int rc = KFS_OK;
    sqlite3_stmt* stmt = NULL;

    // --- Begin Transactions ---
    // Need registry for permission checks
     if (exec_sql(db->arch_db, "BEGIN IMMEDIATE;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "BEGIN IMMEDIATE;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_remove_epic_from_topic: Failed to begin transaction.\n");
        return KFS_ERROR;
    }

    // --- Permission Check: WRITE on the Topic ---
    // This verifies the topic exists and the user has rights to modify its links.
    rc = kfs_check_permission(db, requesting_user_uuid, "Topic", topic_id, KFS_PERM_WRITE);
    if (rc != KFS_OK) {
        if (rc == KFS_NOTFOUND) {
            fprintf(stderr, "[INFO] kfs_remove_epic_from_topic: Topic ID %d not found or permission check failed with NOTFOUND, treating as success for removal.\n", topic_id);
            rc = KFS_OK; // Not found is OK for remove
            goto commit; // Skip actual deletion
        }
        fprintf(stderr, "[ERROR] kfs_remove_epic_from_topic: Permission check failed for topic %d (rc=%d).\n", topic_id, rc);
        goto cleanup; // Permission denied or DB error
    }

    // --- Proceed with Removal from EpicAssignments ---
    const char* sql_delete = "DELETE FROM EpicAssignments WHERE topic_id = ? AND epic_id = ?;";
    rc = sqlite3_prepare_v2(db->arch_db, sql_delete, -1, &stmt, NULL);
     if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, topic_id);
        sqlite3_bind_int(stmt, 2, epic_id);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); stmt = NULL;
        if (rc != SQLITE_DONE) {
             fprintf(stderr, "[ERROR] kfs_remove_epic_from_topic (delete) - Execute failed: %s (rc=%d)\n", sqlite3_errmsg(db->arch_db), rc);
             goto cleanup;
        }
        if (sqlite3_changes(db->arch_db) == 0) {
            fprintf(stdout, "[INFO] kfs_remove_epic_from_topic: Link between topic %d and epic %d not found.\n", topic_id, epic_id);
        }
        rc = KFS_OK; // Reset rc, not finding is OK for remove
    } else { fprintf(stderr, "[ERROR] kfs_remove_epic_from_topic (delete) - Prepare failed: %s\n", sqlite3_errmsg(db->arch_db)); goto cleanup; }


commit:
    // --- Commit Transactions ---
     if (exec_sql(db->arch_db, "COMMIT;", "architecture") != KFS_OK ||
        exec_sql(db->registry_db, "COMMIT;", "registry") != KFS_OK) {
        fprintf(stderr, "[ERROR] kfs_remove_epic_from_topic: Commit failed.\n");
        rc = KFS_ERROR;
        goto cleanup; // Attempt rollback
    }

     if (rc == KFS_OK) { // Only log success if final rc is OK
        fprintf(stdout, "[INFO] kfs_remove_epic_from_topic: Successfully processed removal of epic %d from topic %d by user %llu.\n",
               epic_id, topic_id, (unsigned long long)requesting_user_uuid);
     }
    return rc;

cleanup:
    sqlite3_finalize(stmt); // Ensure stmt finalized
    // Rollback
    exec_sql(db->arch_db, "ROLLBACK;", "architecture");
    exec_sql(db->registry_db, "ROLLBACK;", "registry");
    return rc; // Return the specific error code encountered
}

/**
 * @brief Loads all artifacts associated with a given epic name (via topics) that the user can READ.
 * Allocates memory for the results array and internal structs/strings.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param epic_name Name of the epic.
 * @param results Output array of KFS_Asset structs (caller must free with kfs_assets_free).
 * @param result_count Output number of assets found.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_load_by_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* epic_name, KFS_Asset** results, int* result_count) {
    // Input Validation
    if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !epic_name || !results || !result_count) {
         return KFS_INVALID_ARGUMENT;
    }
    *results = NULL; *result_count = 0;

    int rc = KFS_OK;
    int epic_id = -1;
    sqlite3_stmt* stmt_ids = NULL;

    // Get Epic ID
    rc = kfs_get_epic_id_by_name(db, domain_id, epic_name, &epic_id);
    if (rc != KFS_OK) return rc; // KFS_NOTFOUND or DB error

    // Permission Check: READ on the Epic itself
    // kfs_check_permission handles domain access check implicitly
    rc = kfs_check_permission(db, requesting_user_uuid, "Epic", epic_id, KFS_PERM_READ);
    if (rc != KFS_OK) return rc; // KFS_PERMISSION_DENIED or error

    // Query artifact IDs linked via topics associated with this epic
    const char* sql_ids = "SELECT DISTINCT TA.artifact_id "
                          "FROM EpicAssignments EA "
                          "JOIN TopicAssignments TA ON EA.topic_id = TA.topic_id "
                          "WHERE EA.epic_id = ? ORDER BY TA.artifact_id;";

    rc = sqlite3_prepare_v2(db->arch_db, sql_ids, -1, &stmt_ids, NULL);
    if (rc != KFS_OK) { return rc; }
    sqlite3_bind_int(stmt_ids, 1, epic_id);

    // Use the updated helper which now takes the user UUID
    rc = kfs_load_asset_list(db, stmt_ids, requesting_user_uuid, results, result_count);

    sqlite3_finalize(stmt_ids); // Finalize statement regardless of helper result
    return rc; // Return result from kfs_load_asset_list
}


/**
 * @brief Handles artifacts potentially orphaned by user deactivation.
 * Sets the security_scheme_id to NULL in the architecture.db.Artifacts table
 * for all artifacts owned by the specified *inactive* actor ID.
 *
 * @param db GameDB handle.
 * @param deactivated_actor_id The internal Actor ID of the user who was deactivated.
 * @return KFS_OK on success, KFS_NOTFOUND if user not found/already active, or DB error code.
 */
int kfs_handle_orphaned_artifacts(GameDB* db, int deactivated_actor_id) {
     if (!db || !db->arch_db || !db->registry_db || deactivated_actor_id <= 0) {
        return KFS_INVALID_ARGUMENT;
    }

    sqlite3_stmt* stmt = NULL;
    int rc = KFS_OK;

    // 1. Verify the user is actually inactive in registry.db
    const char* sql_check = "SELECT is_active FROM Actors WHERE id = ?;";
    rc = sqlite3_prepare_v2(db->registry_db, sql_check, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { /* Handle error */ sqlite3_finalize(stmt); return rc; }
    sqlite3_bind_int(stmt, 1, deactivated_actor_id);
    rc = sqlite3_step(stmt);
    int is_active = 1;
    if (rc == SQLITE_ROW) { is_active = sqlite3_column_int(stmt, 0); rc = KFS_OK; }
    else if (rc == SQLITE_DONE) { rc = KFS_NOTFOUND; }
    sqlite3_finalize(stmt); stmt = NULL;

    if (rc == KFS_NOTFOUND) {
         fprintf(stderr, "[WARN] kfs_handle_orphaned_artifacts: Actor ID %d not found.\n", deactivated_actor_id);
         return KFS_NOTFOUND;
    }
    if (rc != KFS_OK) return rc; // DB error during check

    if (is_active) {
        fprintf(stderr, "[WARN] kfs_handle_orphaned_artifacts: Called for actor %d who is still active. No action taken.\n", deactivated_actor_id);
        return KFS_OK;
    }

    // --- Proceed with Update in architecture.db ---
    // Update the Artifacts table directly
    const char* sql_update = "UPDATE Artifacts SET security_scheme_id = NULL, updated_at = ? WHERE owner_actor_id = ?;";
    char* timestamp = get_current_timestamp(); // Update timestamp as well
    if (!timestamp) return KFS_NOMEM;

    rc = sqlite3_prepare_v2(db->arch_db, sql_update, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[ERROR] kfs_handle_orphaned: Update prepare failed: %s\n", sqlite3_errmsg(db->arch_db));
        kfs_mem_free(timestamp); sqlite3_finalize(stmt); return rc;
    }
    sqlite3_bind_text(stmt, 1, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, deactivated_actor_id);
    rc = sqlite3_step(stmt);
    kfs_mem_free(timestamp);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] kfs_handle_orphaned_artifacts - Update failed: %s\n", sqlite3_errmsg(db->arch_db));
        return rc;
    }

    int changes = sqlite3_changes(db->arch_db);
    if (changes > 0) {
        fprintf(stdout, "[INFO] kfs_handle_orphaned_artifacts: Cleared security schemes for %d artifacts owned by deactivated actor %d.\n", changes, deactivated_actor_id);
    } else {
        fprintf(stdout, "[INFO] kfs_handle_orphaned_artifacts: No artifacts found owned by deactivated actor %d.\n", deactivated_actor_id);
    }
    return KFS_OK;
}

/**
 * @brief Loads script artifacts associated with a given epic name that the user can READ.
 * Optionally filters by script format.
 * Allocates memory for the results array and internal structs/strings.
 *
 * @param db GameDB handle.
 * @param requesting_user_uuid UUID of the user requesting the action.
 * @param epic_name Name of the epic.
 * @param format Optional format filter (e.g., "python", NULL for any script).
 * @param results Output array of KFS_Asset structs (caller must free with kfs_assets_free).
 * @param result_count Output number of assets found.
 * @return KFS_OK on success, KFS_INVALID_ARGUMENT, KFS_PERMISSION_DENIED, KFS_NOTFOUND,
 *         KFS_NOMEM, or SQLite error.
 */
int kfs_load_scripts_by_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* epic_name, const char* format, KFS_Asset** results, int* result_count) {
    // Input Validation
     if (!db || !db->arch_db || !db->artifacts_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !epic_name || !results || !result_count) {
         return KFS_INVALID_ARGUMENT;
     }
    *results = NULL; *result_count = 0;

    int rc = KFS_OK;
    int epic_id = -1;
    sqlite3_stmt* stmt_ids = NULL;
    char sql_ids_buffer[512];
    int bind_count = 1;

    // Get Epic ID
    rc = kfs_get_epic_id_by_name(db, domain_id, epic_name, &epic_id);
    if (rc != KFS_OK) return rc;

    // Permission Check: READ on the Epic itself
    rc = kfs_check_permission(db, requesting_user_uuid, "Epic", epic_id, KFS_PERM_READ);
    if (rc != KFS_OK) return rc;

    // Build SQL query for artifact IDs
    const char* base_sql = "SELECT DISTINCT A.id "
                           "FROM EpicAssignments EA "
                           "JOIN Topics T ON EA.topic_id = T.id "
                           "JOIN TopicAssignments TA ON T.id = TA.topic_id "
                           "JOIN Artifacts A ON TA.artifact_id = A.id "
                           "WHERE EA.epic_id = ? AND A.type = 'script' "; // Filter by type='script'

    if (format && strlen(format) > 0) {
        snprintf(sql_ids_buffer, sizeof(sql_ids_buffer), "%s AND A.format = ? ORDER BY A.id;", base_sql);
        bind_count = 2;
    } else {
        snprintf(sql_ids_buffer, sizeof(sql_ids_buffer), "%s ORDER BY A.id;", base_sql);
    }

    rc = sqlite3_prepare_v2(db->arch_db, sql_ids_buffer, -1, &stmt_ids, NULL);
    if (rc != KFS_OK) { return rc; }

    sqlite3_bind_int(stmt_ids, 1, epic_id);
    if (bind_count == 2) sqlite3_bind_text(stmt_ids, 2, format, -1, SQLITE_STATIC);

    // Use the updated helper
    rc = kfs_load_asset_list(db, stmt_ids, requesting_user_uuid, results, result_count);

    sqlite3_finalize(stmt_ids);
    return rc;
}

/* ============================================================================== */
/* ==                      OTHER MISC FUNCTIONS                              == */
/* ============================================================================== */

/**
 * @brief Validates a script based on its format (e.g., Python, JavaScript, Lua).
 * Checks for basic syntax and structural correctness without executing the script.
 * Allocates an error message string if validation fails, which the caller must free.
 *
 * @param format The script format (e.g., "python", "javascript", "lua").
 * @param script_code The script content to validate.
 * @param error_msg Output parameter for error message (NULL if valid or no error; caller must free if set).
 * @return KFS_OK if the script is valid, KFS_INVALID_ARGUMENT for invalid inputs,
 *         KFS_VALIDATION_FAILED if validation fails, KFS_NOMEM for memory allocation failure.
 */
int kfs_validate_script(const char* format, const char* script_code, char** error_msg) {
    // Initialize output
    if (error_msg) *error_msg = NULL;

    // --- Input Validation ---
    if (!format || !script_code || strlen(format) == 0 || strlen(script_code) == 0) {
        fprintf(stderr, "[ERROR] kfs_validate_script: Invalid arguments (format=%s, script_code=%p).\n",
                format ? format : "NULL", (void*)script_code);
        return KFS_INVALID_ARGUMENT;
    }

    // Trim format to lowercase for case-insensitive comparison
    char format_lower[32];
    strncpy(format_lower, format, sizeof(format_lower) - 1);
    format_lower[sizeof(format_lower) - 1] = '\0';
    for (size_t i = 0; format_lower[i]; i++) {
        format_lower[i] = tolower(format_lower[i]);
    }

    // Helper function to set error message
    int set_error(const char* msg, char** out) {
        if (out) {
            *out = KFS_STRDUP(msg);
            if (!*out) {
                fprintf(stderr, "[ERROR] kfs_validate_script: Memory allocation failed for error message.\n");
                return KFS_NOMEM;
            }
        }
        fprintf(stderr, "[ERROR] kfs_validate_script: Validation failed: %s\n", msg);
        return KFS_VALIDATION_FAILED;
    }

    // --- Python Validation ---
    if (strcmp(format_lower, "python") == 0) {
        // Basic checks: indentation consistency, basic syntax (e.g., colons after blocks)
        int indent_level = 0;
        int line_number = 1;
        const char* ptr = script_code;
        int in_string = 0;
        char string_char = 0;

        while (*ptr) {
            // Skip whitespace at start of line
            while (*ptr == ' ' || *ptr == '\t') ptr++;

            // Count indentation (spaces only for simplicity)
            int current_indent = 0;
            while (ptr[current_indent] == ' ') current_indent++;
            ptr += current_indent;

            // Skip empty lines or comments
            if (*ptr == '\n') {
                line_number++;
                ptr++;
                continue;
            }
            if (*ptr == '#') {
                while (*ptr && *ptr != '\n') ptr++;
                if (*ptr == '\n') {
                    line_number++;
                    ptr++;
                }
                continue;
            }

            // Check string literals to avoid parsing inside strings
            if (*ptr == '"' || *ptr == '\'') {
                if (in_string && *ptr == string_char) {
                    in_string = 0; // End of string
                } else if (!in_string) {
                    in_string = 1;
                    string_char = *ptr;
                }
                ptr++;
                continue;
            }

            if (!in_string) {
                // Check for block statements (e.g., def, if, for) expecting a colon
                if (strncmp(ptr, "def ", 4) == 0 || strncmp(ptr, "if ", 3) == 0 ||
                    strncmp(ptr, "for ", 4) == 0 || strncmp(ptr, "while ", 6) == 0) {
                    // Find the colon
                    const char* colon = ptr;
                    while (*colon && *colon != '\n' && *colon != ':') colon++;
                    if (*colon != ':') {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Missing colon in block statement at line %d.", line_number);
                        return set_error(msg, error_msg);
                    }
                    // Update indent expectation for next line
                    indent_level = current_indent + 4; // Assume 4-space indent
                }

                // Check indentation consistency
                if (current_indent > indent_level) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Unexpected indentation at line %d.", line_number);
                    return set_error(msg, error_msg);
                } else if (current_indent < indent_level && current_indent != 0) {
                    indent_level = current_indent; // Dedent
                }
            }

            // Move to next line
            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr == '\n') {
                line_number++;
                ptr++;
            }
        }

        if (in_string) {
            return set_error("Unterminated string literal.", error_msg);
        }

        fprintf(stdout, "[INFO] kfs_validate_script: Python script validated successfully.\n");
        return KFS_OK;
    }

    // --- JavaScript Validation ---
    else if (strcmp(format_lower, "javascript") == 0) {
        // Basic checks: brace matching, reserved keyword misuse
        int brace_count = 0; // For {}
        int paren_count = 0; // For ()
        int bracket_count = 0; // For []
        int in_string = 0;
        char string_char = 0;
        int line_number = 1;
        const char* ptr = script_code;

        while (*ptr) {
            // Handle strings
            if (*ptr == '"' || *ptr == '\'') {
                if (in_string && *ptr == string_char) {
                    in_string = 0;
                } else if (!in_string) {
                    in_string = 1;
                    string_char = *ptr;
                }
                ptr++;
                continue;
            }

            // Skip escaped characters
            if (*ptr == '\\' && *(ptr + 1)) {
                ptr += 2;
                continue;
            }

            if (!in_string) {
                // Check brace/paren/bracket matching
                if (*ptr == '{') brace_count++;
                else if (*ptr == '}') brace_count--;
                else if (*ptr == '(') paren_count++;
                else if (*ptr == ')') paren_count--;
                else if (*ptr == '[') bracket_count++;
                else if (*ptr == ']') bracket_count--;

                // Check for negative counts (closing before opening)
                if (brace_count < 0 || paren_count < 0 || bracket_count < 0) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Mismatched closing delimiter at line %d.", line_number);
                    return set_error(msg, error_msg);
                }
            }

            if (*ptr == '\n') line_number++;
            ptr++;
        }

        if (in_string) {
            return set_error("Unterminated string literal.", error_msg);
        }
        if (brace_count != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Unmatched braces (%d open).", brace_count);
            return set_error(msg, error_msg);
        }
        if (paren_count != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Unmatched parentheses (%d open).", paren_count);
            return set_error(msg, error_msg);
        }
        if (bracket_count != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Unmatched brackets (%d open).", bracket_count);
            return set_error(msg, error_msg);
        }

        // Basic reserved keyword check (e.g., 'function' must be followed by identifier or paren)
        ptr = script_code;
        while (*ptr) {
            if (strncmp(ptr, "function", 8) == 0 && !isspace(ptr[8]) && ptr[8] != '(') {
                return set_error("Invalid use of 'function' keyword (must be followed by space or parenthesis).", error_msg);
            }
            ptr++;
        }

        fprintf(stdout, "[INFO] kfs_validate_script: JavaScript script validated successfully.\n");
        return KFS_OK;
    }

    // --- Lua Validation ---
    else if (strcmp(format_lower, "lua") == 0) {
        // Basic checks: block structure (do/end, if/end), brace matching
        int block_count = 0; // For do/end, if/end, etc.
        int in_string = 0;
        char string_char = 0;
        int line_number = 1;
        const char* ptr = script_code;

        while (*ptr) {
            // Handle strings
            if (*ptr == '"' || *ptr == '\'') {
                if (in_string && *ptr == string_char) {
                    in_string = 0;
                } else if (!in_string) {
                    in_string = 1;
                    string_char = *ptr;
                }
                ptr++;
                continue;
            }

            // Skip escaped characters
            if (*ptr == '\\' && *(ptr + 1)) {
                ptr += 2;
                continue;
            }

            if (!in_string) {
                // Check block starters (do, if, function)
                if (strncmp(ptr, "do", 2) == 0 && isspace(ptr[2])) {
                    block_count++;
                    ptr += 2;
                } else if (strncmp(ptr, "if ", 3) == 0) {
                    block_count++;
                    ptr += 3;
                } else if (strncmp(ptr, "function ", 9) == 0) {
                    block_count++;
                    ptr += 9;
                }
                // Check block ender (end)
                else if (strncmp(ptr, "end", 3) == 0 && (isspace(ptr[3]) || ptr[3] == '\0' || ptr[3] == '\n')) {
                    block_count--;
                    ptr += 3;
                    if (block_count < 0) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Mismatched 'end' at line %d.", line_number);
                        return set_error(msg, error_msg);
                    }
                }
            }

            if (*ptr == '\n') line_number++;
            ptr++;
        }

        if (in_string) {
            return set_error("Unterminated string literal.", error_msg);
        }
        if (block_count != 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Unmatched blocks (%d open).", block_count);
            return set_error(msg, error_msg);
        }

        fprintf(stdout, "[INFO] kfs_validate_script: Lua script validated successfully.\n");
        return KFS_OK;
    }

    // --- Unsupported Format ---
    else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Unsupported script format '%s'.", format);
        fprintf(stderr, "[WARN] kfs_validate_script: %s\n", msg);
        return set_error(msg, error_msg);
    }
}

#endif /* KFS_IMPLEMENTATION */

#endif /* KFS_IMPL_LC_H */
