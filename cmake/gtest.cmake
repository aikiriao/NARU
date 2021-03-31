cmake_minimum_required(VERSION 3.1)

# Google Test settings
include(ExternalProject)

# gtest追加
ExternalProject_Add(GoogleTest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         release-1.7.0
    INSTALL_COMMAND   ""
    LOG_DOWNLOAD ON
    )

# インクルードパス追加
ExternalProject_Get_Property(GoogleTest source_dir)
include_directories(${source_dir}/include)

# 成果物のディレクトリ取得
ExternalProject_Get_Property(GoogleTest binary_dir)

# ライブラリ追加
add_library(gtest STATIC IMPORTED)
set_property(
    TARGET gtest
    PROPERTY IMPORTED_LOCATION ${binary_dir}/libgtest.a
    )

# メインエントリ追加
add_library(gtest_main STATIC IMPORTED)
set_property(
    TARGET gtest_main
    PROPERTY IMPORTED_LOCATION ${binary_dir}/libgtest_main.a
    )
