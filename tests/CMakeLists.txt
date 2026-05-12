add_executable(pippitank-protocol-test protocol_roundtrip.cpp)
target_link_libraries(pippitank-protocol-test PRIVATE pippitank-protocol)

add_test(
    NAME fixture_engine_50frames
    COMMAND sh -c "$<TARGET_FILE:pippitank-emulator> --dump-fixture 50 | cmp - ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/engine_50frames.bin"
)
add_test(
    NAME protocol_roundtrip_50frames
    COMMAND pippitank-protocol-test ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/engine_50frames.bin
)
