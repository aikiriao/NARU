cmake_minimum_required(VERSION 3.15)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME naru_bit_stream_test)

# 実行形式ファイル
add_executable(${TEST_NAME} main.cpp)

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/naru_bit_stream/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} naru_internal)
if (MSVC)
target_link_libraries(${TEST_NAME} gtest gtest_main)
else()
target_link_libraries(${TEST_NAME} gtest gtest_main pthread)
endif()

add_test(
    NAME naru_bit_stream
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_bit_stream
    PROPERTY LABELS lib naru_bit_stream
    )
