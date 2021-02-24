cmake_minimum_required(VERSION 3.1)

# テスト
if(NOT without-test)
    enable_testing()
    project(NaruTest CXX)
    include(cmake/gtest.cmake)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_subdirectory(test)
endif()
