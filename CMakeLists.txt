cmake_minimum_required(VERSION 3.15)
project(GCRecompiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Capstone (integrated but not used yet)
FetchContent_Declare(
    capstone
    GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
    GIT_TAG 5.0.1
)
FetchContent_MakeAvailable(capstone)

include_directories(include)

add_executable(gcrecomp 
    src/main.cpp 
    src/loader/dol.cpp
)

target_link_libraries(gcrecomp PRIVATE capstone)
