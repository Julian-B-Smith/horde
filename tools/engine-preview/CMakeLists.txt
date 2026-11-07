# horde ENGINE PREVIEW (B624): a throwaway plug-in around the clean-room engine, for playing it in Live. A project of
# its own on purpose: the main build, its gates and its plug-in identity don't change, and it configures only where the
# engine's export exists. Use build.sh, which exports the engine from a named commit, never the builder's working tree.
#   HORDE_ENGINE2_DIR   the exported engine2/ directory (required)
#   HORDE_LIBS_DIR      libs/ with clap, clap-wrapper and choc (default: this repo's)
#   HORDE_PREVIEW_WEBVIEW, HORDE_PREVIEW_PAGE   the window (below)
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

# GUI 3's page in a web view (B628) is the window; OFF builds the plain AppKit one instead, the fallback until the page
# has been tried in Live. HORDE_PREVIEW_PAGE names the one self-contained HTML file to embed: the stand-in by default,
# or what tools/gui3_inline.py writes from docs/design/gui3.
option(HORDE_PREVIEW_WEBVIEW "host GUI 3's page in a web view (OFF: the AppKit window)" ON)
set(HORDE_PREVIEW_PAGE ${CMAKE_CURRENT_SOURCE_DIR}/standin_page.html CACHE FILEPATH "the page the web view shows")

# choc as the main build compiles it: the vendored copy with libs/patches applied, never the bare submodule (the root
# CMakeLists.txt's HS_CHOC_PATCHES, in the same order; tools/choc_patch_check.py guards that list). The patches touch
# only the Windows backend, so on macOS they change nothing; using the same copy keeps one choc in the repo.
set(PREVIEW_CHOC_DIR ${CMAKE_BINARY_DIR}/choc-patched)
set(PREVIEW_CHOC_PATCHES
    ${HORDE_REPO_ROOT}/libs/patches/choc-webview2-navigation.patch
    ${HORDE_REPO_ROOT}/libs/patches/choc-webview2-permissions.patch
    ${HORDE_REPO_ROOT}/libs/patches/choc-webview2-external-drop.patch)
execute_process(COMMAND ${CMAKE_COMMAND} -DSRC=${HORDE_LIBS_DIR}/choc/choc -DOUT=${PREVIEW_CHOC_DIR} "-DPATCH=${PREVIEW_CHOC_PATCHES}"
                        -P ${HORDE_REPO_ROOT}/libs/patches/apply_patch.cmake
                RESULT_VARIABLE PREVIEW_CHOC_RC)
if(NOT PREVIEW_CHOC_RC EQUAL 0)
  message(FATAL_ERROR "choc patch not applied (see the error above)")
endif()

# The page as a byte array, through the same tool the main build embeds its GUI with: it also gives the page a policy
# that runs only the page's own inline scripts and loads nothing from anywhere, and refuses a page that would break it.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PAGE_H ${CMAKE_BINARY_DIR}/generated/preview_page.h)
add_custom_command(OUTPUT ${PAGE_H}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
  COMMAND ${Python3_EXECUTABLE} ${HORDE_REPO_ROOT}/tools/embed_file.py ${HORDE_PREVIEW_PAGE} ${PAGE_H} kPreviewPage
  DEPENDS ${HORDE_PREVIEW_PAGE} ${HORDE_REPO_ROOT}/tools/embed_file.py
  COMMENT "Embedding the page")
add_custom_target(preview_page DEPENDS ${PAGE_H})

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

if(HORDE_PREVIEW_WEBVIEW)
  set(PREVIEW_WINDOW preview_webview.mm)
  set(PREVIEW_WINDOW_LIBS "-framework AppKit" "-framework WebKit")
else()
  set(PREVIEW_WINDOW preview_gui.mm)
  set(PREVIEW_WINDOW_LIBS "-framework AppKit")
