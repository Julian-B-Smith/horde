# The hosted-module admission harness (ROADMAP B561, ADR-295). Configured and built by
# tools/admission_check.py into horde's build-admission/ directory, never by the root
# CMakeLists.txt: a module's sources live in a sibling tree that CI and most clones do not have.
#
# THE MODULE IS BUILT BY ITS OWN BUILD. Scape's core/CMakeLists.txt is added as a subdirectory
# (EXCLUDE_FROM_ALL: only its scape_core library is built, because the harness links it), so its
# arithmetic flags are Scape's own (-ffp-contract=fast since its Q-026): the module admitted is
# the module that ships. Its binary directory is under this build's, so nothing is written into
# the sibling tree.
#
# The harness itself carries no flags of its own beyond CMake's Release defaults. The one
# exception is ADMISSION_RTSAN (row A10): the sanitizer build, configured with a RealtimeSanitizer
# compiler, adds -fsanitize=realtime to the harness and its link.
cmake_minimum_required(VERSION 3.20)
project(horde_admission CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED HORDE_SCAPE_DIR)
  message(FATAL_ERROR "set HORDE_SCAPE_DIR to a Scape tree (tools/admission_check.py does)")
endif()
add_subdirectory(${HORDE_SCAPE_DIR}/core ${CMAKE_BINARY_DIR}/scape EXCLUDE_FROM_ALL)

add_executable(admission_harness harness.cpp scape_adapter.cpp)
target_link_libraries(admission_harness PRIVATE scape_core)

option(ADMISSION_RTSAN "Build the harness under RealtimeSanitizer (row A10)" OFF)
if(ADMISSION_RTSAN)
  target_compile_definitions(admission_harness PRIVATE ADMISSION_RTSAN=1)
  # -O1 -g and frame pointers: the reported stacks name the module's source lines
  target_compile_options(admission_harness PRIVATE -fsanitize=realtime -O1 -g -fno-omit-frame-pointer)
  target_link_options(admission_harness PRIVATE -fsanitize=realtime)
endif()
