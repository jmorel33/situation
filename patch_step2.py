import re

with open("situation_api.h", "r") as f:
    content = f.read()

# Make sure SituationCmdBeginRenderToDisplay is marked deprecated
pattern = r"SITAPI SituationError SituationCmdBeginRenderToDisplay\("
if "[[deprecated" not in content and "__attribute__((deprecated))" not in content.replace("SITAPI", "SITAPI"):
    content = content.replace("SITAPI SituationError SituationCmdBeginRenderToDisplay(", "SITAPI __attribute__((deprecated(\"Use SituationCmdBeginRenderPass instead\"))) SituationError SituationCmdBeginRenderToDisplay(")

with open("situation_api.h", "w") as f:
    f.write(content)
