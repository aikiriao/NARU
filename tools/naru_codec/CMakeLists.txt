cmake_minimum_required(VERSION 3.1)

# プロジェクト名
project(NaruCodec C)

# アプリケーション名
set(APP_NAME naru)

# 依存するサブディレクトリを追加
set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)
add_subdirectory(${PROJECT_ROOT_PATH}/libs/BitStream ${CMAKE_CURRENT_BINARY_DIR}/BitStream)
add_subdirectory(${PROJECT_ROOT_PATH}/libs/CommandLineParser ${CMAKE_CURRENT_BINARY_DIR}/CommandLineParser)
add_subdirectory(${PROJECT_ROOT_PATH}/libs/LPCCalculator ${CMAKE_CURRENT_BINARY_DIR}/LPCCalculator)
add_subdirectory(${PROJECT_ROOT_PATH}/libs/Wav ${CMAKE_CURRENT_BINARY_DIR}/Wav)

# インクルードパス
target_include_directories(${APP_NAME}
  PUBLIC
    ${PROJECT_ROOT_PATH}/include
)

# 実行形式ファイル
add_executable(${APP_NAME} naru_codec.c)

# リンクするライブラリ
target_link_libraries(${APP_NAME} bit_stream)
target_link_libraries(${APP_NAME} command_line_parser)
target_link_libraries(${APP_NAME} lpc_calculator)
target_link_libraries(${APP_NAME} wav)

# コンパイルオプション
if(MSVC)
  target_compile_options(${APP_NAME} PRIVATE /W4)
else()
  target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wformat=2 -Wstrict-aliasing=2 -Wconversion -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition)
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
  set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()
set_target_properties(${APP_NAME} PROPERTIES C_STANDARD 90 C_EXTENSIONS OFF)
