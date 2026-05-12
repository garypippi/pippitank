add_library(pippitank-protocol INTERFACE)
target_include_directories(pippitank-protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pippitank-protocol INTERFACE tl::expected)
target_compile_features(pippitank-protocol INTERFACE cxx_std_17)
