find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
add_executable(pippitank-ui main.cpp)
target_link_libraries(pippitank-ui PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
    tl::expected
    pippitank-protocol
    pippitank-io
    pippitank-telemetry
)
