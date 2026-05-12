add_executable(pippitankd main.cpp)
target_link_libraries(pippitankd PRIVATE
    pippitank-protocol
    pippitank-io
)
