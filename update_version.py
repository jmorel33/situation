import re

files_to_update = [
    'sit/k-term/example/situation_api.h',
]

def update_version(filepath, new_patch):
    with open(filepath, 'r') as f:
        content = f.read()

    new_content = re.sub(r'#define SITUATION_VERSION_PATCH \d+', f'#define SITUATION_VERSION_PATCH {new_patch}', content)

    with open(filepath, 'w') as f:
        f.write(new_content)
    print(f"Updated {filepath}")

for file in files_to_update:
    update_version(file, '62')
