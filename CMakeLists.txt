cmake_minimum_required(VERSION 3.15)
project(GCRecompiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Capstone disabled temporarily for custom decoder
# include(FetchContent)

# # Explicitly define ONLY PowerPC (because this was apparently needed)
# set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_PPC_SUPPORT ON CACHE BOOL "" FORCE)
# set(CAPSTONE_ARM_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_ARM64_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_MIPS_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_X86_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_SPARC_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_SYSZ_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_XCORE_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_TMS320C64X_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_M68K_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_EVM_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_WASM_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_BPF_SUPPORT OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_RISCV_SUPPORT OFF CACHE BOOL "" FORCE)

# set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
# set(CAPSTONE_BUILD_STATIC ON CACHE BOOL "" FORCE)
# set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
# # Switch to more stable 4.0.2
# FetchContent_Declare(
#     capstone
#     GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
#     GIT_TAG 4.0.2
# )
# FetchContent_MakeAvailable(capstone)

add_executable(gcrecomp 
    src/main.cpp 
    src/loader/dol.cpp
    src/analysis/disasm.cpp
    src/analysis/cfg.cpp
    src/analysis/analyzer.cpp
    src/analysis/lifter.cpp
    src/analysis/optimizer.cpp
    src/codegen/emitter.cpp
)

target_include_directories(gcrecomp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
