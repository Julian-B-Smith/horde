# The hosted-module admission harness (ROADMAP B561, ADR-295). Configured and built by
# tools/admission_check.py into horde's build-admission/ directory, never by the root
# CMakeLists.txt: a module's sources live in a sibling tree that CI and most clones do not have.
#
# THE MODULE IS BUILT BY ITS OWN BUILD. Scape's core/CMakeLists.txt is added as a subdirectory
# (EXCLUDE_FROM_ALL: only its scape_core library is built, because the harness links it), so its
# arithmetic flags are Scape's own (-ffp-contract=fast since its Q-026): the module admitted is
# the module that ships. Kiln's is added the same way, and its kiln_core carries Kiln's own
# PRIVATE -ffp-contract=off (Kiln D-007 item 4). Each binary directory is under this build's, so
# nothing is written into a sibling tree.
#
# ONE BINARY, EITHER OR BOTH TREES. HORDE_SCAPE_DIR builds the Scape adapter, HORDE_KILN_DIR the
# Kiln adapter (the EQ and the drive, one file, one build); at least one is required.
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

if(NOT DEFINED HORDE_SCAPE_DIR AND NOT DEFINED HORDE_KILN_DIR)
  message(FATAL_ERROR "set HORDE_SCAPE_DIR and/or HORDE_KILN_DIR to a module tree (tools/admission_check.py does)")
endif()

add_executable(admission_harness harness.cpp)

if(DEFINED HORDE_SCAPE_DIR)
  add_subdirectory(${HORDE_SCAPE_DIR}/core ${CMAKE_BINARY_DIR}/scape EXCLUDE_FROM_ALL)
  target_sources(admission_harness PRIVATE scape_adapter.cpp)
  target_link_libraries(admission_harness PRIVATE scape_core)
  target_compile_definitions(admission_harness PRIVATE ADMISSION_SCAPE=1)
endif()

if(DEFINED HORDE_KILN_DIR)
  # module.h includes foundations/fx_operator.h. Scape's build supplies it through scape_core's
  # PUBLIC include (a copy Scape vendors); Kiln vendors none, and FOUNDATIONS is private, so its
  # headers are never copied into horde's tracked tree (the human's ruling of 2026-08-15, see the
  # root CMakeLists.txt). The driver locates a FOUNDATIONS tree and passes its include directory.
  if(NOT DEFINED HORDE_FOUNDATIONS_INCLUDE)
    message(FATAL_ERROR "the Kiln adapter needs foundations/fx_operator.h: set HORDE_FOUNDATIONS_INCLUDE (tools/admission_check.py does)")
  endif()
  add_subdirectory(${HORDE_KILN_DIR}/core ${CMAKE_BINARY_DIR}/kiln EXCLUDE_FROM_ALL)
  # The module admitted is the module that ships: if Kiln's own build stopped applying its
  # contraction flag, the DSP would be judged with a rounding it does not ship with. Kiln's flag
  # is PRIVATE and only on kiln_core (core/src), so this asserts it is there.
  get_target_property(kiln_opts kiln_core COMPILE_OPTIONS)
  string(FIND "${kiln_opts}" "-ffp-contract=off" kiln_fp)
  if(kiln_fp EQUAL -1)
    message(FATAL_ERROR "kiln_core is not built with Kiln's own -ffp-contract=off (Kiln D-007): the module admitted would not be the module that ships")
  endif()
  target_sources(admission_harness PRIVATE kiln_adapter.cpp)
  target_link_libraries(admission_harness PRIVATE kiln_core)
  target_include_directories(admission_harness PRIVATE ${HORDE_FOUNDATIONS_INCLUDE})
  target_compile_definitions(admission_harness PRIVATE ADMISSION_KILN=1)
endif()

option(ADMISSION_RTSAN "Build the harness under RealtimeSanitizer (row A10)" OFF)
if(ADMISSION_RTSAN)
  target_compile_definitions(admission_harness PRIVATE ADMISSION_RTSAN=1)
  # -O1 -g and frame pointers: the reported stacks name the module's source lines
  target_compile_options(admission_harness PRIVATE -fsanitize=realtime -O1 -g -fno-omit-frame-pointer)
  target_link_options(admission_harness PRIVATE -fsanitize=realtime)
endif()
