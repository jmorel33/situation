# Kaizen Filing System (KFS) Security Model Manual

**Version:** 2.3 (Unified memory layer; security model unchanged from 2.1)
**License:** MIT — (c) 2025-2026 Jacques Morel

## 1. Overview

The Kaizen Filing System (KFS) implements a robust, domain-driven security model designed to provide granular control over access to managed entities (Artifacts, Notes, Topics, Epics) within isolated **Domains**. The model centers on **Ownership**, **Security Schemes**, and **Domain-based access control**, ensuring that only authorized **Actors** (Users, Groups, Companies) can interact with entities. **Domains** act as organizational and security boundaries, with a **Domain firewall** enforcing access restrictions. Administrative roles (**Admin** and **Domain Admin**) enable flexible management, supporting multiple administrators.

**Core Principles:**
- **Domain Isolation**: All entities are scoped to a specific Domain, accessible only to Actors with explicit Domain membership.
- **Owner Control**: The Owner of an entity has full permissions, augmented by Security Schemes to grant specific permissions (Read, Write, Delete) to other Actors.
- **Administrative Flexibility**: Admins manage entities within authorized Domains, while Domain Admins manage Domain access roles without data access.
- **Privacy Enforcement**: Target users (e.g., patients in a MedicalDomain) are excluded from accessing their own data, ensuring compliance with privacy requirements.

This manual details the core concepts, permission checking logic, administrative roles, and practical examples for implementation.

## 2. Core Concepts

Understanding these elements is fundamental to using KFS security effectively:

### 2.1. Actors

- An **Actor** is any entity interacting with KFS, such as Users, Groups, Companies, or system processes.
- Defined in the `Actors` table (`registry.db`) with:
  - `id`: Unique `INTEGER PRIMARY KEY` for internal relationships, ownership, and permissions.
  - `uuid`: Globally unique 64-bit `INTEGER` (KFS UUID) for identification and creator tracking.
  - `actor_type`: `TEXT` indicating type (e.g., 'USER', 'GROUP', 'COMPANY', 'SYSTEM').
  - `name`: `TEXT` for username, group name, etc.
  - `role`: Optional `TEXT` for roles (e.g., 'USER', 'ADMIN'). Used for legacy single-admin checks, now largely replaced by group-based roles.
  - `is_active`: `INTEGER` flag (1 for active, 0 for inactive). Inactive Actors cannot perform actions or receive permissions.
- **AdminGroup**: A special Group (`actor_type = 'GROUP', name = 'AdminGroup'`) whose members have administrative privileges across authorized Domains.

### 2.2. Groups and Membership

- Actors with `actor_type` 'GROUP' or 'COMPANY' can contain other Actors as members, defined in the `GroupMembers` table (`registry.db`) linking `group_actor_id` to `member_actor_id`.
- Membership grants inherited permissions for ownership or Security Schemes.
- **Note**: Current implementation assumes single-level group membership for simplicity. Recursive checks are not supported but can be added if needed.

### 2.3. Domains

- **Domains** are top-level organizational and security boundaries, defined in the `Domains` table (`registry.db`) with:
  - `id`: Unique `INTEGER PRIMARY KEY`.
  - `name`: `TEXT` for domain name (e.g., 'MedicalDomain').
  - `owner_actor_id`: `INTEGER` referencing `Actors.id` for the domain owner.
  - `creator_uuid`: `INTEGER` for the creator’s UUID.
  - `created_at`: `TEXT` timestamp.
  - `description`: Optional `TEXT` description.
- **DomainActors**: The `DomainActors` table links `domain_id` to `actor_id`, defining which Actors (Users or Groups) have access to a Domain.
- **Domain Firewall**: All entity operations require Domain membership (direct or via group) before ownership or scheme checks, enforced by `kfs_check_permission`.

### 2.4. Ownership (`owner_actor_id`)

