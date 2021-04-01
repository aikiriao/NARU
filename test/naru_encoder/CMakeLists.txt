cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME naru_encoder_test)

# 実行形式ファイル
add_executable(${TEST_NAME}
    naru_encoder_test.cpp
    naru_encode_processor_test.cpp
    main.cpp
    )

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/naru_encoder/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} byte_array lpc_calculator  naru_internal naru_bit_stream naru_coder)
if (MSVC)
target_link_libraries(${TEST_NAME} gtest gtest_main)
else()
target_link_libraries(${TEST_NAME} gtest gtest_main pthread)
endif()

add_test(
    NAME naru_encoder
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_encoder
    PROPERTY LABELS lib naru_encoder
    )
