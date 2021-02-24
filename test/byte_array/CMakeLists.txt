cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME byte_array_test)

# 実行形式ファイル
add_executable(${TEST_NAME} main.cpp)

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/ByteArray/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} gtest) 
target_link_libraries(${TEST_NAME} gtest_main) 
target_link_libraries(${TEST_NAME} pthread) 

add_test(
    NAME byte_array
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST byte_array
    PROPERTY LABELS lib byte_array
    )
