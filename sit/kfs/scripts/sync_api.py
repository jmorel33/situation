#!/usr/bin/env python3
"""Sync kfs_api.h from impl signatures.

Today reads kfs_impl.h (orchestrator only — no bodies post-S6).
S7: scan kfs_impl_core.h + kfs_impl_auth.h + kfs_impl_lc.h instead.
"""
import re
from pathlib import Path
from typing import List

KFS_PKG_DIR = Path(__file__).resolve().parent.parent
KFS_SRC_DIR = KFS_PKG_DIR / "kfs"
IMPL_PATH = KFS_SRC_DIR / "kfs_impl.h"
API_PATH = KFS_SRC_DIR / "kfs_api.h"


def patch_impl(text: str) -> str:
    text = text.replace("if (rc != KFS_OK && rc != KFS_ROW)", "if (rc != KFS_OK)")

    fwd = """
/* --- Static helper forward declarations --- */
static int check_group_admin_or_owner_perm(GameDB* db, uint64_t requesting_user_uuid, int target_group_actor_id);
static int get_active_actor_info_by_uuid(GameDB* db, uint64_t actor_uuid, int* actor_id, char** actor_type, char** name, int* is_admin);
static int kfs_get_topic_id_by_name(GameDB* db, int domain_id, const char* name, int* topic_id);
static int kfs_get_epic_id_by_name(GameDB* db, int domain_id, const char* name, int* epic_id);
static int kfs_save_asset(GameDB* db, const char* type, const char* name, const char* format, uint64_t creator_uuid, int owner_actor_id, int security_scheme_id, const void* data, size_t data_size, const char* text_data, const char* metadata, int* artifact_id_out);
static int kfs_load_asset_list(GameDB* db, sqlite3_stmt* stmt_ids, uint64_t requesting_user_uuid, KFS_Asset** results, int* result_count);

"""
    fwd_path = KFS_SRC_DIR / "kfs_impl_fwd.h"
    has_split_forwards = fwd_path.is_file() and "kfs_impl_fwd.h" in text
    if "Static helper forward declarations" not in text and not has_split_forwards:
        text = text.replace('#include "kfs_api.h"\n\n', '#include "kfs_api.h"\n' + fwd + "\n")
    if has_split_forwards and "Static helper forward declarations" in text and 'kfs_impl_fwd.h' not in text:
        # Legacy: monolith still had inline forwards; prefer include chain (S2+).
        text = re.sub(
            r'\n/* --- Static helper forward declarations --- */\n'
            r'static int check_group_admin_or_owner_perm.*?'
            r'static int kfs_load_asset_list\([^\n]+\n\n',
            '\n#include "kfs_impl_fwd.h"\n\n',
            text,
            count=1,
            flags=re.DOTALL,
        )

    old_epic_by_name = """int kfs_get_epic_by_name(GameDB* db, const char* name, KFS_Epic* epic) {
     if (!db || !db->arch_db || !name || !epic) return KFS_INVALID_ARGUMENT;
     int epic_id = -1;
     int rc = kfs_get_epic_id_by_name(db, name, &epic_id);
     if (rc != KFS_OK) return rc;
     return kfs_get_epic(db, epic_id, epic);
}"""
    new_epic_by_name = """int kfs_get_epic_by_name(GameDB* db, uint64_t requesting_actor_uuid, int domain_id, const char* name, KFS_Epic* epic) {
     if (!db || !db->arch_db || !name || !epic || requesting_actor_uuid == 0 || domain_id <= 0) return KFS_INVALID_ARGUMENT;
     int epic_id = -1;
     int rc = kfs_get_epic_id_by_name(db, domain_id, name, &epic_id);
     if (rc != KFS_OK) return rc;
     return kfs_get_epic(db, requesting_actor_uuid, domain_id, epic_id, epic);
}"""
    text = text.replace(old_epic_by_name, new_epic_by_name)

    text = text.replace(
        "int kfs_load_by_topic(GameDB* db, uint64_t requesting_user_uuid, const char* topic_name, KFS_Asset** results, int* result_count)",
        "int kfs_load_by_topic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* topic_name, KFS_Asset** results, int* result_count)",
    )
    text = text.replace(
        "if (!db || !db->arch_db || !db->artifacts_db || !db->registry_db || requesting_user_uuid == 0 || !topic_name || !results || !result_count) {\n        return KFS_INVALID_ARGUMENT;\n    }\n    *results = NULL; *result_count = 0;\n\n    int rc = KFS_OK;\n    int topic_id = -1;\n    sqlite3_stmt* stmt_ids = NULL;\n\n    // Get Topic ID\n    rc = kfs_get_topic_id_by_name(db, topic_name, &topic_id);",
        "if (!db || !db->arch_db || !db->artifacts_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !topic_name || !results || !result_count) {\n        return KFS_INVALID_ARGUMENT;\n    }\n    *results = NULL; *result_count = 0;\n\n    int rc = KFS_OK;\n    int topic_id = -1;\n    sqlite3_stmt* stmt_ids = NULL;\n\n    // Get Topic ID\n    rc = kfs_get_topic_id_by_name(db, domain_id, topic_name, &topic_id);",
    )

    text = text.replace(
        "int kfs_load_by_epic(GameDB* db, uint64_t requesting_user_uuid, const char* epic_name, KFS_Asset** results, int* result_count)",
        "int kfs_load_by_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* epic_name, KFS_Asset** results, int* result_count)",
    )
    text = text.replace(
        "if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || !epic_name || !results || !result_count) {\n         return KFS_INVALID_ARGUMENT;\n    }\n    *results = NULL; *result_count = 0;\n\n    int rc = KFS_OK;\n    int epic_id = -1;\n    sqlite3_stmt* stmt_ids = NULL;\n\n    // Get Epic ID\n    rc = kfs_get_epic_id_by_name(db, epic_name, &epic_id);",
        "if (!db || !db->arch_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !epic_name || !results || !result_count) {\n         return KFS_INVALID_ARGUMENT;\n    }\n    *results = NULL; *result_count = 0;\n\n    int rc = KFS_OK;\n    int epic_id = -1;\n    sqlite3_stmt* stmt_ids = NULL;\n\n    // Get Epic ID\n    rc = kfs_get_epic_id_by_name(db, domain_id, epic_name, &epic_id);",
    )

    text = text.replace(
        "int kfs_load_scripts_by_epic(GameDB* db, uint64_t requesting_user_uuid, const char* epic_name, const char* format, KFS_Asset** results, int* result_count)",
        "int kfs_load_scripts_by_epic(GameDB* db, uint64_t requesting_user_uuid, int domain_id, const char* epic_name, const char* format, KFS_Asset** results, int* result_count)",
    )
    text = text.replace(
        "if (!db || !db->arch_db || !db->artifacts_db || !db->registry_db || requesting_user_uuid == 0 || !epic_name || !results || !result_count) {\n         return KFS_INVALID_ARGUMENT;\n     }\n    *results = NULL; *result_count = 0;\n\n    int rc = KFS_OK;\n    int epic_id = -1;\n    sqlite3_stmt* stmt_ids = NULL;\n    char sql_ids_buffer[512];\n    int bind_count = 1;\n\n    // Get Epic ID\n    rc = kfs_get_epic_id_by_name(db, epic_name, &epic_id);",
        "if (!db || !db->arch_db || !db->artifacts_db || !db->registry_db || requesting_user_uuid == 0 || domain_id <= 0 || !epic_name || !results || !result_count) {\n         return KFS_INVALID_ARGUMENT;\n     }\n    *results = NULL; *result_count = 0;\n\n    int rc = KFS_OK;\n    int epic_id = -1;\n    sqlite3_stmt* stmt_ids = NULL;\n    char sql_ids_buffer[512];\n    int bind_count = 1;\n\n    // Get Epic ID\n    rc = kfs_get_epic_id_by_name(db, domain_id, epic_name, &epic_id);",
    )

    chunks = re.split(r"(?=^(?:static )?(?:int|void) )", text, flags=re.MULTILINE)
    out = []
    for chunk in chunks:
        if re.match(
            r"^(?:static )?(?:int|void) kfs_\w+\([^)]*requesting_actor_uuid[^)]*\)\s*\{",
            chunk,
            re.DOTALL,
        ):
            chunk = chunk.replace("requesting_user_uuid", "requesting_actor_uuid")
        out.append(chunk)
    text = "".join(out)

    if "#ifndef KFS_IMPL_H" not in text:
        text = text.replace(
            "#ifdef KFS_IMPLEMENTATION\n",
            "#ifndef KFS_IMPL_H\n#define KFS_IMPL_H\n\n#ifdef KFS_IMPLEMENTATION\n",
            1,
        )
        text = text.replace(
            "#endif /* KFS_IMPLEMENTATION */",
            "#endif /* KFS_IMPLEMENTATION */\n\n#endif /* KFS_IMPL_H */",
            1,
        )

    return text


