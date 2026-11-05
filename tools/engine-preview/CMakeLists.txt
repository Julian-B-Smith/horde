# horde ENGINE PREVIEW (B624): a throwaway plug-in around the clean-room engine, for playing it in Live. A project of
# its own on purpose: the main build, its gates and its plug-in identity don't change, and it configures only where the
# engine's export exists. Use build.sh, which exports the engine from a named commit, never the builder's working tree.
#   HORDE_ENGINE2_DIR   the exported engine2/ directory (required)
#   HORDE_LIBS_DIR      libs/ with clap and clap-wrapper (default: this repo's)
cmake_minimum_required(VERSION 3.20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
project(horde_engine_preview VERSION 0.0.1 LANGUAGES C CXX OBJC OBJCXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)   # Debug is -O0, and its CPU figures would mean nothing
endif()
if(NOT HORDE_ENGINE2_DIR)
  message(FATAL_ERROR "set HORDE_ENGINE2_DIR to an exported engine2/ (see build.sh)")
endif()
set(HORDE_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(HORDE_LIBS_DIR ${HORDE_REPO_ROOT}/libs CACHE PATH "libs/ with clap and clap-wrapper")

# The same pinned SDK commits as the main build's CMakeLists.txt, and for the same reason: clap-wrapper's own download
# names them by git tag, which its owner can move. Bumping one means bumping it there too.
include(FetchContent)
FetchContent_Declare(horde_vst3sdk
  GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
  GIT_TAG 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0   # v3.8.0_build_66
  GIT_SUBMODULES base public.sdk pluginterfaces cmake
  SOURCE_SUBDIR populate-only)
FetchContent_MakeAvailable(horde_vst3sdk)
set(VST3_SDK_ROOT ${horde_vst3sdk_SOURCE_DIR})
FetchContent_Declare(horde_audiounitsdk
  GIT_REPOSITORY https://github.com/apple/AudioUnitSDK.git
  GIT_TAG 53a9a2008aae7fb1b0a9f093dd523b9b12f6c0d9   # AudioUnitSDK-1.1.0
  SOURCE_SUBDIR populate-only)
FetchContent_MakeAvailable(horde_audiounitsdk)
set(AUDIOUNIT_SDK_ROOT ${horde_audiounitsdk_SOURCE_DIR})
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
add_subdirectory(${HORDE_LIBS_DIR}/clap ${CMAKE_BINARY_DIR}/libs/clap EXCLUDE_FROM_ALL)
add_subdirectory(${HORDE_LIBS_DIR}/clap-wrapper ${CMAKE_BINARY_DIR}/libs/clap-wrapper EXCLUDE_FROM_ALL)

# EXCLUDE_FROM_ALL: the engine's suite and test builds stay unbuilt; only the two libraries below link here.
add_subdirectory(${HORDE_ENGINE2_DIR} ${CMAKE_BINARY_DIR}/engine2 EXCLUDE_FROM_ALL)

# The levelled bank, generated once per engine export (level_bank takes minutes, and a host can't wait for it).
add_executable(gen_bank gen_bank.cpp)
target_link_libraries(gen_bank PRIVATE horde_engine2_bank)
set(BANK_INC ${CMAKE_BINARY_DIR}/generated/bank_embedded.inc)
add_custom_command(OUTPUT ${BANK_INC}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
  COMMAND nice -n 10 $<TARGET_FILE:gen_bank> ${HORDE_ENGINE2_DIR}/data/bank/presets.json ${BANK_INC}
  DEPENDS gen_bank ${HORDE_ENGINE2_DIR}/data/bank/presets.json
  COMMENT "Levelling the bank")
add_custom_target(bank_embedded DEPENDS ${BANK_INC})

add_library(preview-impl STATIC preview_plugin.cpp)
add_dependencies(preview-impl bank_embedded)
target_include_directories(preview-impl PUBLIC ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2)
target_link_libraries(preview-impl PUBLIC clap clap-wrapper-extensions horde_engine2)

make_clapfirst_plugins(
  TARGET_NAME horde-engine-preview
  IMPL_TARGET preview-impl
  OUTPUT_NAME "horde-engine-preview"
  ENTRY_SOURCE "preview_entry.cpp"
  BUNDLE_IDENTIFIER "com.mind-lathe.horde.engine-preview"
  BUNDLE_VERSION ${PROJECT_VERSION}
  COPY_AFTER_BUILD FALSE
  PLUGIN_FORMATS CLAP VST3 AUV2
  ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/assets
  AUV2_MANUFACTURER_NAME "Mindlathe"
  AUV2_MANUFACTURER_CODE "LfTk"
  AUV2_SUBTYPE_CODE "HsEp"   # its own, so Live can't confuse it with horde (Hsaw) or horde-test (HsTs)
  AUV2_INSTRUMENT_TYPE "aumu")

# macOS: the wrapper leaves a signature that doesn't verify, and Live's scanner skips such a bundle without a log line.
foreach(fmt clap vst3 auv2)
  add_custom_command(TARGET horde-engine-preview_${fmt} POST_BUILD
    COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:horde-engine-preview_${fmt}>" VERBATIM)
endforeach()

# The smoke test's must-fail build: the same shell with its routes removed. A bare CLAP, never installed.
add_library(preview-plant MODULE preview_plugin.cpp preview_entry.cpp)
add_dependencies(preview-plant bank_embedded)
target_compile_definitions(preview-plant PRIVATE HORDE_PREVIEW_PLANT_NO_ROUTES)
target_include_directories(preview-plant PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2)
target_link_libraries(preview-plant PRIVATE clap horde_engine2)
set_target_properties(preview-plant PROPERTIES PREFIX "" SUFFIX ".clap")

add_executable(preview-smoke smoke.cpp)
add_dependencies(preview-smoke bank_embedded)
target_include_directories(preview-smoke PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2)
target_link_libraries(preview-smoke PRIVATE clap horde_engine2_bank)
