cmake_minimum_required(VERSION 3.1)

# コーデックライブラリ
project(NARUCodecLibrary C)
# ライブラリ名
set(CODEC_LIB_NAME narucodec)
# ライブラリ
add_library(${CODEC_LIB_NAME} INTERFACE)
# リンクするライブラリ
target_link_libraries(${CODEC_LIB_NAME}
    INTERFACE naru_encoder naru_decoder naru_coder naru_bit_stream naru_internal lpc_calculator byte_array
    )
if(NOT MSVC)
    target_link_libraries(${CODEC_LIB_NAME} INTERFACE m)
endif()
# インクルードパス
target_include_directories(${CODEC_LIB_NAME}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

# デコーダライブラリ
project(NARUDecoderLibrary C)
# ライブラリ名
set(DECODER_LIB_NAME narudec)
# ライブラリ
add_library(${DECODER_LIB_NAME} INTERFACE)
# リンクするライブラリ
target_link_libraries(${DECODER_LIB_NAME}
    INTERFACE naru_decoder naru_coder naru_bit_stream naru_internal byte_array
    )
if(NOT MSVC)
    target_link_libraries(${DECODER_LIB_NAME} INTERFACE m)
endif()
# インクルードパス
target_include_directories(${DECODER_LIB_NAME}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
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
    else()
        set(CMAKE_CXX_FLAGS "-std=gnu++11") # gtestがGNU独自拡張を使用しているため
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    endif()
    include(cmake/gtest.cmake)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_subdirectory(test)
endif()
