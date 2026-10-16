# horde's new engine: the library and its suite. A standalone CMake project, on purpose: it
# must configure in the builder's tree, which holds nothing above engine2/ but the spec.
#   cmake -S <abs>/engine2 -B <abs build dir> -G "Unix Makefiles"   (or run engine2/build.sh)
cmake_minimum_required(VERSION 3.20)
project(horde_engine2 VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Unix Makefiles is single-config, so the type is fixed at configure time. Release is the default
# because every performance number (spec section 9) is measured there, never in Debug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "build type" FORCE)
endif()
# No -ffp-contract requirement: the new engine allows fused multiply-add (spec BD-7).

# ---- the engine -------------------------------------------------------------------------
# Empty until the first stage lands (spec BD-8). The one source file exists because a static
# library needs one; stages add their files here, one concept per file (BD-1).
add_library(horde_engine2 STATIC
  core/api/engine.cpp)
target_include_directories(horde_engine2 PUBLIC core)
if(NOT MSVC)
  target_compile_options(horde_engine2 PRIVATE -Wall -Wextra)
endif()

# ---- the suite --------------------------------------------------------------------------
# Every tests/items/*.cpp is an item that registers itself (tests/suite.h); a new item needs no
# edit here. CONFIGURE_DEPENDS makes the build notice a new file without a manual reconfigure.
enable_testing()
file(GLOB ENGINE2_SUITE_ITEMS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/items/*.cpp)
add_executable(horde_engine2_suite tests/runner.cpp ${ENGINE2_SUITE_ITEMS})
target_link_libraries(horde_engine2_suite PRIVATE horde_engine2)
target_include_directories(horde_engine2_suite PRIVATE tests)
add_test(NAME suite COMMAND horde_engine2_suite)

# The runner's own must-fail control: the same runner with one item that fails must exit nonzero.
# Without it an empty suite and a runner that cannot fail look the same.
add_executable(horde_engine2_runner_control tests/runner.cpp tests/controls/failing_item.cpp)
target_include_directories(horde_engine2_runner_control PRIVATE tests)
add_test(NAME runner_control COMMAND horde_engine2_runner_control)
set_tests_properties(runner_control PROPERTIES WILL_FAIL TRUE)

# ---- the WebAssembly build ----------------------------------------------------------------
# A stub that refuses to configure: see wasm/README.md.
option(HORDE_ENGINE2_WASM "build the WebAssembly target (not available until a toolchain version is approved)" OFF)
if(HORDE_ENGINE2_WASM)
  add_subdirectory(wasm)
endif()
