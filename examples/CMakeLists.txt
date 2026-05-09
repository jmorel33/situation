# Example: Minimal (simple test)
add_executable(minimal_example minimal.c)
target_compile_definitions(minimal_example PRIVATE SITUATION_IMPLEMENTATION)
target_link_libraries(minimal_example PRIVATE situation)

# Example: Console (full featured)
add_executable(console_example console.c)
target_compile_definitions(console_example PRIVATE SITUATION_IMPLEMENTATION)
target_link_libraries(console_example PRIVATE situation)

# Set output directory
set_target_properties(minimal_example console_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy any required assets to build directory if needed
# add_custom_command(TARGET console_example POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:console_example>/assets
# )