- Every entity (Artifact, Note, Topic, Epic, SecurityScheme, Domain) has an **Owner**, stored in the `owner_actor_id` column (`INTEGER`) referencing `Actors.id`.
- Owners have full **Read, Write, Delete** permissions on their entities, unless restricted by Domain access.
- Owners can assign or modify `security_scheme_id` on their entities.

### 2.5. Creation Tracking (`creator_uuid`)

- Entities track their creator via the `creator_uuid` column (`INTEGER`), storing the `Actors.uuid` of the creating Actor.
- **Creator status grants no permissions**, serving only for traceability.

### 2.6. Security Schemes

- A **Security Scheme** is an Access Control List (ACL) applied by an Owner to control entity access, defined in the `SecuritySchemes` table (`registry.db`):
  - `id`: Unique `INTEGER PRIMARY KEY`.
  - `domain_id`: `INTEGER` linking to `Domains.id`, ensuring schemes are domain-specific.
  - `name`: `TEXT` for scheme name (e.g., 'Medical_Dossier_Access').
  - `creator_uuid`: `INTEGER` for creator’s UUID.
  - `owner_actor_id`: `INTEGER` for the scheme owner.
  - `created_at`: `TEXT` timestamp.
- Entities link to a scheme via the nullable `security_scheme_id` column (`INTEGER`). A `NULL` or invalid ID defaults to owner-only access.

### 2.7. Permissions and `SchemeAllowedActors`

- The `SchemeAllowedActors` table (`registry.db`) defines permissions for a scheme:
  - `security_scheme_id`: `INTEGER` referencing `SecuritySchemes.id`.
  - `actor_id`: `INTEGER` referencing `Actors.id` (User or Group).
  - `can_read`: `INTEGER` (1 for read access, 0 for none).
  - `can_write`: `INTEGER` (1 for write access, 0 for none).
  - `can_delete`: `INTEGER` (1 for delete access, 0 for none).
- Permissions apply to direct Users or Group members.

### 2.8. Administrative Roles

- **Admin Role**:
  - Granted via membership in the `AdminGroup` (Group with `name = 'AdminGroup'`).
  - Admins have full **Read, Write, Delete** permissions on entities within Domains they access (via `DomainActors`).
  - Admins can manage Domains (`kfs_add_domain`, `kfs_update_domain`, `kfs_delete_domain`) and Security Schemes.
- **Domain Admin Role**:
  - A specialized role for managing Domain access roles (adding/removing Actors in `DomainActors`) without accessing entity data.
  - Granted via a Security Scheme or group membership (e.g., 'DomainAdmins' group) with **Write** permission on the Domain.
  - Domain Admins cannot access entity data unless explicitly granted via `DomainActors` or `SchemeAllowedActors`.
- **Multiple Admins**: Supported by adding multiple Users to `AdminGroup` or 'DomainAdmins' group, replacing the legacy single-admin model.

### 2.9. Permission Definitions

The permissions `Read`, `Write`, and `Delete` have specific meanings depending on the entity they are applied to. Understanding these is crucial for correctly configuring Security Schemes and predicting API behavior.

