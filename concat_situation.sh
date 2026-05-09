#!/bin/bash
# concat_situation.sh - Concatenate the Situation library into a single C file.
#
# Recursively resolves all #include "..." directives within sit/ files,
# producing a self-contained output suitable for single-file distribution
# or feeding into tools that need the full source in one shot.
#
# Usage:
#     ./concat_situation.sh [output_file]
#
#     Default output: build/situation_full.c

set -e
export LC_ALL=C

OUTPUT="${1:-situation_full.c}"

declare -A SEEN
FILE_COUNT=0

resolve_include() {
    local inc_path="$1"
    local current_file="$2"
    local base_dir
    base_dir=$(dirname "$current_file")
    
    local candidate="$base_dir/$inc_path"
    if [[ -f "$candidate" ]]; then
        echo "$candidate"
        return
    fi
    # Try from repo root
    if [[ -f "$inc_path" ]]; then
        echo "$inc_path"
        return
    fi
    echo ""
}

should_inline() {
    local inc_path="$1"
    local resolved="$2"
    
    [[ -z "$resolved" ]] && return 1
    
    # Inline anything under sit/
    if [[ "$resolved" == sit/* || "$resolved" == */sit/* ]]; then
        return 0
    fi
    # Also inline situation_impl* or situation_api* relative includes
    if [[ "$inc_path" == situation_impl* || "$inc_path" == situation_api* ]]; then
        return 0
    fi
    return 1
}

concat_file() {
    local filepath="$1"
    
    # Normalize path
    local norm_path
    norm_path=$(realpath "$filepath" 2>/dev/null || echo "$filepath")
    
    if [[ ! -f "$filepath" ]]; then
        echo "/* [file not found: $filepath] */"
        return
    fi
    
    if [[ -n "${SEEN[$norm_path]+x}" ]]; then
        echo "/* [already included: $filepath] */"
        return
    fi
    SEEN["$norm_path"]=1
    FILE_COUNT=$((FILE_COUNT + 1))
    
    local sep
    sep=$(printf '=%.0s' {1..70})
    echo ""
    echo "/* $sep */"
    echo "/* FILE: $filepath */"
    echo "/* $sep */"
    echo ""
    
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ ^[[:space:]]*\#include[[:space:]]+\"([^\"]+)\" ]]; then
            local inc_path="${BASH_REMATCH[1]}"
            local resolved
            resolved=$(resolve_include "$inc_path" "$filepath")
            if should_inline "$inc_path" "$resolved"; then
                concat_file "$resolved"
                continue
            fi
        fi
        printf '%s\n' "$line"
    done < "$filepath"
}

# --- Main ---
if [[ ! -f "situation.h" ]]; then
    echo "Error: situation.h not found. Run from the repo root." >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT" 2>/dev/null || echo ".")"

{
    echo "/* Auto-generated single-file concatenation of the Situation library. */"
    echo "/* Do not edit. Regenerate with: ./concat_situation.sh */"
    concat_file "situation.h"
} > "$OUTPUT"

LINE_COUNT=$(wc -l < "$OUTPUT")
echo "Concatenated $FILE_COUNT files -> $OUTPUT ($LINE_COUNT lines)"
