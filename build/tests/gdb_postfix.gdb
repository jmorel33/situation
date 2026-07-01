set pagination off
set confirm off
set $n = 0
break vkDestroyShaderModule
commands
  silent
  set $n = $n + 1
  printf "mod destroy #%d handle=%p\n", $n, $rdx
  continue
end
break vkDestroyPipeline
commands
  silent
  set $n = $n + 1
  printf "pipe destroy #%d handle=%p\n", $n, $rdx
  continue
end
break vkDestroyPipelineLayout
commands
  silent
  set $n = $n + 1
  printf "layout destroy #%d handle=%p\n", $n, $rdx
  continue
end
run --module core --headless
bt 10
