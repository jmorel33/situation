
def check_ifdefs(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()

    stack = []
    for i, line in enumerate(lines):
        line_num = i + 1
        stripped = line.strip()

        # Skip comments (simple check)
        if stripped.startswith("//"): continue

        if stripped.startswith("#if") or stripped.startswith("#ifdef") or stripped.startswith("#ifndef"):
            stack.append((line_num, stripped))
        elif stripped.startswith("#endif"):
            if not stack:
                print(f"Error: Unmatched #endif at line {line_num}")
            else:
                stack.pop()

    if stack:
        print("Error: Unclosed preprocessor directives (Stack Dump):")
        for line_num, content in stack:
            print(f"  Line {line_num}: {content}")

        # Heuristic: The last few items on the stack are likely the culprits
        if len(stack) > 2:
             print("\nPotential culprit (last unclosed inner block):")
             print(f"  Line {stack[-1][0]}: {stack[-1][1]}")
             print(f"  Line {stack[-2][0]}: {stack[-2][1]}")

    else:
        print("All preprocessor directives are balanced.")

check_ifdefs('situation.h')
