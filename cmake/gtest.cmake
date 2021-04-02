cmake_minimum_required(VERSION 3.1)

# Google Test settings
include(ExternalProject)

# gtest追加
ExternalProject_Add(GoogleTest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         release-1.10.0
    INSTALL_COMMAND   ""
    LOG_DOWNLOAD ON
    )

# インクルードパス追加
ExternalProject_Get_Property(GoogleTest source_dir)
include_directories(${source_dir}/googletest/include)

# 成果物のディレクトリ取得
ExternalProject_Get_Property(GoogleTest binary_dir)

# ライブラリ追加
add_library(gtest STATIC IMPORTED)
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(libgtest_path "${binary_dir}/lib/Release/gtest.lib")
    else()
        set(libgtest_path "${binary_dir}/lib/Debug/gtestd.lib")
    endif()
    set_property(
        TARGET gtest
        PROPERTY IMPORTED_LOCATION ${libgtest_path}
        )
else()
    set_property(
        TARGET gtest
        PROPERTY IMPORTED_LOCATION ${binary_dir}/lib/libgtest.a
        )
endif()

# メインエントリ追加
add_library(gtest_main STATIC IMPORTED)
if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(libgtest_main_path "${binary_dir}/lib/Release/gtest_main.lib")
    else()
        set(libgtest_main_path "${binary_dir}/lib/Debug/gtest_maind.lib")
    endif()
    set_property(
        TARGET gtest_main
        PROPERTY IMPORTED_LOCATION ${libgtest_main_path}
        )
else()
    set_property(
        TARGET gtest_main
        PROPERTY IMPORTED_LOCATION ${binary_dir}/lib/libgtest_main.a
        )
endif()
