# Example: Minimal (simple test)
add_executable(minimal_example minimal.c)
target_compile_definitions(minimal_example PRIVATE SITUATION_IMPLEMENTATION)
target_link_libraries(minimal_example PRIVATE situation)

# Example: KaOS Terminal (canonical console — see doc/plan/CONSOLE_MERGE_DEPRECATION_PLAN.md)
add_executable(kterm_console console/console_host_app.c)
target_include_directories(kterm_console PRIVATE
    ${CMAKE_SOURCE_DIR}/examples/console
    ${CMAKE_SOURCE_DIR}/sit/k-term
)
target_compile_definitions(kterm_console PRIVATE
    SITUATION_IMPLEMENTATION
    SITUATION_USE_OPENGL
    SITUATION_ENABLE_THREADING
)
target_link_libraries(kterm_console PRIVATE situation)

# Example: 2D platformer (keyboard)
add_executable(platformer_plumber_example platformer_plumber.c)
target_compile_definitions(platformer_plumber_example PRIVATE SITUATION_IMPLEMENTATION)
target_link_libraries(platformer_plumber_example PRIVATE situation)

# Example: first-person raycast shooter (mouse + WASD)
add_executable(demon_hunt_example demon_hunt.c)
target_compile_definitions(demon_hunt_example PRIVATE SITUATION_IMPLEMENTATION)
target_link_libraries(demon_hunt_example PRIVATE situation)

# Set output directory
set_target_properties(minimal_example kterm_console platformer_plumber_example demon_hunt_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
