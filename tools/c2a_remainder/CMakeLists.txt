# B667 / CONTRACT 7 C2a: the remainder bench. Built by build.sh against one `git archive` of the clean-room engine
# (never its working tree); standalone, links the engine and its bank translator only (EXCLUDE_FROM_ALL keeps the engine's
# own suite out of this build).
cmake_minimum_required(VERSION 3.20)
project(c2a_remainder LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "build type" FORCE)   # PF-3: Release only
endif()
set(HORDE_ENGINE2_DIR "" CACHE PATH "the exported engine2/ directory")
if(NOT EXISTS "${HORDE_ENGINE2_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "HORDE_ENGINE2_DIR must name an exported engine2/ (build.sh does this)")
endif()
add_subdirectory("${HORDE_ENGINE2_DIR}" engine EXCLUDE_FROM_ALL)
add_executable(c2a_remainder main.cpp)
target_link_libraries(c2a_remainder PRIVATE horde_engine2_bank)
target_include_directories(c2a_remainder PRIVATE "${HORDE_ENGINE2_DIR}/tests" "${HORDE_ENGINE2_DIR}/tests/harness" "${HORDE_ENGINE2_DIR}/tests/cpu")
target_compile_definitions(c2a_remainder PRIVATE HORDE_HAVE_BANK_TRANSLATOR HORDE_ENGINE2_DATA_DIR="${HORDE_ENGINE2_DIR}/data"
  HORDE_ENGINE2_FIXTURE_DIR="${HORDE_ENGINE2_DIR}/tests/fixtures" HORDE_ENGINE2_TOOLS_DIR="${HORDE_ENGINE2_DIR}/tools")
