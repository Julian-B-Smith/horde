# The slot library's own test build (ROADMAP B714): rack_test against every module modules.json names (the test-only
# ones included), clean and once per plant. Configured and run by tools/rack_check.py in ./verify full; it needs the
# sibling trees at their pins and FOUNDATIONS, and no engine.
#   HORDE_RACK_CHOC_DIR       a directory holding choc/ (rack_check.py passes the repo's libs/choc)
#   HORDE_RACK_LEDGERS        the ledger files (rack_check.py passes the test plug-in's and the test module's)
#   HORDE_RACK_TEST_MODULES   the modules.json keys to host (default: all of them)
#   HORDE_RACK_RTSAN          ON: the real-time build (rack_rtsan_probe under -fsanitize=realtime), nothing else
cmake_minimum_required(VERSION 3.20)
project(horde_rack_tests CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
# 13.3: Scape writes numbers with std::to_chars for double (see tools/engine-preview/CMakeLists.txt).
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/rack)
if(NOT HORDE_RACK_TEST_MODULES)
  string(JSON n LENGTH "${HORDE_RACK_MODULES_JSON}" modules)
  math(EXPR last "${n} - 1")
  foreach(i RANGE ${last})
    string(JSON k GET "${HORDE_RACK_MODULES_JSON}" modules ${i} key)
    list(APPEND HORDE_RACK_TEST_MODULES ${k})
  endforeach()
endif()

option(HORDE_RACK_RTSAN "the RealtimeSanitizer build of the rack's audio path" OFF)
if(HORDE_RACK_RTSAN)
  # Every module's DSP is compiled by this compiler too, so a call it makes on the audio thread is seen, not assumed.
  horde_rack_library(rack_rt)
  horde_rack_registry(reg_rt RACK rack_rt MODULES ${HORDE_RACK_TEST_MODULES} LEDGERS ${HORDE_RACK_LEDGERS})
  add_executable(rack_rtsan_probe rack_rtsan_probe.cpp)
  target_link_libraries(rack_rtsan_probe PRIVATE reg_rt)
  target_compile_options(rack_rtsan_probe PRIVATE -fsanitize=realtime -O1 -g -fno-omit-frame-pointer)
  target_link_options(rack_rtsan_probe PRIVATE -fsanitize=realtime)
  return()
endif()

horde_rack_library(rack_clean)
horde_rack_registry(reg_clean RACK rack_clean MODULES ${HORDE_RACK_TEST_MODULES} LEDGERS ${HORDE_RACK_LEDGERS})
add_executable(rack_test rack_test.cpp)
target_link_libraries(rack_test PRIVATE reg_clean)

# The must-fail builds, one fault each (rack.cpp, chunk.cpp):
#   off-keeps-gains  an off slot keeps its I/O gains in the path (RS-4.8 as it stood; the +-6 dB checks)
#   deaf             the slot passes its input and never runs its module
#   no-values        the host's values never reach the module (it plays its defaults; the old reverb-deaf plant's fault)
#   idle-cut         the idle path cuts every slot, tails and all (the old reverb-idle plant's fault)
#   level-cut        a slot stops once its output is quiet (under 1e-7, the old filter's rung_out), not exactly silent
#   drop-slot-state  a save forgets the first slot's record (the round trip)
#   ignore-legacy    the legacy reader consumes version 5 and 6 lines and drops what they say (the migration)
foreach(plant OFF_KEEPS_GAINS DEAF NO_VALUES IDLE_CUT LEVEL_CUT DROP_SLOT_STATE IGNORE_LEGACY)
  string(TOLOWER ${plant} name)
  string(REPLACE "_" "-" name ${name})
  horde_rack_library(rack_${name} DEFINES HORDE_RACK_PLANT_${plant})
  horde_rack_registry(reg_${name} RACK rack_${name} MODULES ${HORDE_RACK_TEST_MODULES} LEDGERS ${HORDE_RACK_LEDGERS})
  add_executable(rack_test_${name} rack_test.cpp)
  target_link_libraries(rack_test_${name} PRIVATE reg_${name})
endforeach()
