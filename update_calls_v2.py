
import re

# This is a revised, more careful script to update function call sites.
# It processes the file line-by-line and applies stricter rules to
# avoid incorrectly modifying definitions, declarations, and comments.

SITUATION_H_PATH = "situation.h"

def generate_function_manifest(file_content):
    """
    Parses the file content to find all SITAPI and static function declarations
    that have been updated to accept a SituationContext parameter. This part
    of the original script was correct.
    """
    manifest = set()
    # This regex is quite specific and works well for finding the definitions.
    pattern = re.compile(r"^(?:SITAPI|static)\s+[\w\s\*]+\s+([a-zA-Z_]\w*)\s*\(\s*SituationContext\s+\w+\s*[,)]")

    for line in file_content.splitlines():
        match = pattern.match(line.strip())
        if match:
            function_name = match.group(1)
            # Exclude context management functions to avoid recursion or strange replacements
            if function_name not in ["SituationCreateContext", "SituationDestroyContext", "SituationSetCurrentContext", "SituationGetCurrentContext", "SituationLogWarning", "_SituationSetErrorFromCode"]:
                manifest.add(function_name)
    return manifest

def update_call_sites_carefully(file_content, manifest):
    """
    Finds and replaces function calls, but with strict contextual checks.
    """
    # Create the regex pattern for all target function names.
    # The `\b` ensures we match whole words only.
    function_pattern = '|'.join(r'\b' + re.escape(fn) + r'\b' for fn in manifest)
    if not function_pattern:
        return file_content # Manifest is empty, nothing to do.

    # Regex to find a call to one of the functions.
    # It looks for `FunctionName(` and captures the name.
    # It uses a negative lookahead `(?!\s*_ctx\b)` to avoid double-patching.
    call_pattern = re.compile(r"\b(" + function_pattern + r")\s*\((?!\s*_ctx\b)")

    processed_lines = []
    lines = file_content.splitlines()

    for line in lines:
        stripped_line = line.strip()

        # --- CONTEXT CHECKS ---
        # 1. Skip definitions and declarations
        if stripped_line.startswith("SITAPI") or stripped_line.startswith("static"):
            processed_lines.append(line)
            continue
        # 2. Skip single-line comments
        if stripped_line.startswith("//"):
            processed_lines.append(line)
            continue
        # 3. Skip lines that are just part of a multi-line comment block
        if stripped_line.startswith("*"):
            processed_lines.append(line)
            continue

        # --- PERFORM REPLACEMENT ---
        # Now that we've filtered out the most problematic lines, apply the replacement.
        # This is safer than a global find-replace.

        # The replacer function will decide what to replace with.
        def replacer(match):
            func_name = match.group(1)
            # We need to look ahead to see if the call is empty `()` or has args.
            # The full string from the match point onwards helps here.
            # `match.end()` gives the index of the character right after `FunctionName(`.
            rest_of_line = line[match.end():]

            if rest_of_line.lstrip().startswith(')'):
                # This is an empty call: `FunctionName()`
                # We replace `FunctionName()` with `FunctionName(_ctx)`
                # The regex match is just `FunctionName(`, so we return `FunctionName(_ctx`
                # and let the rest of the line be handled. This is tricky.

                # A better way: the replacement string should be carefully constructed.
                # The regex `call_pattern` matches `FunctionName(`. We replace it with
                # `FunctionName(_ctx` if the next non-space char is `)`, or
                # `FunctionName(_ctx, ` otherwise.

                # For simplicity in this script, let's stick to two patterns again,
                # but applied line-by-line.

                pass # Re-evaluating the replacement strategy below.

        # New Strategy: Two patterns applied line-by-line.

        # Pattern 1: Find calls with arguments and inject `_ctx, `
        # `FunctionName(arg1` -> `FunctionName(_ctx, arg1`
        # `(?!\s*\))` ensures there's something other than `)` after the `(`.
        with_args_pattern = re.compile(r"\b(" + function_pattern + r")\s*\(\s*(?!\s*\)|_ctx\b)")
        modified_line = with_args_pattern.sub(r"\1(_ctx, ", line)

        # Pattern 2: Find empty calls and inject `_ctx`
        # `FunctionName()` -> `FunctionName(_ctx)`
        without_args_pattern = re.compile(r"\b(" + function_pattern + r")\s*\(\s*\)")
        # Run this on the result of the first pass to handle lines with multiple calls
        final_line = without_args_pattern.sub(r"\1(_ctx)", modified_line)

        processed_lines.append(final_line)

    return "\n".join(processed_lines)


def main():
    try:
        with open(SITUATION_H_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: {SITUATION_H_PATH} not found.")
        return

    manifest = generate_function_manifest(content)
    print(f"Generated manifest with {len(manifest)} functions.")

    if not manifest:
        print("Manifest is empty. No functions to update. Exiting.")
        return

    updated_content = update_call_sites_carefully(content, manifest)

    try:
        with open(SITUATION_H_PATH, 'w', encoding='utf-8') as f:
            f.write(updated_content)
        print(f"Careful update complete. Please review the changes in {SITUATION_H_PATH}.")
    except IOError as e:
        print(f"Error writing to {SITUATION_H_PATH}: {e}")

if __name__ == "__main__":
    main()
