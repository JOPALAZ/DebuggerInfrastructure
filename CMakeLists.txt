cmake_minimum_required(VERSION 3.15)
project(MyProject VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(EXECUTABLE_NAME DebuggerInfrastructure)

# Include directories for headers
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ThirdParties/sqlite3
    /usr/local/include   # NCNN headers
)

# Use pkg-config to find OpenCV
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENCV4 REQUIRED opencv4)

# Find OpenMP and Threads
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

# Add executable target
add_executable(${EXECUTABLE_NAME}
    src/main/main.cpp
    src/Logger/Logger.cpp
    src/DbHandler/DbHandler.cpp
    src/REST/RESTapi.cpp
    src/FrontEnd/FrontEnd.cpp
    src/LaserHandler/LaserHandler.cpp
    src/ServoHandler/ServoHandler.cpp
    src/GPIOHandler/GPIOHandler.cpp
    src/AimHandler/AimHandler.cpp
    src/DeadLocker/DeadLocker.cpp
    src/NeuralNetworkHandler/NeuralNetworkHandler.cpp
    src/ExternalConfigsHelper/ExternalConfigsHelper.cpp
)

# Include directories for the target
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${INCLUDE_DIRS}
    ${OPENCV4_INCLUDE_DIRS}
)

# Add compiler flags based on compiler
target_compile_options(${EXECUTABLE_NAME} PRIVATE
    ${OPENCV4_CFLAGS_OTHER}
)
if(MSVC)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /W4)
else()
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        -fdiagnostics-color=always
        -fopenmp
    )
endif()

# Link libraries
add_library(sqlite3_c STATIC src/ThirdParties/sqlite3/sqlite3.c)
set_target_properties(sqlite3_c PROPERTIES LINKER_LANGUAGE C)

target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    sqlite3_c
    fmt
    gpiod
    ${OPENCV4_LIBRARIES}
    ncnn
    OpenMP::OpenMP_CXX
    Threads::Threads
)

# Build type settings
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

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

# Output directory
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Post-build: copy resources
add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>/res
)
