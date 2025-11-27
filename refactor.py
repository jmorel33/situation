import re
import sys

def get_return_type_and_name(signature):
    clean_sig = re.sub(r'\s+', ' ', signature.replace("SITAPI", "").strip())

    # The name is the word right before the opening parenthesis
    name_match = re.search(r'(\w+)\s*\((.*)\)\s*\{', clean_sig)
    if not name_match:
        name_match = re.search(r'(\w+)\s*\((.*)\)', clean_sig)
        if not name_match:
             return None, None

    name = name_match.group(1)

    try:
        name_index = clean_sig.rindex(' ' + name + '(')
        return_type = clean_sig[:name_index].strip()

        if name.startswith('*'):
            num_stars = name.count('*')
            return_type += ' *' * num_stars
            name = name.lstrip('*')

        return return_type, name
    except ValueError:
        return None, None


def get_default_return_value(return_type):
    if return_type == "void":
        return None
    # Pointers
    if return_type.endswith("*") or return_type in ["SituationContext", "GLFWwindow*", "VkInstance", "VkDevice", "VkPhysicalDevice", "VkRenderPass", "VkCommandBuffer", "SituationAudioDeviceInfo*"]:
        return "NULL"
    if return_type == "bool":
        return "false"
    # Numeric types
    if return_type in ["int", "uint32_t", "uint64_t", "long", "char", "size_t", "GLint", "GLuint", "ma_uint64", "ma_result", "BOOL", "HRESULT", "DWORD"]:
        return "0"
    if return_type in ["float", "double"]:
        return "0.0f"
    # Structs
    if return_type.startswith("Situation") or return_type.startswith("Color") or return_type == "Vector2" or return_type == "Rectangle" or return_type == "_SituationQueueFamilyIndices" or return_type.startswith("Vk") or return_type.startswith("D3D11"):
        return "{0}"
    if return_type == "SituationError":
        return "SITUATION_ERROR_NOT_INITIALIZED"
    return "0"


def process_file(filepath):
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading file: {e}", file=sys.stderr)
        return

    implementation_start_index = content.find("#ifdef SITUATION_IMPLEMENTATION")
    if implementation_start_index == -1:
        print("SITUATION_IMPLEMENTATION not found", file=sys.stderr)
        return

    header_part = content[:implementation_start_index]
    implementation_part = content[implementation_start_index:]

    new_implementation_lines = []
    lines = implementation_part.split('\n')

    i = 0
    while i < len(lines):
        line = lines[i]

        if line.strip().startswith("SITAPI"):
            full_signature = line
            # Collect multi-line signature
            brace_line_index = i
            while '{' not in lines[brace_line_index]:
                brace_line_index += 1
                if brace_line_index >= len(lines):
                    new_implementation_lines.extend(lines[i:brace_line_index])
                    i = brace_line_index
                    break
                full_signature += " " + lines[brace_line_index]

            if brace_line_index >= len(lines):
                continue

            original_lines_for_sig = lines[i:brace_line_index+1]

            return_type, name = get_return_type_and_name(full_signature)

            # Functions that create or manage contexts don't need the macro.
            # Also, static helper functions that are not part of the API but might have SITAPI for DLL export reasons might need to be excluded if they are called internally without a context.
            # For now, this list is based on the user's PR description and obvious cases.
            if name and name not in ["SituationGetVersionString", "SituationCreateContext", "SituationDestroyContext", "SituationSetCurrentContext", "SituationGetCurrentContext", "SituationLogWarning"]:

                new_implementation_lines.extend(original_lines_for_sig)

                if return_type == "void":
                    macro = "    SIT_RESOLVE_CTX_OR_RETURN_VOID(ctx);"
                else:
                    default_val = get_default_return_value(return_type)
                    if default_val is None: # Should only be for void, but as a safeguard
                        print(f"Warning: No default return value for type '{return_type}' in function '{name}'. Skipping macro.", file=sys.stderr)
                    else:
                        macro = f"    SIT_RESOLVE_CTX_OR_RETURN(ctx, {default_val});"
                        new_implementation_lines.append(macro)
            else:
                new_implementation_lines.extend(original_lines_for_sig)

            i = brace_line_index + 1
        else:
            new_implementation_lines.append(line)
            i += 1

    new_content = header_part + "\n".join(new_implementation_lines)

    try:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(new_content)
    except Exception as e:
        print(f"Error writing file: {e}", file=sys.stderr)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        process_file(sys.argv[1])
    else:
        print("Please provide a file path.", file=sys.stderr)
