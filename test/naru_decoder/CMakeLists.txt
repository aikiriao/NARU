cmake_minimum_required(VERSION 3.15)

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
target_link_libraries(${TEST_NAME} gtest gtest_main naru_encoder naru_coder byte_array naru_bit_stream naru_internal lpc_calculator)
if (NOT MSVC)
target_link_libraries(${TEST_NAME} pthread)
endif()

# コンパイルオプション
set_target_properties(${TEST_NAME}
    PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

add_test(
    NAME naru_decoder
    COMMAND $<TARGET_FILE:${TEST_NAME}>
    )

# run with: ctest -L lib
set_property(
    TEST naru_decoder
    PROPERTY LABELS lib naru_decoder
    )
