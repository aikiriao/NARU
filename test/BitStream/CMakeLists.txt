cmake_minimum_required(VERSION 3.1)

add_executable(bit_stream_test main.cpp)

# リンクするライブラリ
target_link_libraries(bit_stream_test
    gtest
    gtest_main
    pthread
    )

add_test(
    NAME bit_stream
    COMMAND $<TARGET_FILE:bit_stream_test>
    )

# run with: ctest -L lib
set_property(
    TEST bit_stream
    PROPERTY LABELS lib bit_stream
    )
