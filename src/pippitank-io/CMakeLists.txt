add_library(pippitank-io INTERFACE)
target_include_directories(pippitank-io INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pippitank-io INTERFACE cxx_std_17)
