cmake_minimum_required(VERSION 2.8)

# cl likes to complain about POSIX interfaces like fopen, but we don't care.

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_definitions(-std=c++11)
  add_definitions(-fno-exceptions)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_definitions(/wd4201)
  add_definitions(/W4 /WX)
  add_definitions(/GR-)
  add_definitions(/D_CRT_SECURE_NO_WARNINGS)
  add_definitions(/DWIN32_LEAN_AND_MEAN)
  add_definitions(/D_HAS_EXCEPTIONS=0)
endif()

add_subdirectory (wotparser)