| Entity Type | Read Permission (`KFS_PERM_READ`) | Write Permission (`KFS_PERM_WRITE`) | Delete Permission (`KFS_PERM_DELETE`) |
| :--- | :--- | :--- | :--- |
| **Artifact** | - View metadata (`kfs_get_artifact`)<br>- Read asset data (`kfs_get_asset_data`)<br>- Load the full artifact (`kfs_load_artifact`) | - Update metadata (`kfs_update_artifact`)<br>- Link/Unlink asset data (`kfs_link_asset_to_artifact`, `kfs_delete_asset`)<br>- Assign/Remove Topics (`kfs_assign_topic_to_artifact`)<br>- Assign/Remove Notes (`kfs_assign_note`) | - Permanently delete the artifact and its asset (`kfs_delete_artifact`) |
| **Note** | - View note content (`kfs_get_note`) | - Update note content or ownership (`kfs_update_note`) | - Permanently delete the note (`kfs_delete_note`) |
| **Topic** | - View topic metadata (`kfs_get_topic`)<br>- List artifacts assigned to the topic (`kfs_load_by_topic`) | - Update metadata (`kfs_update_topic`)<br>- Link/Unlink related topics (`kfs_link_related_topic`)<br>- Assign/Remove Epics (`kfs_assign_epic_to_topic`) | - Permanently delete the topic (`kfs_delete_topic`) |
| **Epic** | - View epic metadata (`kfs_get_epic`)<br>- List topics assigned to the epic | - Update metadata (`kfs_update_epic`) | - Permanently delete the epic (`kfs_delete_epic`) |
| **Security<br>Scheme** | - View the scheme's configuration, including its list of allowed actors and permissions (`kfs_get_security_scheme`) | - Modify the scheme's Access Control List (ACL) by adding or removing actors (`kfs_add_actor_to_scheme`, `kfs_remove_actor_from_scheme`)<br>- Update the scheme's owner | - Permanently delete the security scheme (`kfs_delete_security_scheme`) |
| **Domain** | - The fundamental "Domain Firewall" pass. Grants the ability to attempt any action on entities within the Domain. Without Read access to the Domain, all other permissions are irrelevant. | - Grants **Domain Admin** role. Allows adding/removing actors from the Domain (`kfs_add_actor_to_domain`, `kfs_remove_actor_from_domain`) and managing the Domain's metadata (`kfs_update_domain`). | - Permanently delete the Domain (`kfs_delete_domain`). This is a highly destructive action typically reserved for members of the `AdminGroup`. |

## 3. Permission Check Flow (`kfs_check_permission`)

The `kfs_check_permission` function verifies access for entity operations, enforcing the **Domain firewall** and admin roles:

1. **Identify Requester**:
   - Maps `requesting_user_uuid` to `requesting_actor_id` in `Actors`. Checks `is_active`. If inactive or not found, **DENY** (`KFS_PERMISSION_DENIED` or `KFS_NOTFOUND`).
2. **Check AdminGroup Membership**:
   - Queries `GroupMembers` for `AdminGroup` membership. If found, the user is an Admin.
3. **Identify Entity**:
   - Retrieves `domain_id`, `owner_actor_id`, and `security_scheme_id` for the `entity_type` (e.g., 'Artifact') and `entity_id`. If not found, **DENY** (`KFS_NOTFOUND`).
4. **Domain Firewall**:
   - Checks `DomainActors` for direct `actor_id` membership in `domain_id`.
   - If not found, checks group-based membership via `GroupMembers` for Groups/Companies in `DomainActors`.
   - If no Domain access, **DENY** (`KFS_PERMISSION_DENIED`).
5. **Admin Bypass**:
   - If the user is in `AdminGroup` and has Domain access, **GRANT** access, bypassing ownership/scheme checks.
6. **Ownership Check**:
   - If `requesting_actor_id` matches `owner_actor_id`, **GRANT**.
   - If the owner is a Group/Company, checks `GroupMembers` for membership. If found, **GRANT**.
7. **Security Scheme Check**:
   - If `security_scheme_id` is invalid (`NULL` or <= 0), **DENY**.
   - Validates `security_scheme_id` belongs to `domain_id` in `SecuritySchemes`. If not, **DENY** (`KFS_NOTFOUND`).
   - **Direct Grant**: Checks `SchemeAllowedActors` for `requesting_actor_id`. If found and the required permission (`can_read`, `can_write`, `can_delete`) is set, **GRANT**.
   - **Group Grant**: Checks `SchemeAllowedActors` for Groups/Companies. For each:
     - Verifies the group has the required permission.
     - Checks `GroupMembers` for `requesting_actor_id`. If both true, **GRANT**.
   - If no scheme grants apply, **DENY**.
