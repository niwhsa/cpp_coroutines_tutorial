cmake_minimum_required(VERSION 3.20)
project(event_scheduler)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force using Clang
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

# Add coroutines flag for Clang on macOS
add_compile_options(-std=c++20 -stdlib=libc++)

#Add all source files
set(SOURCES
    src/main.cpp
    src/event_handlers.cpp
    src/event_registry.cpp
    src/event_benchmarker.cpp
)
# Add executable
add_executable(event_scheduler ${SOURCES})

# Include directories
# target_include_directories(event_scheduler PRIVATE include)
target_include_directories(event_scheduler PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
