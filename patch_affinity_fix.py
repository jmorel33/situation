import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

# Fix duplicates (because I ran this in earlier steps but python script replaced the original text again, or maybe the previous check was missing?)
data = re.sub(r'(_SituationSetThreadAffinity\(.*?\);\n)\s*_SituationSetThreadAffinity\(.*?\);\n', r'\1', data)

with open("sit/situation_impl.h", "w") as f:
    f.write(data)
