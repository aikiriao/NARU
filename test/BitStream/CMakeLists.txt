cmake_minimum_required(VERSION 3.1)

# テスト名
set(TEST_NAME bit_stream_test)

# 実行形式ファイル
add_executable(${TEST_NAME}
  main.cpp
  bit_stream_test.cpp 
  byte_array_test.cpp
  )

# インクルードディレクトリ
set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${PROJECT_ROOT_PATH}/libs/BitStream/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} gtest) 
target_link_libraries(${TEST_NAME} gtest_main) 
target_link_libraries(${TEST_NAME} pthread) 

add_test(
    NAME bit_stream
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST bit_stream
    PROPERTY LABELS lib bit_stream
    )
