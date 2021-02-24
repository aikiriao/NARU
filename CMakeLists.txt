cmake_minimum_required(VERSION 3.1)

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
  add_subdirectory(libs)
  add_subdirectory(test)
endif()
