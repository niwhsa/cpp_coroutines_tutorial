cmake_minimum_required(VERSION 3.15)
project(event_scheduler)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add policy setting before find_package
cmake_policy(SET CMP0167 NEW)

# Find boost with all required components
find_package(Boost REQUIRED COMPONENTS 
    system 
    thread
)

# Add compiler flags for macOS
if(APPLE)
    add_compile_options(-Wall -Wextra -pedantic)
    add_compile_definitions(_DARWIN_C_SOURCE)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

set(SOURCES
    src/main.cpp
    src/fs_benchmark.cpp
    src/event_handlers.cpp
    src/event_registry.cpp
    src/event_benchmarker.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Link against all required Boost libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    Boost::system
    Boost::thread
)

# Print Boost information for debugging
message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost library dirs: ${Boost_LIBRARY_DIRS}")
