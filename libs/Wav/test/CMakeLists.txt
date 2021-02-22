cmake_minimum_required(VERSION 3.1)

add_executable(wav_test main.cpp)

# リンクするライブラリ
target_link_libraries(wav_test 
    gtest
    gtest_main
    pthread
    )

add_test(
    NAME wav
    COMMAND $<TARGET_FILE:wav_test>
    )

# run with: ctest -L lib
set_property(
    TEST wav
    PROPERTY LABELS lib wav
    )
