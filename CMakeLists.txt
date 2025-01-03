# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Project name and supported languages
project(MyProject VERSION 1.0 LANGUAGES CXX)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the executable name
set(EXECUTABLE_NAME DebuggerInfrastructure)

# Specify the directories containing header files
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Find the required libraries
# Attempt to locate sqlite3 in the specified libs directory
find_library(SQLITE3_LIBRARY sqlite3
    PATHS ${CMAKE_SOURCE_DIR}/libs
    NO_DEFAULT_PATH
)

# Check if sqlite3 was found
if(NOT SQLITE3_LIBRARY)
    message(FATAL_ERROR "sqlite3 library not found in ${CMAKE_SOURCE_DIR}/libs")
endif()

# Add the executable and specify source files
add_executable(${EXECUTABLE_NAME}
    src/main/main.cpp
    src/Logger/Logger.cpp
    src/Common/DbHandler.cpp
    src/REST/RESTapi.cpp
    src/FrontEnd/FrontEnd.cpp
)

# Include directories for the target
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${INCLUDE_DIRS}
)

# Link necessary libraries
target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
        ${SQLITE3_LIBRARY}
)
# Link ws2_32 only on Windows
if(WIN32)
    target_link_libraries(${EXECUTABLE_NAME}
    PRIVATE
        ws2_32)
endif()

# Add compiler flags based on the compiler being used and build type
if (MSVC)
    # Compiler flags for MSVC
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /W4)
else()
    # Compiler flags for GCC/Clang
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -fdiagnostics-color=always)
endif()

# Handle different build types
# CMake supports Debug, Release, RelWithDebInfo, and MinSizeRel by default
# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Apply build type specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -g)
    message(STATUS "Configuring for Debug build")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -O3)
    message(STATUS "Configuring for Release build")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -O2 -g)
    message(STATUS "Configuring for RelWithDebInfo build")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    target_compile_options(${EXECUTABLE_NAME} PRIVATE -Os)
    message(STATUS "Configuring for MinSizeRel build")
endif()

# Set the output directory for the executable
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add a post-build step to copy the 'res' directory to the executable's directory
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>/res
)

