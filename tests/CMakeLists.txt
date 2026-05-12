add_test(
    NAME fixture_engine_50frames
    COMMAND sh -c "$<TARGET_FILE:pippitank-emulator> --dump-fixture 50 | cmp - ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/engine_50frames.bin"
)
