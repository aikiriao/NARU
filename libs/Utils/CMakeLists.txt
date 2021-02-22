cmake_minimum_required(VERSION 3.1)

# プロジェクト名
project(utility C)

# ライブラリ名
set(LIB_NAME utils)

# 静的ライブラリ指定
add_library(${LIB_NAME} STATIC)

# ソースディレクトリ
add_subdirectory(src)

# インクルードパス
target_include_directories(${LIB_NAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# コンパイルオプション
if(MSVC)
  target_compile_options(${LIB_NAME} PRIVATE /W4)
else()
  target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=2 -Wconversion -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition)
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
  set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()
set_target_properties(${LIB_NAME} PROPERTIES C_STANDARD 90 C_EXTENSIONS OFF)

# テスト
if(NOT without-test)
    enable_testing()
    project(utility_test CXX)
    include(../../cmake/gtest.cmake)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_subdirectory(test)
    # ビルド後にテストリソースを持ってくる
    add_custom_command(
      TARGET ${LIB_NAME}
      POST_BUILD
      COMMAND
      ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test/wav/a.wav $<TARGET_FILE_DIR:${LIB_NAME}>/test/wav/)
endif()
