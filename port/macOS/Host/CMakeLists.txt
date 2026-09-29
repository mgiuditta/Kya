project(Kya_Mac)

# ponytail: win_main.cpp has no Windows code, so both hosts share it. Split it when one diverges.
add_executable(Kya_Mac "${CMAKE_CURRENT_LIST_DIR}/../../Windows/Host/src/win_main.cpp")
target_link_libraries(Kya_Mac PRIVATE Kya DebugMenu)

message("Exe directory: ${KYA_EXECUTABLE_DIRECTORY}")
set_target_properties(Kya_Mac PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${KYA_EXECUTABLE_DIRECTORY}")
set_target_properties(Kya_Mac PROPERTIES OUTPUT_NAME "Kya_$<CONFIG>")
