cmake_minimum_required(VERSION 3.1)

# コーデックライブラリ
project(NARUCodecLibrary C)
# ライブラリ名
set(CODEC_LIB_NAME narucodec)
# ライブラリ
add_library(${CODEC_LIB_NAME} INTERFACE)
# リンクするライブラリ
target_link_libraries(${CODEC_LIB_NAME} 
  INTERFACE byte_array lpc_calculator naru_internal naru_bit_stream naru_coder naru_decoder naru_encoder
)
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
  INTERFACE byte_array naru_internal naru_bit_stream naru_coder naru_decoder
)
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
  if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
  endif()
  include(cmake/gtest.cmake)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_subdirectory(test)
endif()
