cmake_minimum_required(VERSION 3.16)
project(cdyn LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The C++ core (M1) — tests/bench binaries off for the wheel build.
set(DYNMATCH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DYNMATCH_BUILD_BENCH OFF CACHE BOOL "" FORCE)
add_subdirectory(cpp)
set_target_properties(dynmatch PROPERTIES POSITION_INDEPENDENT_CODE ON)  # needs -fPIC for the shared module

find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(cdyn cpp/python/cdyn.cpp)
target_link_libraries(cdyn PRIVATE dynmatch)

# Wheel layout: extension module + the pure-Python dynamic.py at top level.
install(TARGETS cdyn DESTINATION .)
install(FILES dynamic.py DESTINATION .)
