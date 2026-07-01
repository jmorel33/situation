import re

with open("px_vm.h", "r") as f:
    text = f.read()

# Let's count OpCodes to verify where to insert
matches = re.findall(r'OP_[A-Z_]+.*= 0x[0-9A-F]+', text)
for match in matches:
    if "0x4" in match:
        print(match)
