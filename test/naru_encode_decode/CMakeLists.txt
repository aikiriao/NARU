cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME naru_encode_decode_test)

# 実行形式ファイル
add_executable(${TEST_NAME}
    main.cpp
    )

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/include)
include_directories(${PROJECT_ROOT_PATH}/libs/naru_internal/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} naru_encoder naru_decoder  naru_coder byte_array naru_bit_stream naru_internal lpc_calculator)
if (MSVC)
target_link_libraries(${TEST_NAME} gtest gtest_main)
else()
target_link_libraries(${TEST_NAME} gtest gtest_main pthread)
endif()

add_test(
    NAME naru_encode_decode
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_encode_decode
    PROPERTY LABELS lib naru_encode_decode
    )
