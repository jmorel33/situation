
import re

SITUATION_H_PATH = "situation.h"

def add_context_to_signatures(content):
    # This regex is designed to find function declarations and definitions.
    # It captures the prefix, return type, function name, and existing parameters.
    # The re.MULTILINE flag is now correctly passed to re.compile.
    pattern = re.compile(
        r"^(?P<prefix>SITAPI|static)\s+(?P<return_type>[\w\s\*]+?)\s+"
        r"(?P<func_name>[a-zA-Z_]\w*)\s*\((?P<params>[^\)]*)\)",
        flags=re.MULTILINE
    )

    # List of functions to EXCLUDE from modification. This is critical.
    # These are typically callbacks from other libraries or functions that don't need context.
    exclude_list = [
        # Context management functions themselves
        "SituationCreateContext", "SituationDestroyContext", "SituationSetCurrentContext", "SituationGetCurrentContext",

        # Logging functions that are part of the context system
        "SituationLogWarning", "_SituationSetErrorFromCode",

        # GLFW callbacks with fixed signatures
        "_SituationGLFWErrorCallback", "_SituationGLFWFileDropCallback", "_SituationGLFWWindowFocusCallback",
        "_SituationGLFWWindowIconifyCallback", "_SituationGLFWFramebufferSizeCallback", "_SituationGLFWKeyCallback",
        "_SituationGLFWCharCallback", "_SituationGLFWMouseButtonCallback", "_SituationGLFWCursorPosCallback",
        "_SituationGLFWScrollCallback", "_SituationGLFWJoystickCallback",

        # MiniAudio callbacks with fixed signatures
        "sit_miniaudio_data_callback", "_sit_miniaudio_capture_callback",
        "_situation_stream_read_thunk", "_situation_stream_seek_thunk",

        # Windows callbacks
        "_SituationMonitorEnumProc",

        # Vulkan callbacks
        "_SituationVulkanDebugCallback",

        # Other internal helpers that do not require full context
        "_SituationSortVirtualDisplaysCallback"
    ]

    def replacer(match):
        func_name = match.group("func_name")
        if func_name in exclude_list:
            return match.group(0) # Return the original string unchanged

        prefix = match.group("prefix")
        return_type = match.group("return_type").strip()
        params = match.group("params").strip()

        # Construct the new parameter list
        if not params or params.lower() == 'void':
            new_params = "SituationContext ctx"
        else:
            # Avoid adding context if it's somehow already there
            if "SituationContext" in params:
                return match.group(0)
            new_params = f"SituationContext ctx, {params}"

        return f"{prefix} {return_type} {func_name}({new_params})"

    # The sub call no longer needs the flags argument.
    updated_content = pattern.sub(replacer, content)
    return updated_content

def main():
    try:
        with open(SITUATION_H_PATH, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Error: {SITUATION_H_PATH} not found.")
        return

    updated_content = add_context_to_signatures(content)

    try:
        with open(SITUATION_H_PATH, 'w', encoding='utf-8') as f:
            f.write(updated_content)
        print("Successfully added context parameter to function signatures.")
    except IOError as e:
        print(f"Error writing to {SITUATION_H_PATH}: {e}")

if __name__ == "__main__":
    main()