def strip_c_comments(text: str) -> str:
    """Remove // and /* */ comments so inline param notes do not merge signatures."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text[i : i + 2] == "//":
            i += 2
            while i < n and text[i] not in "\r\n":
                i += 1
            continue
        if text[i : i + 2] == "/*":
            end = text.find("*/", i + 2)
            i = n if end == -1 else end + 2
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


def extract_decls(text: str) -> List[str]:
    lines = text.splitlines()
    decls = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if re.match(r"^(int|void)\s+kfs_\w+\s*\(", line) and not line.strip().startswith("static"):
            parts = [line.rstrip()]
            i += 1
            while i < len(lines) and "{" not in parts[-1]:
                parts.append(lines[i].rstrip())
                i += 1
            sig = " ".join(strip_c_comments(p) for p in parts)
            sig = sig[: sig.rindex(")") + 1]
            sig = re.sub(r"\s+", " ", sig).strip()
            decls.append(sig + ";")
            continue
        i += 1
    seen = set()
    unique = []
    for d in decls:
        if d not in seen:
            seen.add(d)
            unique.append(d)
    return unique


def regenerate_api(decls: List[str]) -> None:
    header = API_PATH.read_text(encoding="utf-8", errors="replace")
    start = header.index("/* --- Function Declarations --- */")
    end = header.index("#endif /* KFS_API_H */")
    new_header = (
        header[:start]
        + "/* --- Function Declarations --- */\n\n"
        + "\n".join(decls)
        + "\n\n"
        + header[end:]
    )
    with open(API_PATH, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(new_header)


def main() -> None:
    text = IMPL_PATH.read_text(encoding="utf-8", errors="replace")
    text = patch_impl(text)
    IMPL_PATH.write_text(text, encoding="utf-8")
    decls = extract_decls(text)
    regenerate_api(decls)
    print(f"Patched impl; regenerated {len(decls)} API declarations")


if __name__ == "__main__":
    main()