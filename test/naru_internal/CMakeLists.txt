cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME naru_internal_test)

# 実行形式ファイル
add_executable(${TEST_NAME} 
  naru_internal_test.cpp
  naru_utility_test.cpp
  main.cpp
  )

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/naru_internal/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} gtest) 
target_link_libraries(${TEST_NAME} gtest_main) 
target_link_libraries(${TEST_NAME} pthread) 

add_test(
    NAME naru_internal
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_internal
    PROPERTY LABELS lib naru_internal
    )

# ビルド後にテストリソースを持ってくる
add_custom_command(
  TARGET ${TEST_NAME}
  POST_BUILD
  COMMAND
  ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/PriChanIcon.png ${CMAKE_CURRENT_SOURCE_DIR}/a.wav $<TARGET_FILE_DIR:${TEST_NAME}>)