8. **Final Denial**:
   - If all checks fail, **DENY** (`KFS_PERMISSION_DENIED`).

## 4. Default Behavior

- **No Security Scheme**: If `security_scheme_id` is `NULL` or invalid, only the Owner (direct or via group) can access the entity.
- **No Domain Access**: Without `DomainActors` membership, all operations are denied by the Domain firewall, even for Owners or Admins.
- **AdminGroup**: Admins bypass ownership/scheme checks but require Domain access.
- **Domain Admin**: Can manage `DomainActors` entries but needs explicit permissions for entity access.

## 5. Practical Usage & Examples

**Setup**:
- Actors: UserA (id=1, uuid=1001), UserB (id=2, uuid=1002), MedicalGroup (id=101, uuid=10101, type='GROUP'), AdminGroup (id=201, uuid=20101, type='GROUP', name='AdminGroup'), DomainAdmins (id=301, uuid=30101, type='GROUP').
- UserA is a member of `AdminGroup`; UserB is a member of `MedicalGroup` and `DomainAdmins`.
- Domain: MedicalDomain (id=1, owner_actor_id=101).

**Example 1: Owner-Only Artifact in Domain**
- UserA creates Artifact 10 in MedicalDomain using `kfs_create_artifact(..., creator_uuid=1001, owner_actor_id=101, domain_id=1, security_scheme_id=NULL)`.
- **Result**:
  - UserA (AdminGroup) attempts `kfs_get_artifact(...)` -> Domain access check passes -> Admin bypass -> **GRANT**.
  - UserB (MedicalGroup member) attempts Read -> Domain access passes -> Not Owner -> No Scheme -> **DENY**.
  - UserC (no Domain access) attempts Read -> Domain firewall denies access -> **DENY**.

**Example 2: Security Scheme with Group Access**
- UserA creates Scheme 20 in MedicalDomain using `kfs_create_security_scheme(..., owner_actor_id=101, domain_id=1)`.
- UserA calls `kfs_add_actor_to_scheme(..., domain_id=1, scheme_id=20, allowed_actor_id=101, can_read=1, can_write=1, can_delete=0)`.
- UserA applies Scheme 20 to Artifact 11 by calling `kfs_update_artifact(..., security_scheme_id=20)`.
- **Result**:
  - UserA: Admin bypass -> **GRANT**.
  - UserB: Domain access -> `MedicalGroup` is in Scheme 20 -> `can_read=1` -> **GRANT** for Read operations.
  - UserB attempts `kfs_delete_artifact(...)` -> `can_delete=0` -> **DENY**.
  - UserC: No Domain access -> **DENY**.

**Example 3: Domain Admin Managing Access**
- UserB (DomainAdmins group member) is granted `Write` permission on the `MedicalDomain` entity itself, for instance via a dedicated "DomainAccessControl" Security Scheme.
- UserB can now successfully call `kfs_add_actor_to_domain(..., domain_id=1, actor_id=102)` to add UserC to the MedicalDomain.
- **Result**:
  - UserB: `Write` permission on the Domain entity grants authorization for managing `DomainActors`.
  - UserB attempts to read Artifact 11 using `kfs_get_artifact(...)` -> No scheme permission on the artifact itself -> **DENY**.
  - UserC: Now has Domain access and can access artifacts if permitted by their individual schemes.

**Example 4: Admin Deleting Artifact**
- UserA (`AdminGroup` member) calls `kfs_delete_artifact(..., domain_id=1, artifact_id=10)`.
- **Result**:
  - `kfs_check_permission` verifies UserA has Domain access, then identifies them as an `AdminGroup` member.
  - The check returns **GRANT**, bypassing ownership and scheme rules.
  - Artifact 10 and its asset are deleted. Cascading deletes remove associated entries from `TopicAssignments` and `EntityNotes`.

