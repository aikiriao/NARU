cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME command_line_parser_test)

# 実行形式ファイル
add_executable(${TEST_NAME} main.cpp)

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/command_line_parser/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} gtest)
target_link_libraries(${TEST_NAME} gtest_main)
target_link_libraries(${TEST_NAME} pthread)

add_test(
    NAME command_line_parser
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST command_line_parser
    PROPERTY LABELS lib command_line_parser
    )
