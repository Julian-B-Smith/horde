# horde ENGINE PREVIEW (B624): a throwaway plug-in around the clean-room engine, for playing it in Live. A project of
# its own on purpose: the main build, its gates and its plug-in identity don't change, and it configures only where the
# engine's export exists. Use build.sh, which exports the engine from a named commit, never the builder's working tree.
#   HORDE_ENGINE2_DIR   the exported engine2/ directory (required)
#   HORDE_LIBS_DIR      libs/ with clap, clap-wrapper and choc (default: this repo's)
#   HORDE_PREVIEW_WEBVIEW, HORDE_PREVIEW_PAGE   the window (below)
#   each hosted module's tree, read only, and HORDE_FOUNDATIONS_DIR: the variables tools/rack/modules.json names (below)
cmake_minimum_required(VERSION 3.20)
# 13.3, not 11.0 as before B710: a hosted module's core (B710's) writes numbers with std::to_chars for double (no locale,
# no allocation, any thread), which libc++ offers from macOS 13.3. Lowering only that module's target would leave a
# bundle claiming 11.0 that can't load there; this says what the bundle needs.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "")
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

# The rack (B714): the generic module slot, tools/rack, and every module tools/rack/modules.json hands the default
# plug-in (its non-test entries), each from its pin: a tree at exactly its .pin's commit with its clean paths as that
# commit has them, or the configure stops (the module played is the module admitted), and FOUNDATIONS pinned by tree
# (tools/rack/pins/foundations.pin), its fx_operator.h byte-identical to every copy a module vendors. tools/rack/CMakeLists.txt says
# how. Nothing here names a module.
set(HORDE_RACK_CHOC_DIR ${PREVIEW_CHOC_DIR})
add_subdirectory(${HORDE_REPO_ROOT}/tools/rack ${CMAKE_BINARY_DIR}/rack)
set(PREVIEW_LEDGERS ${CMAKE_CURRENT_SOURCE_DIR}/host_ids.lock.json)
horde_rack_library(preview_rack)
horde_rack_registry(preview_rack_modules RACK preview_rack MODULES ${HORDE_RACK_DEFAULT_MODULES} LEDGERS ${PREVIEW_LEDGERS})
# The rack with no module at all: "the build as it was", the reference every off-is-bit-identical check is held to.
horde_rack_registry(preview_rack_none RACK preview_rack MODULES LEDGERS ${PREVIEW_LEDGERS})

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

