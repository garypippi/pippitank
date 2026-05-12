add_library(pippitank-telemetry INTERFACE)
target_include_directories(pippitank-telemetry INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pippitank-telemetry INTERFACE cxx_std_17)
target_link_libraries(pippitank-telemetry INTERFACE
    pippitank-protocol
)
