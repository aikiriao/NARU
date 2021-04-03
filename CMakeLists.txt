cmake_minimum_required(VERSION 3.15)

# コーデックライブラリ
project(NARUCodecLibrary C)
set(CODEC_LIB_NAME narucodec)
add_library(${CODEC_LIB_NAME}
    STATIC
    $<TARGET_OBJECTS:naru_encoder>
    $<TARGET_OBJECTS:naru_decoder>
    $<TARGET_OBJECTS:naru_coder>
    $<TARGET_OBJECTS:naru_bit_stream>
    $<TARGET_OBJECTS:naru_internal>
    $<TARGET_OBJECTS:lpc_calculator>
    )

# デコーダライブラリ
project(NARUDecoderLibrary C)
set(DECODER_LIB_NAME narudec)
add_library(${DECODER_LIB_NAME}
    STATIC
    $<TARGET_OBJECTS:naru_decoder>
    $<TARGET_OBJECTS:naru_coder>
    $<TARGET_OBJECTS:naru_bit_stream>
    $<TARGET_OBJECTS:naru_internal>
    )

# 依存するプロジェクト
add_subdirectory(libs)

# テスト
if(NOT without-test)
    enable_testing()
    # C++環境でないとgtestがビルドできないので、CXXプロジェクトを作る
    # -> Cとの挙動で差異が生じるかもしれない...
    project(NARUTest CXX)
    if(MSVC)
        # ランタイムライブラリの整合性が取れずリンクエラーが出るためdllを使わない
        set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
        set(CMAKE_CXX_FLAGS_RELEASE "/MT")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    else()
        set(CMAKE_CXX_FLAGS "-std=gnu++11") # gtestがGNU独自拡張を使用しているため
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    endif()
    include(cmake/gtest.cmake)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_subdirectory(test)
endif()