# The levelled bank, generated once per engine export (level_bank takes minutes, and a host can't wait for it), and
# B645's demo bank after it (demo_bank.json: four corners and the morph's settings per preset).
add_executable(gen_bank gen_bank.cpp demo_bank.cpp morph_core.cpp)
target_include_directories(gen_bank PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PREVIEW_CHOC_DIR})
target_link_libraries(gen_bank PRIVATE horde_engine2_bank)
set(BANK_INC ${CMAKE_BINARY_DIR}/generated/bank_embedded.inc)
add_custom_command(OUTPUT ${BANK_INC}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
  COMMAND nice -n 10 $<TARGET_FILE:gen_bank> ${HORDE_ENGINE2_DIR}/data/bank/presets.json ${CMAKE_CURRENT_SOURCE_DIR}/demo_bank.json ${BANK_INC}
  DEPENDS gen_bank ${HORDE_ENGINE2_DIR}/data/bank/presets.json ${CMAKE_CURRENT_SOURCE_DIR}/demo_bank.json
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
target_link_libraries(preview-impl PUBLIC clap clap-wrapper-extensions horde_engine2 preview_rack_modules ${PREVIEW_WINDOW_LIBS})

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
#                        resolver's morph levels dropped, so no source fades (B639), and overruns counted but never
#                        recorded as lines in the session log (B642), and a demo's corners B to D never loaded (B645)
#   preview-plant-map    the bridge's key map shifted one row (the engine's keys and, since B711, the test rows' too), the
#                        output tap hearing silence, and a load filling corner A only, so a factory preset after a demo
#                        keeps the demo's other corners (B645)
#   preview-plant-morph  the pad's position dropped, from the host or the page (B636, B639), alone: every other plant here changes something
#                        the morph's checks also read, which would let its control fail for the wrong reason
#   preview-plant-norack  the rack with no module at all, alone: "the build as it was", the reference every preset must
#                        match bit for bit with every slot off (B714; it stands in for B660's and B710's no-module builds)
#   preview-plant-rack-idle  the idle engine's skip (OU-4) cuts every slot's tail, alone: the control of the tail checks
#                        (B714; B710's reverb-idle plant, generic)
#   preview-plant-silent-refusal  a refused chunk says nothing (no log line, no `load` member), alone: the control of the
#                        loud refusal (B714, acceptance (6))
#   preview-plant-droptest  the page's sets of the test rows (B711) dropped, alone: their descriptors, gestures and host
#                        parameters are all there, so the checks that a page set reaches the host fail for that reason only
foreach(plant preview-plant preview-plant-map preview-plant-morph preview-plant-norack preview-plant-rack-idle
              preview-plant-silent-refusal preview-plant-droptest)
  add_library(${plant} MODULE preview_plugin.cpp bridge_core.cpp morph_core.cpp preview_gui.mm preview_entry.cpp)
  add_dependencies(${plant} bank_embedded)
  target_compile_definitions(${plant} PRIVATE "HORDE_PREVIEW_BUILD_LABEL=\"${HORDE_PREVIEW_BUILD_LABEL}\"")
  target_include_directories(${plant} PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
  target_link_libraries(${plant} PRIVATE clap horde_engine2 "-framework AppKit")
  if(plant STREQUAL "preview-plant-norack")
    target_link_libraries(${plant} PRIVATE preview_rack_none)
  else()
    target_link_libraries(${plant} PRIVATE preview_rack_modules)
  endif()
  set_target_properties(${plant} PROPERTIES PREFIX "" SUFFIX ".clap")
endforeach()
target_compile_definitions(preview-plant PRIVATE HORDE_PREVIEW_PLANT_NO_ROUTES HORDE_PREVIEW_PLANT_VOICE_CAP
                           HORDE_PREVIEW_PLANT_DROP_BEND HORDE_PREVIEW_PLANT_FROZEN_TAP HORDE_PREVIEW_PLANT_STATE_NO_MORPH
                           HORDE_PREVIEW_PLANT_DROP_LEVEL HORDE_PREVIEW_PLANT_NO_OVERRUN_LINES HORDE_PREVIEW_PLANT_DEMO_ONE_CORNER)
target_compile_definitions(preview-plant-map PRIVATE HORDE_PREVIEW_PLANT_KEYMAP_SHIFT HORDE_PREVIEW_PLANT_SILENT_TAP
                           HORDE_PREVIEW_PLANT_DEMO_KEEPS_CORNERS)
target_compile_definitions(preview-plant-morph PRIVATE HORDE_PREVIEW_PLANT_DROP_POSITION)
target_compile_definitions(preview-plant-rack-idle PRIVATE HORDE_PREVIEW_PLANT_RACK_IDLE_CUT)
target_compile_definitions(preview-plant-silent-refusal PRIVATE HORDE_PREVIEW_PLANT_SILENT_REFUSAL)
target_compile_definitions(preview-plant-droptest PRIVATE HORDE_PREVIEW_PLANT_DROP_TEST_SETS)

# B714, acceptance (1): the TEST CONFIGURATION. The same preview_plugin.cpp and bridge_core.cpp, with the registry of
# every modules.json entry (its test-only ones too: a module added by data alone) and the test module's own ledger block
# beside the plug-in's. Never installed, never the plug-in. preview-plant-module-branch is the same build with one
# `if (module == ...)` planted in state_save; the module it names is the first test-only entry's, read here from
# modules.json, never written in the source. smoke runs the generated rack checks on both: the first passes, the
# second must fail.
file(GLOB PREVIEW_TEST_LEDGER_FILES ${HORDE_REPO_ROOT}/tools/rack/test/host_ids*.json)   # a test module's own id block
set(PREVIEW_TEST_LEDGERS ${PREVIEW_LEDGERS} ${PREVIEW_TEST_LEDGER_FILES})
set(PREVIEW_ALL_MODULES "")
set(PREVIEW_BRANCH_MODULE "")
string(JSON preview_n LENGTH "${HORDE_RACK_MODULES_JSON}" modules)
math(EXPR preview_last "${preview_n} - 1")
foreach(i RANGE ${preview_last})
  string(JSON k GET "${HORDE_RACK_MODULES_JSON}" modules ${i} key)
  string(JSON only GET "${HORDE_RACK_MODULES_JSON}" modules ${i} test_only)
  list(APPEND PREVIEW_ALL_MODULES ${k})
  if(only AND PREVIEW_BRANCH_MODULE STREQUAL "")
    string(JSON PREVIEW_BRANCH_MODULE GET "${HORDE_RACK_MODULES_JSON}" modules ${i} module)
  endif()
endforeach()
horde_rack_registry(preview_rack_testmods RACK preview_rack MODULES ${PREVIEW_ALL_MODULES} LEDGERS ${PREVIEW_TEST_LEDGERS})
foreach(plant preview-testmods preview-plant-module-branch)
  add_library(${plant} MODULE preview_plugin.cpp bridge_core.cpp morph_core.cpp preview_gui.mm preview_entry.cpp)
  add_dependencies(${plant} bank_embedded)
  target_compile_definitions(${plant} PRIVATE "HORDE_PREVIEW_BUILD_LABEL=\"${HORDE_PREVIEW_BUILD_LABEL}, test configuration\"")
  target_include_directories(${plant} PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
  target_link_libraries(${plant} PRIVATE clap horde_engine2 preview_rack_testmods "-framework AppKit")
  set_target_properties(${plant} PROPERTIES PREFIX "" SUFFIX ".clap")
endforeach()
target_compile_definitions(preview-plant-module-branch PRIVATE "HORDE_PREVIEW_PLANT_MODULE_BRANCH=\"${PREVIEW_BRANCH_MODULE}\"")

add_executable(preview-smoke smoke.cpp morph_core.cpp)
add_dependencies(preview-smoke bank_embedded)
# choc for its JSON reader alone (B711): the hello reply's test-row descriptors are read as JSON, not by string search.
target_include_directories(preview-smoke PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
target_link_libraries(preview-smoke PRIVATE clap horde_engine2_bank)

# Opens the real window and moves its real controls: nothing else can read a window before the human opens it.
add_executable(preview-gui-shot gui_shot.mm)
target_include_directories(preview-gui-shot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(preview-gui-shot PRIVATE clap "-framework AppKit" "-framework WebKit")

# The bridge's pure half, headless (section 7's rows). It links the engine only for its manifest, and the rack's registry
# for its slot table (the rack's keys on the page, the chunk's fx section and its migration).
add_executable(preview-bridge-test bridge_test.cpp bridge_core.cpp morph_core.cpp)
target_include_directories(preview-bridge-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
target_link_libraries(preview-bridge-test PRIVATE horde_engine2 preview_rack_modules)
target_compile_definitions(preview-bridge-test PRIVATE "HORDE_PREVIEW_FIXTURES=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures\"")

# The recorded frames GUI 3's ReplayTransport plays (section 7): a fixed chord through the built plug-in's bridge.
add_executable(preview-frames-dump frames_dump.cpp)
add_dependencies(preview-frames-dump bank_embedded)
target_include_directories(preview-frames-dump PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(preview-frames-dump PRIVATE clap horde_engine2_bank)

# B642's drop-out probe: holds a chord while the pad moves over four corners, times every block and looks for gaps.
# preview-morph-wholespan is its measurement build: the morph's events ride in the host's whole span (not cut at every
# control interval), so the probe's --pair can time the two side by side and hold their outputs bit for bit (DT-2).
add_executable(preview-morph-probe morph_probe.cpp)
add_library(preview-morph-wholespan MODULE preview_plugin.cpp bridge_core.cpp morph_core.cpp preview_gui.mm preview_entry.cpp)
add_dependencies(preview-morph-wholespan bank_embedded)
target_compile_definitions(preview-morph-wholespan PRIVATE "HORDE_PREVIEW_BUILD_LABEL=\"${HORDE_PREVIEW_BUILD_LABEL}\"" HORDE_PREVIEW_PROBE_WHOLE_SPANS)
target_include_directories(preview-morph-wholespan PRIVATE ${CMAKE_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
target_link_libraries(preview-morph-wholespan PRIVATE clap horde_engine2 preview_rack_modules "-framework AppKit")
set_target_properties(preview-morph-wholespan PROPERTIES PREFIX "" SUFFIX ".clap")
target_include_directories(preview-morph-probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(preview-morph-probe PRIVATE clap horde_engine2)
# B642's minimal reproduction of the silent class: Quantum's per-switch picks leave no source on (the resolver alone).
add_executable(preview-morph-hole morph_hole.cpp)
target_link_libraries(preview-morph-hole PRIVATE horde_engine2)

# B645's measurement of the demo bank, built on demand: renders every corner and a pad sweep through the engine and the
# clean room's resolver, writes WAVs and a listening page, and reports level, silence, DC, CPU and how far the corners
# differ. It reads demo_bank.json through the same reader as gen_bank, so it measures what the plug-in embeds.
add_executable(preview-demo-render demo_render.cpp demo_bank.cpp morph_core.cpp)
target_include_directories(preview-demo-render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${HORDE_REPO_ROOT}/h2 ${PREVIEW_CHOC_DIR})
target_link_libraries(preview-demo-render PRIVATE horde_engine2_bank)
