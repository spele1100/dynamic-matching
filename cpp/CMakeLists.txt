cmake_minimum_required(VERSION 3.16)
project(dynmatch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# No forced -march: on x86-64 the baseline (SSE2) auto-vectorizes the shim
# fallbacks; on aarch64, armv8-a (baseline NEON) activates the NEON paths in
# include/dynmatch/simd.h. Cross-build for ARM64 targets with:
#   cmake -B build-arm -DCMAKE_SYSTEM_NAME=Linux \
#         -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
#         -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
#         -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ && cmake --build build-arm
# (firefly RK3588 / DVS boards build natively with plain `cmake -B build`.)

add_library(dynmatch STATIC
    src/blossom.cpp
    src/dynmatch.cpp
)
target_include_directories(dynmatch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(DYNMATCH_BUILD_TESTS "Build test binary" ON)
option(DYNMATCH_BUILD_BENCH "Build benchmark binary" ON)

if(DYNMATCH_BUILD_TESTS)
    add_executable(test_dynmatch tests/test_dynmatch.cpp)
    target_link_libraries(test_dynmatch PRIVATE dynmatch)
    set(GOLDEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)
    target_compile_definitions(test_dynmatch PRIVATE GOLDEN_DIR="${GOLDEN_DIR}")
    add_test(NAME dynmatch COMMAND test_dynmatch ${GOLDEN_DIR})
    enable_testing()
endif()

if(DYNMATCH_BUILD_BENCH)
    add_executable(bench_dynamic bench/bench_dynamic.cpp)
    target_link_libraries(bench_dynamic PRIVATE dynmatch)
endif()
