# Edge26SimStandalone — builds the sim sources outside of UE5 for headless tests.
cmake_minimum_required(VERSION 3.20)
project(edge26_sim_replay CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Werror -Wno-unused-parameter)
    # Defined integer overflow for sim arithmetic (we never rely on it but want defined behavior).
    add_compile_options(-fwrapv)
endif()

# The sim sources live in Edge26Sim/. CMake compiles them directly — they must
# never #include Engine/, GameFramework/, Chaos/, etc.
set(SIM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Edge26Sim)

file(GLOB_RECURSE SIM_HEADERS
    ${SIM_ROOT}/Public/*.h
)
file(GLOB_RECURSE SIM_SOURCES
    ${SIM_ROOT}/Private/*.cpp
)
# Exclude UE5-only files (module bootstrap etc.) from the standalone build.
list(FILTER SIM_SOURCES EXCLUDE REGEX "/Private/UE5/")

set(TEST_SOURCES
    tests/test_math.cpp
    tests/test_snapshot.cpp
)

add_executable(edge26_sim_replay
    main.cpp
    ReplayIO.cpp
    ${TEST_SOURCES}
    ${SIM_HEADERS}
    ${SIM_SOURCES}
)

target_include_directories(edge26_sim_replay PRIVATE
    ${SIM_ROOT}/Public
    ${SIM_ROOT}/Private
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(replay_generator
    tests/replay_generator.cpp
    ReplayIO.cpp
    ${SIM_HEADERS}
)
target_include_directories(replay_generator PRIVATE
    ${SIM_ROOT}/Public
    ${CMAKE_CURRENT_SOURCE_DIR}
)
