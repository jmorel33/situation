import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

# I messed up `_SituationGLExecuteCommands` brace!
# Let's search for `[GLExecute] START:`
match = re.search(r'SIT_DEBUG_LOG\("\[GLExecute\] START: packet_count=%zu\\n", buf->packet_count\);(.*?)\n    if \(buf->packet_count == 0\) return;', data, flags=re.DOTALL)
if match:
    pass

# Wait, `if (buf->packet_count == 0) return;` is around line 6018.
# Why did all functions after this get nested inside `_SituationGLExecuteCommands`?
# Did I accidentally delete a closing brace `}` inside `_SituationGLExecuteCommands`?
# Let's look at the end of `_SituationGLExecuteCommands`