endif()
add_library(preview-impl STATIC preview_plugin.cpp bridge_core.cpp morph_core.cpp ${PREVIEW_WINDOW})
# build.sh passes which engine commit and which build this is; the window, the log and the description show it.
set(HORDE_PREVIEW_BUILD_LABEL "engine unknown, build 0" CACHE STRING "")
target_compile_definitions(preview-impl PUBLIC "HORDE_PREVIEW_BUILD_LABEL=\"${HORDE_PREVIEW_BUILD_LABEL}\"")
add_dependencies(preview-impl bank_embedded preview_page)
target_include_directories(preview-impl PUBLIC ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
target_link_libraries(preview-impl PUBLIC clap clap-wrapper-extensions horde_engine2 ${PREVIEW_WINDOW_LIBS})

make_clapfirst_plugins(
  TARGET_NAME horde-engine-preview
  IMPL_TARGET preview-impl
  OUTPUT_NAME "horde-engine-preview"
  ENTRY_SOURCE "preview_entry.cpp"
  BUNDLE_IDENTIFIER "com.mindlathe.horde-engine-preview"
  BUNDLE_VERSION ${PROJECT_VERSION}
  COPY_AFTER_BUILD FALSE
  PLUGIN_FORMATS CLAP VST3 AUV2
  ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/assets
  AUV2_MANUFACTURER_NAME "Mindlathe"
  AUV2_MANUFACTURER_CODE "LfTk"
  AUV2_SUBTYPE_CODE "HsEp"   # its own, so Live can't confuse it with any other horde AU
  AUV2_INSTRUMENT_TYPE "aumu")

# macOS: the wrapper leaves a signature that doesn't verify, and Live's scanner skips such a bundle without a log line.
foreach(fmt clap vst3 auv2)
  add_custom_command(TARGET horde-engine-preview_${fmt} POST_BUILD
    COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:horde-engine-preview_${fmt}>" VERBATIM)
endforeach()

# The smoke test's must-fail builds, bare CLAPs never installed. They keep the AppKit window whatever
# HORDE_PREVIEW_WEBVIEW says, so every build compiles the fallback and it can't rot unseen.
#   preview-plant        the routes removed, the retired Voices limit of 4 put back, pitch bends dropped, the output tap
#                        frozen after its first block, a state chunk that forgets the morph (B636), and the
#                        resolver's morph levels dropped, so no source fades (B639)
#   preview-plant-map    the bridge's key map shifted one row, and the output tap hearing silence
#   preview-plant-morph  the pad's position dropped, from the host or the page (B636, B639), alone: every other plant here changes something
#                        the morph's checks also read, which would let its control fail for the wrong reason
foreach(plant preview-plant preview-plant-map preview-plant-morph)
  add_library(${plant} MODULE preview_plugin.cpp bridge_core.cpp morph_core.cpp preview_gui.mm preview_entry.cpp)
  add_dependencies(${plant} bank_embedded)
  target_compile_definitions(${plant} PRIVATE "HORDE_PREVIEW_BUILD_LABEL=\"${HORDE_PREVIEW_BUILD_LABEL}\"")
  target_include_directories(${plant} PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
  target_link_libraries(${plant} PRIVATE clap horde_engine2 "-framework AppKit")
  set_target_properties(${plant} PROPERTIES PREFIX "" SUFFIX ".clap")
endforeach()
target_compile_definitions(preview-plant PRIVATE HORDE_PREVIEW_PLANT_NO_ROUTES HORDE_PREVIEW_PLANT_VOICE_CAP
                           HORDE_PREVIEW_PLANT_DROP_BEND HORDE_PREVIEW_PLANT_FROZEN_TAP HORDE_PREVIEW_PLANT_STATE_NO_MORPH
                           HORDE_PREVIEW_PLANT_DROP_LEVEL)
target_compile_definitions(preview-plant-map PRIVATE HORDE_PREVIEW_PLANT_KEYMAP_SHIFT HORDE_PREVIEW_PLANT_SILENT_TAP)
target_compile_definitions(preview-plant-morph PRIVATE HORDE_PREVIEW_PLANT_DROP_POSITION)

add_executable(preview-smoke smoke.cpp morph_core.cpp)
add_dependencies(preview-smoke bank_embedded)
target_include_directories(preview-smoke PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2)
target_link_libraries(preview-smoke PRIVATE clap horde_engine2_bank)

# Opens the real window and moves its real controls: nothing else can read a window before the human opens it.
add_executable(preview-gui-shot gui_shot.mm)
target_include_directories(preview-gui-shot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(preview-gui-shot PRIVATE clap "-framework AppKit" "-framework WebKit")

# The bridge's pure half, headless (section 7's rows). It links the engine only for its manifest.
add_executable(preview-bridge-test bridge_test.cpp bridge_core.cpp morph_core.cpp)
target_include_directories(preview-bridge-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PREVIEW_CHOC_DIR})
target_link_libraries(preview-bridge-test PRIVATE horde_engine2)

# The recorded frames GUI 3's ReplayTransport plays (section 7): a fixed chord through the built plug-in's bridge.
add_executable(preview-frames-dump frames_dump.cpp)
add_dependencies(preview-frames-dump bank_embedded)
target_include_directories(preview-frames-dump PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(preview-frames-dump PRIVATE clap horde_engine2_bank)
