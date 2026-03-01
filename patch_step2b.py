with open("situation_impl.h", "r") as f:
    content = f.read()

content = content.replace("SITAPI SituationError SituationCmdBeginRenderToDisplay(", "SITAPI __attribute__((deprecated(\"Use SituationCmdBeginRenderPass instead\"))) SituationError SituationCmdBeginRenderToDisplay(")

with open("situation_impl.h", "w") as f:
    f.write(content)
