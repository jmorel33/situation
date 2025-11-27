
import re

SITUATION_H_PATH = "situation.h"

def add_resolve_macros(content):
    # This regex finds a function signature that we just modified,
    # captures the return type, and the opening curly brace on the same or next line.
    pattern = re.compile(
        r"^(?P<prefix>SITAPI|static)\s+(?P<return_type>[\w\s\*]+?)\s+"
        r"(?P<func_name>[a-zA-Z_]\w*)\s*\(\s*SituationContext\s+ctx.*?\)\s*\{",
        re.MULTILINE | re.DOTALL
    )

    exclude_list = [
        "SituationCreateContext", "SituationDestroyContext", "SituationSetCurrentContext", "SituationGetCurrentContext",
        "SituationLogWarning", "_SituationSetErrorFromCode", "_SituationSetError"
    ]

    def replacer(match):
        func_name = match.group("func_name")
        if func_name in exclude_list:
            return match.group(0) # Return original match

        return_type = match.group("return_type").strip()

        # Decide which macro to use based on return type
        if return_type == "void":
            macro = "    SIT_RESOLVE_CTX_OR_RETURN_VOID(ctx);"
        else:
            # Determine a sensible default return value for non-void functions.
            # 0, NULL, or false are common. We'll default to {0}, which works for pointers, bools, and numbers.
            # A more advanced script might try to parse the type more accurately.
            retval = "{0}"
            if "bool" in return_type:
                retval = "false"
            elif "*" in return_type:
                retval = "NULL"

            macro = f"    SIT_RESOLVE_CTX_OR_RETURN(ctx, {retval});"

        # The match includes the opening brace, so we add the macro after it.
        return f"{match.group(0)}\n{macro}"

    updated_content = pattern.sub(replacer, content)
    return updated_content

def main():
    try:
        with open(SITUATION_H_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: {SITUATION_H_PATH} not found.")
        return

    updated_content = add_resolve_macros(content)

    try:
        with open(SITUATION_H_PATH, 'w', encoding='utf-8') as f:
            f.write(updated_content)
        print("Successfully injected context-resolution macros.")
    except IOError as e:
        print(f"Error writing to {SITUATION_H_PATH}: {e}")

if __name__ == "__main__":
    main()
