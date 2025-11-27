
import re

# This script is designed to be run on the situation.h file
# after the function DEFINITIONS have been updated, but before the
# CALL SITES have been updated.

# It performs two main passes:
# 1. Manifest Generation: It scans the file to build a list of all functions
#    (both SITAPI and static) that now require a `SituationContext` parameter.
# 2. Call Site Injection: It re-reads the file and uses the manifest to find
#    all call sites for those functions, injecting `_ctx, ` as the first argument.

SITUATION_H_PATH = "situation.h"

def generate_function_manifest(file_content):
    """
    Parses the file content to find all SITAPI and static function declarations
    that have been updated to accept a SituationContext parameter.
    """
    manifest = set()
    # Regex to find SITAPI or static functions that take SituationContext as the first param
    # It captures the function name.
    # Pattern: (SITAPI|static) [return_type] FunctionName(SituationContext ctx, ...)
    # It's simplified to look for the function name after the type and before the parenthesis.
    pattern = re.compile(r"^(?:SITAPI|static)\s+[\w\s\*]+\s+([a-zA-Z_]\w*)\s*\(\s*SituationContext\s+\w+\s*[,)]")

    for line in file_content.splitlines():
        match = pattern.match(line.strip())
        if match:
            function_name = match.group(1)
            # Exclude the context management functions themselves
            if function_name not in ["SituationCreateContext", "SituationDestroyContext", "SituationSetCurrentContext", "SituationGetCurrentContext", "SituationLogWarning"]:
                 manifest.add(function_name)

    # Also add functions that were manually updated or missed by regex
    # For example, if the function signature is split across lines.
    # This list can be manually curated if needed.
    # For now, we rely on the regex which should be quite effective.

    return manifest

def update_call_sites(file_content, manifest):
    """
    Finds and replaces function calls to inject the context parameter.
    """
    # Create a massive regex pattern to find all function calls to be updated.
    # Pattern: FunctionName( not_a_context_param ... )
    # The negative lookahead `(?!_ctx)` is crucial to avoid double-patching.
    # It looks for a function name from our manifest followed by an opening parenthesis.
    # It uses a word boundary `\b` to avoid matching parts of longer names.
    function_pattern = '|'.join(r'\b' + re.escape(fn) + r'\b' for fn in manifest)
    # The regex looks for a function call that is NOT `FunctionName(_ctx, ...)`
    # and is NOT an empty call `FunctionName()`. This is to avoid changing
    # empty calls which might be a different refactor. It specifically targets
    # calls with at least one argument.
    # Correction: The logic should be simpler. If it's a function in our manifest,
    # its call site needs to be updated, unless it's a definition.
    # A definition will be preceded by SITAPI or static, or a return type. A call site won't.

    # Simpler approach: Replace `FunctionName(` with `FunctionName(_ctx, ` for all manifest functions.
    # This is brittle. A better regex looks for a function call that isn't a definition.
    # Let's try a negative lookbehind to avoid `SITAPI` or `static`.
    call_site_pattern = re.compile(
        # Negative lookbehind: Not preceded by "SITAPI " or "static "
        r"(?<!SITAPI\s|static\s)"
        # Capture one of the function names from our manifest
        r"("+ function_pattern + r")"
        # Followed by an opening parenthesis
        r"\s*\("
    )

    # We need to be careful not to modify function *pointers* being assigned.
    # e.g., `callback = FunctionName;` should not be touched. The `\(` handles this.

    # We also need to avoid changing the function DEFINITION again.
    # The negative lookbehind helps, but might not be enough if definitions span lines.
    # Let's refine. The most common pattern is `FunctionName(arg1, ...)` becoming `FunctionName(_ctx, arg1, ...)`
    # And `FunctionName()` becoming `FunctionName(_ctx)`.

    # Regex for calls with no arguments: FunctionName() -> FunctionName(_ctx)
    empty_call_pattern = re.compile(r"\b(" + function_pattern + r")\s*\(\s*\)")
    # Regex for calls with arguments: FunctionName(arg1 -> FunctionName(_ctx, arg1
    # We use a negative lookahead `(?!_ctx)` to prevent double-applying the fix.
    arg_call_pattern = re.compile(r"\b(" + function_pattern + r")\s*\((?!\s*_ctx\b)")


    # Perform the replacement
    # First, handle calls with arguments.
    updated_content = arg_call_pattern.sub(r"\1(_ctx, ", file_content)

    # Then, handle calls with no arguments on the result of the first pass.
    # This order is important.
    # We need to adjust the empty call pattern to NOT match if it was just modified.
    # Let's combine into a single, more robust regex.

    # New single regex:
    # It finds `FunctionName(` and checks what's inside.
    # If it's `)`, it replaces with `(_ctx)`.
    # If it's something else, it prepends `_ctx, `.
    # This is getting complex for a single regex. Let's do it with a function replacer.

    def replacer(match):
        function_name = match.group(1)
        # This is a call site. We need to inject `_ctx`.
        # The regex captured the function name.
        # The full match is `FunctionName(`.
        # We need to decide if we are injecting into an empty `()` or `(arg1, ...)`.
        # This is hard to do without seeing the rest of the line.

        # Let's revert to the simpler two-pass regex, but be careful.
        # The key is that they are mutually exclusive. One matches `()` and the other matches `(arg...`

        # New strategy:
        # Pass 1: Add `_ctx` to empty calls: `FunctionName()` -> `FunctionName(_ctx)`
        # Pass 2: Add `_ctx, ` to non-empty calls: `FunctionName(arg1)` -> `FunctionName(_ctx, arg1)`
        return f"{function_name}(_ctx, "


    # Let's stick with the two-pattern approach, it's safer.
    # Pass 1: `FunctionName(arg1, ...)` -> `FunctionName(_ctx, arg1, ...)`
    # This pattern looks for a function call that does NOT have `)` immediately after `(`.
    # It also avoids already-patched calls.
    with_args_pattern = re.compile(r"\b(" + function_pattern + r")\s*\(\s*(?!\s*\)|_ctx\b)")
    content_pass1 = with_args_pattern.sub(r"\1(_ctx, ", file_content)

    # Pass 2: `FunctionName()` -> `FunctionName(_ctx)`
    # This pattern looks for a function call with nothing inside the parens.
    without_args_pattern = re.compile(r"\b(" + function_pattern + r")\s*\(\s*\)")
    final_content = without_args_pattern.sub(r"\1(_ctx)", content_pass1)

    return final_content

def main():
    try:
        with open(SITUATION_H_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: {SITUATION_H_PATH} not found.")
        return

    # 1. Build the manifest of functions needing updates
    manifest = generate_function_manifest(content)
    print(f"Found {len(manifest)} functions to update.")
    # print("Functions:", sorted(list(manifest)))

    # 2. Update all call sites for these functions
    updated_content = update_call_sites(content, manifest)

    # 3. Write the changes back to the file
    try:
        with open(SITUATION_H_PATH, 'w', encoding='utf-8') as f:
            f.write(updated_content)
        print("Successfully updated call sites in situation.h")
    except IOError as e:
        print(f"Error writing to {SITUATION_H_PATH}: {e}")

if __name__ == "__main__":
    main()
