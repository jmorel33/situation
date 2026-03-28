import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

start = data.find('static void _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index) {')
count = 0
line_num = data[:start].count('\n') + 1

for i in range(start, len(data)):
    if data[i] == '\n':
        line_num += 1
    elif data[i] == '{':
        count += 1
    elif data[i] == '}':
        count -= 1

    if count == 0 and data[i] == '}':
        print("Function ends at line", line_num)
        break