**Example 5: Trash Can Workflow with Domain**
- **Setup**: A 'TrashCan' Epic exists in `MedicalDomain` (owned by `MedicalGroup`, with Scheme 8 applied: `MedicalGroup` R/W=1, D=0; `Auditor` group R/W/D=1).
- UserB wants to "delete" Artifact 15 (which has Scheme 7: `MedicalGroup` R/W=1, D=0).
- **Application Logic**:
  - Application first checks if a hard delete is possible: `kfs_check_permission(..., artifact_id=15, KFS_PERM_DELETE)` -> **DENY**.
  - Application performs a "soft delete" by calling:
    - `kfs_remove_topic_from_artifact(...)`
    - `kfs_assign_topic_to_artifact_by_name(..., topic_name="TrashItems")`
    - `kfs_update_artifact(..., security_scheme_id=8)` to apply the TrashCan scheme.
    - All these calls require `Write` permission, which is granted to UserB by Scheme 7.
- An Auditor user (member of `Auditor` group) later calls `kfs_delete_artifact(..., domain_id=1, artifact_id=15)`. Scheme 8 on the artifact grants them `Delete` permission -> **GRANT**.

## 6. Important Considerations

- **Domain Setup**: Correctly define `Domains`, `DomainActors`, and `SecuritySchemes` to enforce isolation and privacy. All entity operations require the `domain_id` to enforce the Domain firewall.

- **Admin vs. Domain Admin Roles**:
  - **Admin**: An Actor with full data access privileges. Granted via membership in the Group named **`AdminGroup`**. Admins bypass ownership and scheme checks for entities within Domains they can access. The `Actors.role` column is deprecated for permission checks and should be ignored.
  - **Domain Admin**: A role for managing a Domain's user access list. Granted by giving an Actor `Write` permission on the Domain entity itself. This allows them to call `kfs_add_actor_to_domain` and `kfs_remove_actor_from_domain` but grants no access to the data within the Domain.

- **`Actors.id` vs. `Actors.uuid`**: Using the correct identifier is critical.
  - **Use `Actors.id` (internal `INTEGER`) for:**
    - `owner_actor_id` on all entities.
    - `actor_id` in the `SchemeAllowedActors` table.
    - `actor_id` in the `DomainActors` table.
    - `group_actor_id` and `member_actor_id` in `GroupMembers`.
  - **Use `Actors.uuid` (64-bit `INTEGER`) for:**
    - `creator_uuid` on all entities (for tracking/auditing).
    - `requesting_user_uuid` in all permission-checked API functions.

### 6.1. Error Handling

API functions return status codes to indicate success or failure. Understanding the most common error codes is essential for building a reliable application.

- **`KFS_OK`**: The operation was successful.
- **`KFS_PERMISSION_DENIED`**: The user is authenticated and the entity exists, but they do **not** have the required permissions for the action (e.g., trying to `Write` with only `Read` access, or lacking Domain access).
- **`KFS_NOTFOUND`**: The requested entity (e.g., Artifact, Topic, Domain) does not exist. This can also be returned if the user lacks the permissions to even know the entity exists, providing an extra layer of security through obscurity.
- **`KFS_CONSTRAINT`**: The operation violated a database rule, such as trying to create an entity with a name that must be unique (e.g., a `Domain` or a `Topic` within a domain).
- **`KFS_INVALID_ARGUMENT`**: The function was called with invalid parameters, such as a `NULL` pointer for a required output or an ID of `0`.
- **`KFS_MISMATCH`**: The operation failed because of incompatible entities, most commonly trying to link entities from different Domains.

- **Performance**: Index `DomainActors.domain_id`, `SecuritySchemes.domain_id`, `SchemeAllowedActors.actor_id`, and `GroupMembers.member_actor_id` to optimize permission checks.

- **Transactions**: Wrap multi-table operations (e.g., `kfs_delete_artifact`) in transactions for consistency. The library aims to do this internally for its own functions.

