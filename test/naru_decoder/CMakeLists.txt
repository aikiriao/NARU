cmake_minimum_required(VERSION 3.1)

set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# テスト名
set(TEST_NAME naru_decoder_test)

# 実行形式ファイル
add_executable(${TEST_NAME}
    naru_decoder_test.cpp
    naru_decode_processor_test.cpp
    main.cpp
    )

# インクルードディレクトリ
include_directories(${PROJECT_ROOT_PATH}/libs/naru_decoder/include)
include_directories(${PROJECT_ROOT_PATH}/libs/naru_encoder/include)

# リンクするライブラリ
target_link_libraries(${TEST_NAME} naru_encoder)
target_link_libraries(${TEST_NAME} naru_coder)
target_link_libraries(${TEST_NAME} byte_array)
target_link_libraries(${TEST_NAME} naru_bit_stream)
target_link_libraries(${TEST_NAME} naru_internal)
target_link_libraries(${TEST_NAME} lpc_calculator)
target_link_libraries(${TEST_NAME} gtest)
target_link_libraries(${TEST_NAME} gtest_main)
target_link_libraries(${TEST_NAME} pthread)

add_test(
    NAME naru_decoder
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_decoder
    PROPERTY LABELS lib naru_decoder
    )
