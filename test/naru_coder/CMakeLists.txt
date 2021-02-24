cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME naru_coder_test)

# 実行形式ファイル
add_executable(${TEST_NAME} main.cpp)

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/NARUCoder/include)
include_directories(${PROJECT_ROOT_PATH}/libs/NARUInternal/include)
include_directories(${PROJECT_ROOT_PATH}/libs/NARUBitStream/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} naru_bit_stream) 
target_link_libraries(${TEST_NAME} naru_internal) 
target_link_libraries(${TEST_NAME} gtest) 
target_link_libraries(${TEST_NAME} gtest_main) 
target_link_libraries(${TEST_NAME} pthread) 

add_test(
    NAME naru_coder
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_coder
    PROPERTY LABELS lib naru_coder
    )

# ビルド後にテストリソースを持ってくる
add_custom_command(
  TARGET ${TEST_NAME}
  POST_BUILD
  COMMAND
  ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PriChanIcon.png $<TARGET_FILE_DIR:${TEST_NAME}>)