- **Privacy**: Ensure target users are excluded from `DomainActors` and `SchemeAllowedActors` for their own data (e.g., medical records).

***

### **Replacement for Section 7 (add new subsection 7.1)**

*This adds a new subsection to "7. API Integration Notes" that provides a clear bootstrap procedure.*

## 7. API Integration Notes

- **Memory init**: Call `kfs_mem_init(NULL)` before any `sqlite3_*` in your process; `kfs_init` calls it automatically if omitted. Free API-returned pointers with `kfs_mem_free()` (see `kfs_mem.h`, [architecture.md](architecture.md) §9).
- **Domain Parameter**: Most functions (e.g., `kfs_get_artifact`, `kfs_delete_artifact`) require `domain_id` to enforce the Domain firewall.
- **Requesting User**: All access-controlled functions take `requesting_user_uuid` for permission checks.
- **Creation**: Functions creating entities (e.g., `kfs_create_artifact`) require `owner_actor_id` and `creator_uuid`.
- **Security Schemes**: Use `kfs_add_actor_to_scheme` to manage permissions, ensuring `domain_id` matches.
- **Admin Functions**: Domain management (`kfs_add_domain`, `kfs_update_domain`) requires `AdminGroup` membership.
- **Error Handling**: Check for `KFS_PERMISSION_DENIED`, `KFS_NOTFOUND`, and SQLite errors.
- **load_artifact**: Integrates with `kfs_get_artifact` and `kfs_get_asset_data`, respecting Domain and scheme permissions.

### 7.1. System Bootstrap

To initialize a new KFS instance and create the first administrator, follow this sequence:

1.  **Initialize memory and databases**:
    Call `kfs_mem_init(NULL)` if you use SQLite elsewhere; then `kfs_init(&db, ...)` to create and open the database files and schema.

2.  **Create the `AdminGroup`**:
    The `AdminGroup` is the cornerstone of system administration. Create it as an Actor.
    ```c
    int admin_group_id;
    uint64_t admin_group_uuid;
    // The first action may use a temporary 'system' UUID (e.g., 0) as the requester.
    kfs_add_actor(db, 0, "GROUP", "AdminGroup", "SYSTEM", 1, &admin_group_uuid, &admin_group_id);
    ```

3.  **Create the First Administrative User**:
    Create the first user who will be the system administrator.
    ```c
    int first_admin_id;
    uint64_t first_admin_uuid;
    // Again, the system (0) can be the initial requester.
    kfs_add_actor(db, 0, "USER", "god_user", "USER", 1, &first_admin_uuid, &first_admin_id);
    ```

4.  **Add User to `AdminGroup`**:
    This is the step that grants administrative privileges.
    ```c
    // first_admin_uuid is the requesting actor (requester UUID).
    kfs_add_member_to_group(db, first_admin_uuid, admin_group_id, first_admin_id);
    ```

5.  **Create Initial Domain**:
    With an administrative user in place, you can now create your first Domain.
    ```c
    int domain_id;
    // The new admin (first_admin_uuid) is now the requester.
    kfs_add_domain(db, first_admin_uuid, "PrimaryDomain", admin_group_id, "Main organizational domain", &domain_id);
    ```
From this point on, all actions should be performed by an authenticated user (`requesting_user_uuid`).

### 7.2. Legacy `kfs_save_*` API

`kfs_save_text`, `kfs_save_script`, and `kfs_save_file` are convenience helpers that predate the
UUID-based permission convention. They take **`owner_id` and `creator_id`** as internal `Actors.id`
values (not UUIDs) and do **not** accept `requesting_user_uuid`. Topic assignment inside these
helpers uses an internal bypass (no permission check; topic resolved by name globally). Prefer
`kfs_create_artifact` and the domain-scoped APIs for new code. A future revision may align these
signatures with the rest of the public API.

