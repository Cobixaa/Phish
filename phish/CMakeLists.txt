cmake_minimum_required(VERSION 3.16)
project(phish LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler warnings and optimizations
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Wpedantic)
    add_compile_options(-march=native -mtune=native)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
        add_link_options(-flto)
    endif()
elseif (MSVC)
    add_compile_options(/W4)
endif()

include(GNUInstallDirs)

file(GLOB_RECURSE PHISH_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(phish ${PHISH_SOURCES})

target_include_directories(phish PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(phish PRIVATE Threads::Threads)

# Faster symbol stripping in release installs
install(TARGETS phish RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})