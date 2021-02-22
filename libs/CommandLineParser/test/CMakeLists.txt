cmake_minimum_required(VERSION 3.1)

add_executable(command_line_parser_test main.cpp)

# リンクするライブラリ
target_link_libraries(command_line_parser_test
    gtest
    gtest_main
    pthread
    )

add_test(
    NAME command_line_parser
    COMMAND $<TARGET_FILE:command_line_parser_test>
    )

# run with: ctest -L lib
set_property(
    TEST command_line_parser
    PROPERTY LABELS lib command_line_parser
    )
