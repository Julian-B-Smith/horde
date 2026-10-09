# HYPERSAW — CLAP-native, VST3/AUv2 via clap-wrapper (ADR-002).
# Build (this machine — see global notes: Unix Makefiles only, absolute paths):
#   cmake -S . -B build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
#   cmake --build <abs>/build-release -j
# Perf work happens in Release only (Debug is -O0 and meaningless for ceilings).
cmake_minimum_required(VERSION 3.27)
project(HYPERSAW VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Windows: static MSVC runtime project-wide. clap-wrapper sets this for its own
# directory scope only, so without the root-level setting our targets get /MD
# and its libs get /MT — LNK2038 at link. Static runtime is also what you ship
# in a plugin (no VC redist dependency).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(APPLE)
  enable_language(OBJC OBJCXX)
  # Apple Silicon + Intel; hosts are arm64-first but Live still ships x86_64 users.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# Pinned SDKs live as submodules (libs/clap @ 1.2.10, libs/clap-wrapper @ v0.15.1).
#
# The VST3 SDK and the AudioUnitSDK are fetched at configure time, pinned here by
# COMMIT (B446 Wave 2). clap-wrapper's own download names them by git TAG, and a
# tag is a pointer its owner can move, so those fetches are switched OFF and the
# wrapper is handed the roots below instead (base_sdks.cmake honours a provided
# *_SDK_ROOT before any download). OFF is also the fail-closed half: any SDK the
# wrapper would still want to download now stops the configure instead of
# fetching an unpinned copy. The VST3 SDK's own submodules are pinned by the
# gitlinks of the commit below. Bumping an SDK = change the hash here (and the
# tag comment), nothing else; tools/gen_sbom.py reads these two lines.
# SOURCE_SUBDIR names a directory with no CMakeLists.txt on purpose: populate the
# sources only, never add_subdirectory them (clap-wrapper compiles what it needs).
include(FetchContent)
set(HORDE_VST3_SDK_COMMIT 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0)         # v3.8.0_build_66
set(HORDE_AUDIOUNIT_SDK_COMMIT 53a9a2008aae7fb1b0a9f093dd523b9b12f6c0d9)    # AudioUnitSDK-1.1.0
FetchContent_Declare(horde_vst3sdk
  GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
  GIT_TAG ${HORDE_VST3_SDK_COMMIT}
  GIT_SUBMODULES base public.sdk pluginterfaces cmake
  SOURCE_SUBDIR populate-only)
FetchContent_MakeAvailable(horde_vst3sdk)
set(VST3_SDK_ROOT ${horde_vst3sdk_SOURCE_DIR})
if(APPLE)
  FetchContent_Declare(horde_audiounitsdk
    GIT_REPOSITORY https://github.com/apple/AudioUnitSDK.git
    GIT_TAG ${HORDE_AUDIOUNIT_SDK_COMMIT}
    SOURCE_SUBDIR populate-only)
  FetchContent_MakeAvailable(horde_audiounitsdk)
  set(AUDIOUNIT_SDK_ROOT ${horde_audiounitsdk_SOURCE_DIR})
endif()
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/clap EXCLUDE_FROM_ALL)
add_subdirectory(libs/clap-wrapper EXCLUDE_FROM_ALL)

# choc, PATCHED (B446, ADR-194 D-S5, ADR-196): the WebView2 backend gains an
# allowNavigation option, which the Windows GUI uses to admit only its embedded
# page, and an allowPermission option, which it uses to refuse page script the
# clipboard. libs/choc stays the upstream pin; the patches are applied in list
# order at configure time to a copy in the build tree, and the configure STOPS
# if one no longer applies (libs/patches/README.md). HS_CHOC_PATCHES is the one
# list of them: tools/gen_sbom.py reads it for the SBOM's pedigree.patches. Every choc include in our code is therefore
# `<choc/...>` through HS_CHOC_DIR, never a relative path into libs/choc, which
# would compile the unpatched header; tools/choc_patch_check.py enforces that.
set(HS_CHOC_DIR ${CMAKE_CURRENT_BINARY_DIR}/choc-patched)
set(HS_CHOC_PATCHES
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/patches/choc-webview2-navigation.patch
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/patches/choc-webview2-permissions.patch)
# Quoted: unquoted, the list's `;` would split it into separate arguments.
execute_process(COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/libs/choc/choc
                        -DOUT=${HS_CHOC_DIR} "-DPATCH=${HS_CHOC_PATCHES}"
                        -P ${CMAKE_CURRENT_SOURCE_DIR}/libs/patches/apply_patch.cmake
                RESULT_VARIABLE HS_CHOC_RC)
if(NOT HS_CHOC_RC EQUAL 0)
  message(FATAL_ERROR "choc patch not applied (see the error above); refusing to configure")
endif()
# Re-run the copy-and-patch when anything it copies changes, not only the
# patched header: a choc bump that leaves choc_WebView.h alone still changes the
# copy. The submodule's HEAD file covers a bump that adds files the glob has not
# seen yet (a detached submodule's HEAD holds the pinned commit).
file(GLOB_RECURSE HS_CHOC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/choc/choc/*)
execute_process(COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR}/libs/choc rev-parse --path-format=absolute --git-path HEAD
                OUTPUT_VARIABLE HS_CHOC_HEAD OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(HS_CHOC_HEAD AND EXISTS "${HS_CHOC_HEAD}")
  list(APPEND HS_CHOC_FILES ${HS_CHOC_HEAD})
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             ${HS_CHOC_PATCHES} ${CMAKE_CURRENT_SOURCE_DIR}/libs/patches/apply_patch.cmake
             ${HS_CHOC_FILES})

# GUI HTML is embedded at build time (self-contained bundle, no file deps).
# find_package resolves the interpreter portably — bare `python3` is absent
# from Windows CI runners' PATH (same trap family as L0003).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
# HYPERSAW_GUI2: which interface gets embedded. ON since 2026-08-23 — gui2 is
# now the shipped interface and gui.html is the legacy one, on the human's call.
# The 2026-08-07 condition for the swap was "when it reaches parity", and it
# has: gui_reach measures gui2 at 142/159 declared params against gui.html's
# 102/159, and gui2 is the only one carrying the morph engine, the second
# oscillator, the mixer and the FX rack. Pass -DHYPERSAW_GUI2=OFF for the
# legacy single-column interface, which is kept building precisely so that
# escape hatch stays real rather than notional.
option(HYPERSAW_GUI2 "Embed gui2.html (default); OFF embeds the legacy gui.html" ON)
if(HYPERSAW_GUI2)
  set(GUI_HTML_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/gui2.html)
else()
  set(GUI_HTML_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/gui.html)
endif()
set(GUI_HTML_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/gui_html.h)
add_custom_command(
  OUTPUT ${GUI_HTML_HEADER}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.py
          ${GUI_HTML_SOURCE} ${GUI_HTML_HEADER} kGuiHtml
  DEPENDS ${GUI_HTML_SOURCE} tools/embed_file.py
  COMMENT "Embedding GUI: ${GUI_HTML_SOURCE}")

# FACTORY BANK (B129). Every docs/presets/factory/**/*.json embedded as one
# FactoryFile array; the shell copies them into the per-user store on first
# open. CONFIGURE_DEPENDS so ADDING a preset file re-runs the glob — a plain
# GLOB would ship a stale bank until someone happened to re-configure. The
# directory may be EMPTY or ABSENT (the bank content is B130, in flight): the
# glob then yields nothing, the generated array is inert, and the install is a
# no-op. Do not "fix" that into a hard dependency on the folder existing.
set(FACTORY_BANK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/presets/factory)
set(FACTORY_BANK_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/factory_bank.h)
file(GLOB_RECURSE FACTORY_BANK_FILES CONFIGURE_DEPENDS ${FACTORY_BANK_DIR}/*.json)
add_custom_command(
  OUTPUT ${FACTORY_BANK_HEADER}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.py
          --bank ${FACTORY_BANK_DIR} ${FACTORY_BANK_HEADER} kFactoryBank
  DEPENDS ${FACTORY_BANK_FILES} tools/embed_file.py
  COMMENT "Embedding factory bank: ${FACTORY_BANK_DIR}")

# clap-first idiom (see libs/clap-wrapper/tests/clap-first-example): a static
# impl lib carrying the factory, wrapped per-format by make_clapfirst_plugins.
# The webview GUI module (ADR-019) compiles on macOS only for now; elsewhere
# the shell exposes no gui extension and hosts use their generic editor.
# BUILD ID stamped into the GUI corner. "Is the plugin cached?" cost real
# debugging time twice (2026-07-30), and a hand-typed version can lie; this is
# read from git at CONFIGURE time and re-read whenever CMake reruns. Dirty tree
# gets a "+" so a local edit is never mistaken for the committed hash.
execute_process(COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse --short HEAD
                OUTPUT_VARIABLE HS_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE HS_GIT_RC)
if(NOT HS_GIT_RC EQUAL 0)
  set(HS_GIT_HASH "nogit")
endif()
execute_process(COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} status --porcelain
                OUTPUT_VARIABLE HS_GIT_DIRTY ERROR_QUIET)
if(NOT HS_GIT_DIRTY STREQUAL "")
  set(HS_GIT_HASH "${HS_GIT_HASH}+")
endif()

# Build stamp regenerated EVERY build (see tools/build_stamp.cmake for why a
# configure-time stamp is not good enough).
set(HS_STAMP_H ${CMAKE_CURRENT_BINARY_DIR}/generated/build_stamp.h)
add_custom_target(hypersaw_build_stamp ALL
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${HS_STAMP_H}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_stamp.cmake
  BYPRODUCTS ${HS_STAMP_H}
  COMMENT "stamping build id + time")

add_library(${PROJECT_NAME}-impl STATIC src/hypersaw_clap.cpp)
target_compile_definitions(${PROJECT_NAME}-impl PRIVATE HYPERSAW_BUILD_ID="${HS_GIT_HASH}")
add_dependencies(${PROJECT_NAME}-impl hypersaw_build_stamp)
# ${CMAKE_CURRENT_SOURCE_DIR}/src is load-bearing for the GENERATED headers:
# generated/factory_bank.h includes "gui/preset_store.h", and a quoted include
# from a generated file has no src/ to be relative to.
target_include_directories(${PROJECT_NAME}-impl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
                                                        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME}-impl PUBLIC clap clap-wrapper-extensions)
# PUBLIC: checks that link the impl and include choc must see the same patched
# header, or one binary would hold two definitions of WebView::Options.
target_include_directories(${PROJECT_NAME}-impl PUBLIC ${HS_CHOC_DIR})
if(APPLE)
  target_sources(${PROJECT_NAME}-impl PRIVATE src/gui/hypersaw_gui.mm ${GUI_HTML_HEADER}
                                              ${FACTORY_BANK_HEADER})
  target_include_directories(${PROJECT_NAME}-impl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_link_libraries(${PROJECT_NAME}-impl PUBLIC "-framework WebKit" "-framework Cocoa")
elseif(WIN32)
  # WebView2 via choc (COM declarations embedded — no external SDK).
  # CI-compile-verified; runtime validation is a recorded residual.
  target_sources(${PROJECT_NAME}-impl PRIVATE src/gui/hypersaw_gui_win.cpp ${GUI_HTML_HEADER}
                                              ${FACTORY_BANK_HEADER})
  target_include_directories(${PROJECT_NAME}-impl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_link_libraries(${PROJECT_NAME}-impl PUBLIC ole32 shlwapi version)
endif()

set(HYPERSAW_PLUGIN_FORMATS CLAP VST3 AUV2)

make_clapfirst_plugins(
        TARGET_NAME ${PROJECT_NAME}
        IMPL_TARGET ${PROJECT_NAME}-impl

        # The DEVICE is horde (ADR-114), and this is the name a host shows:
        # the AU browser entry, the VST3 bundle, the plugin the player sees.
        # Changed 2026-08-27 on the human's report that "the au is still called
        # hypersaw".
        #
        # AUV2_OUTPUT_NAME does NOT work as a narrower lever here: in the
        # target-based configuration clap-wrapper OVERWRITES it from the CLAP
        # target's LIBRARY_OUTPUT_NAME (wrap_auv2.cmake:100-101), so setting it
        # is inert. This is the only real control.
        #
        # SAFE FOR SAVED SESSIONS, because identity is not the name: the CLAP id
        # (`com.lifted-truck.hypersaw`, frozen) and the AU triple
        # (`aumu`/`Hsaw`/`LfTk`, frozen just above) are what a host stores, and
        # the VST3 UID derives from the CLAP id rather than the filename.
        # THE ONE REAL COST: this produces NEW bundles (horde.vst3,
        # horde.component). They do not replace HYPERSAW.vst3 / .component, so
        # the old pair must be deleted by hand or both show in the browser.
        OUTPUT_NAME "horde"

        ENTRY_SOURCE "src/hypersaw_clap_entry.cpp"

        BUNDLE_IDENTIFIER "com.lifted-truck.hypersaw"
        BUNDLE_VERSION ${PROJECT_VERSION}

        COPY_AFTER_BUILD FALSE

        PLUGIN_FORMATS ${HYPERSAW_PLUGIN_FORMATS}

        ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/HYPERSAW_assets

        AUV2_MANUFACTURER_NAME "Mindlathe"
        AUV2_MANUFACTURER_CODE "LfTk"
        # FROZEN, exactly like the CLAP id: `aumu`/`Hsaw`/`LfTk` is the triple a
        # host stores to re-find this AU in a saved session. The DISPLAY name may
        # change freely; these three may not.
        AUV2_SUBTYPE_CODE "Hsaw"
        AUV2_INSTRUMENT_TYPE "aumu"
)

# SWARM-FX: the effects product line as a standalone AUDIO-EFFECT plugin
# (Track E1.3). Its own impl/entry/factory — the instrument shell is untouched
# — sharing the parity-proven cores (filter_core.h / notch_core.h) headers-only.
# No webview GUI yet (generic host params); AUv2 type is 'aufx' (effect).
add_library(SWARMFX-impl STATIC src/swarmfx_clap.cpp)
target_link_libraries(SWARMFX-impl PUBLIC clap clap-wrapper-extensions)

make_clapfirst_plugins(
        TARGET_NAME SWARMFX
        IMPL_TARGET SWARMFX-impl
        OUTPUT_NAME "SWARM-FX"
        ENTRY_SOURCE "src/swarmfx_entry.cpp"
        BUNDLE_IDENTIFIER "com.lifted-truck.swarmfx"
        BUNDLE_VERSION ${PROJECT_VERSION}
        COPY_AFTER_BUILD FALSE
        PLUGIN_FORMATS ${HYPERSAW_PLUGIN_FORMATS}
        ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/SWARMFX_assets
        AUV2_MANUFACTURER_NAME "Mindlathe"
        AUV2_MANUFACTURER_CODE "LfTk"
        AUV2_SUBTYPE_CODE "Swfx"
        AUV2_INSTRUMENT_TYPE "aufx"
)

# L0-1 parity harness (host-side tool, not shipped): SwarmCore vs JS goldens.
add_executable(parity_check tools/parity_check.cpp)
if(NOT MSVC)
  target_compile_options(parity_check PRIVATE -O2)
endif()

# L0-2..5 + L0-13 trajectory suite (host-side tool, not shipped).
add_executable(trajectory_check tools/trajectory_check.cpp)
if(NOT MSVC)
  target_compile_options(trajectory_check PRIVATE -O2)
endif()

# Session-persistence oracle: drives the impl through the CLAP params/state
# extensions with a stub host (save -> fresh instance -> load -> identical
# values AND identical audio).
# Real-time safety oracle: the audio thread must not allocate (charter rule,
# previously enforced only by review).
# mpe_check: per-note expression must reach EVERY oscillator (2026-08-09).
# Links the plugin impl, like rtsafety_probe — it drives the real CLAP event
# path rather than a core in isolation, because the bug lived in the event
# fan-out, which a core-level test cannot see.
# samplerate_check: behaviour declared in SECONDS must not track the sample
# rate (ADR-009). Core-level, no plugin link.
add_executable(samplerate_check tools/samplerate_check.cpp)
target_include_directories(samplerate_check PRIVATE src)
if(NOT MSVC)
  target_compile_options(samplerate_check PRIVATE -O2)
endif()

# routing_check: the B23 crosspoint matrix (ADR-088) before it enters the audio
# path — core + oracle first, same order the glide and swarmalator ports used.
add_executable(routing_check tools/routing_check.cpp)
target_include_directories(routing_check PRIVATE src)
# B50 phase 1: assertions 8-12 drive the matrix through the SHELL's parameter
# surface (ids, morph field, state chunk), so the oracle needs the impl library.
# Assertions 1-7 still measure the core alone and are unaffected by the link.
target_link_libraries(routing_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(routing_check PRIVATE -O2)
endif()

# subdiv_check: rendering must not depend on how a buffer is subdivided
# (ADR-086). Core-level, so no plugin link.
add_executable(subdiv_check tools/subdiv_check.cpp)
target_include_directories(subdiv_check PRIVATE src)
if(NOT MSVC)
  target_compile_options(subdiv_check PRIVATE -O2)
endif()

add_executable(mpe_check tools/mpe_check.cpp)
target_link_libraries(mpe_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(mpe_check PRIVATE -O2)
endif()

# mixer_check: per-oscillator mute/solo actually gate the mix (B24, 2026-08-09).
# Not yet in ./verify — that is a protected path and needs a human gate.
# registry_dump: emits our parameter tables for FOUNDATIONS' registry_conformance
# (F2 Stage 1 increment 2). Links BOTH shells — the instrument and swarmfx —
# because the conformance run covers both.
add_executable(registry_dump tools/registry_dump.cpp)
target_link_libraries(registry_dump PRIVATE ${PROJECT_NAME}-impl SWARMFX-impl)
if(NOT MSVC)
  target_compile_options(registry_dump PRIVATE -O2)
endif()

# paramscope_check: a parameter's SCOPE must be what it claims — globals reach
# every oscillator, per-osc params reach only their own (ADR-089 conditions met).
add_executable(paramscope_check tools/paramscope_check.cpp)
target_link_libraries(paramscope_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(paramscope_check PRIVATE -O2)
endif()

add_executable(mixer_check tools/mixer_check.cpp)
target_link_libraries(mixer_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(mixer_check PRIVATE -O2)
endif()

# notchslot_check: FX rack slot type 6 (Notch) actually reaches the DSP.
# notch_check (below) proves NotchCore itself is correct in isolation; this
# proves the RACK routes to it — drives the real plugin through the CLAP
# factory exactly like mixer_check/steal_check, not a standalone core harness.
add_executable(notchslot_check tools/notchslot_check.cpp)
target_link_libraries(notchslot_check PRIVATE ${PROJECT_NAME}-impl)

# slotcontract_check: every FX slot keeps the promise it declares in
# kSlotContract. Drives the real rack through the CLAP factory; the declaration
# lives in fx_rack.h so a NEW slot type must state its identity point and side
# effects, and this measures whether it told the truth.
add_executable(slotcontract_check tools/slotcontract_check.cpp)
target_link_libraries(slotcontract_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(notchslot_check PRIVATE -O2)
endif()

# conformance_check: FOUNDATIONS' note-lifecycle conformance suite (their R5),
# run against OUR tag tables. OPTIONAL BY CONSTRUCTION — their headers are
# vendored to a gitignored path because this repo is public and theirs is
# private (human ruling 2026-08-15), so the target simply does not exist where
# they are absent, which is every CI runner. `./verify` reports it SKIPPED
# there rather than pretending the check ran.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/vendor/foundations/note_conformance.h")
  add_executable(conformance_check tools/conformance_check.cpp)
  target_link_libraries(conformance_check PRIVATE ${PROJECT_NAME}-impl)
  target_include_directories(conformance_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libs/vendor")
  if(NOT MSVC)
    target_compile_options(conformance_check PRIVATE -O2)
  endif()
endif()

add_executable(rtsafety_probe tools/rtsafety_probe.cpp)
target_link_libraries(rtsafety_probe PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(rtsafety_probe PRIVATE -O2)
endif()

# Oscillator preset tier (B20): format + slot-agnosticism.
add_executable(preset_check tools/preset_check.cpp)
target_include_directories(preset_check PRIVATE src)
if(NOT MSVC)
  target_compile_options(preset_check PRIVATE -O2)
endif()

add_executable(state_check tools/state_check.cpp)
target_link_libraries(state_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(state_check PRIVATE -O2)
endif()

# B100 state-fixture corpus (tests/state_fixtures): gen_state_fixtures writes
# blobs + goldens (append-only, never overwrites); statefix_check loads every
# fixture and requires a bit-identical render. Not in ./verify — that is a
# protected path and a human decision (see the corpus README).
# undo_check: the B84/ADR-160 history oracle. Reuses the fixture corpus above
# for its bit-identity half, which is why it sits here. NOT in ./verify —
# standing ruling: wiring a gate is the human's decision (charter), proposed in
# the PR that adds this.
add_executable(undo_check tools/undo_check.cpp)
target_link_libraries(undo_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(undo_check PRIVATE -O2)
endif()

# B134 per-route modulation polarity (polarity_check). STANDALONE and NOT in
# ./verify — wiring a gate is the human's decision (charter), proposed in the
# PR that adds it. Reuses statefix_common.h's stub host + render.
add_executable(polarity_check tools/polarity_check.cpp)
target_link_libraries(polarity_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(polarity_check PRIVATE -O2)
endif()

# B148 the swarm size never indexes past kMaxV (ncap_check). Core-direct — it
# includes swarm_core.h and links nothing, because the defect it pins is
# exactly the one the SHELL's param row hid from every core-direct tool.
# STANDALONE and NOT in ./verify — wiring a gate is the human's decision
# (charter), proposed in the PR that adds it.
add_executable(ncap_check tools/ncap_check.cpp)
if(NOT MSVC)
  target_compile_options(ncap_check PRIVATE -O2)
endif()

# B335 swarm_core.h's ensemble timing (onset scatter, timing correction, per-voice
# envelopes), rendered as the reference for its JS transcription in the composed
# engine (onset_ref_check). Core-direct, links nothing (the ncap_check pattern). It
# re-derives tools/labharness/onset_ref_cpp.json and fails when the committed copy is
# stale, so the fixture must be regenerated with THIS target's binary (its flags
# decide the last bits). Wired into ./verify full in the PR that adds it (ADR-180 §1).
add_executable(onset_ref_check tools/onset_ref_check.cpp)
if(NOT MSVC)
  target_compile_options(onset_ref_check PRIVATE -O2)
endif()

# B149 the ensemble-timing stream is seeded and survives the chunk
# (tseed_check). Links the impl because sections D-E assert on the SHIPPED
# state extension — the chunk is the shell's, so a core-only stand-in would
# certify the wrong thing. STANDALONE and NOT in ./verify — wiring a gate is
# the human's decision (charter), proposed in the PR that adds it.
add_executable(tseed_check tools/tseed_check.cpp)
target_link_libraries(tseed_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(tseed_check PRIVATE -O2)
endif()

# B89 phase 1 parameter classification (paramclass_check). STANDALONE and NOT
# in ./verify — wiring a gate is the human's decision, proposed in the PR that
# adds it. Prints the full class table before it asserts: the classification is
# a judgement the human reviews, and a green exit says only that the rule is
# self-consistent. Reuses statefix_common.h's stub host.
add_executable(paramclass_check tools/paramclass_check.cpp)
target_link_libraries(paramclass_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(paramclass_check PRIVATE -O2)
endif()

add_executable(gen_state_fixtures tools/gen_state_fixtures.cpp)
target_link_libraries(gen_state_fixtures PRIVATE ${PROJECT_NAME}-impl)
add_executable(statefix_check tools/statefix_check.cpp)
target_link_libraries(statefix_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(gen_state_fixtures PRIVATE -O2)
  target_compile_options(statefix_check PRIVATE -O2)
endif()

# Note-lifecycle oracle (ADR-038): seeded random note streams through the
# CLAP process path (poly/mono/legato + MIDI vel-0 note-off convention);
# output must decay to silence after all keys are up.
add_executable(waveshape_check tools/waveshape_check.cpp)
if(NOT MSVC)
  target_compile_options(waveshape_check PRIVATE -O2)
endif()

add_executable(endprobe tools/endprobe.cpp)
target_link_libraries(endprobe PRIVATE ${PROJECT_NAME}-impl)

# DAW-like end-to-end probe (diagnostic, not a gate): drives the real plugin
# through the CLAP factory — params, notes, and RAW MIDI including the plain
# channel-0 wheel — at 44.1k AND 48k, and prints the rendered pitch trajectory.
# Born 2026-08-19: every glide oracle was core-level, so a wheel that never
# reached the engine shipped green.
# Shell-level CPU bench (diagnostic): the real plugin through the CLAP factory
# with suspects toggled one row at a time. cpu_bench times SwarmCore alone;
# the ADR-099 finding (a silent oscillator billed like an audible one) was
# invisible to it and obvious here.
# ADR-100 lifecycle probe + the tail-cost reproduction of the human's patch
# (diagnostics, not gates — same rule as glidepath_probe).
# ADR-109 corner-edit routing probe (diagnostic, not a gate)
add_executable(corner_probe tools/corner_probe.cpp)
target_link_libraries(corner_probe PRIVATE ${PROJECT_NAME}-impl)
add_executable(enable_probe tools/enable_probe.cpp)
target_link_libraries(enable_probe PRIVATE ${PROJECT_NAME}-impl)
add_executable(user_patch_bench tools/user_patch_bench.cpp)
target_link_libraries(user_patch_bench PRIVATE ${PROJECT_NAME}-impl)
add_executable(shell_bench tools/shell_bench.cpp)
target_link_libraries(shell_bench PRIVATE ${PROJECT_NAME}-impl)
# B204 save-stall measurement: state_save, the B174/B122 dirty tests and their
# control, and whether session length reaches the save path. Diagnostic, not a
# gate — same standing as the two benches above.
add_executable(save_bench tools/save_bench.cpp)
target_link_libraries(save_bench PRIVATE ${PROJECT_NAME}-impl)
# Its VST3 twin: loads the BUILT bundle as a host does and times
# IComponent::getState, because the stall was reported on the VST3 path and the
# CLAP number is not a stand-in for it. Only where the VST3 SDK target exists
# (clap-wrapper CPM-fetches it) and only on APPLE — the loader is CoreFoundation.
if(APPLE AND TARGET base-sdk-vst3)
  add_executable(vst3_save_bench tools/vst3_save_bench.cpp)
  target_link_libraries(vst3_save_bench PRIVATE base-sdk-vst3 "-framework CoreFoundation")
  add_dependencies(vst3_save_bench ${PROJECT_NAME}_vst3)
endif()
add_executable(glidepath_probe tools/glidepath_probe.cpp)
target_link_libraries(glidepath_probe PRIVATE ${PROJECT_NAME}-impl)
add_executable(tailprobe tools/tailprobe.cpp)
target_link_libraries(tailprobe PRIVATE ${PROJECT_NAME}-impl)

add_executable(paramfunc_smoke tools/paramfunc_smoke.cpp)
target_link_libraries(paramfunc_smoke PRIVATE ${PROJECT_NAME}-impl)

add_executable(paramleak_probe tools/paramleak_probe.cpp)
target_link_libraries(paramleak_probe PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(paramleak_probe PRIVATE -O2)
endif()

# kstuck_probe: the 2026-08-12 field report — a note that fails to die at, or
# after, high coupling. Every note-lifecycle oracle we have runs at ONE value of
# K, so the coupling axis is untested for lifecycle in an instrument whose whole
# thesis is the coupling.
add_executable(kstuck_probe tools/kstuck_probe.cpp)
target_link_libraries(kstuck_probe PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(kstuck_probe PRIVATE -O2)
endif()

# steal_check: WHICH voice dies when the pool is full. notefuzz proves no voice
# HANGS and proves it whether the victim is oldest, newest or random — steal
# priority was entirely unpinned until this (FOUNDATIONS seam question 1).
add_executable(steal_check tools/steal_check.cpp)
target_link_libraries(steal_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(steal_check PRIVATE -O2)
endif()

# gain_probe: scratch diagnostic for the 2026-08-25 "too quiet on many
# settings" report. Measures peak/RMS dBFS out of the real plugin across voice
# count, coupling K and normExp. Not a gate; not run by ./verify.
# ratchet_probe: scratch diagnostic for the B47 second report ("more the
# longer I play... moreso when I morph partway"). Cost trajectory + occupancy
# + RSS under a seeded stream, static and morphing. Not a gate.
add_executable(ratchet_probe tools/ratchet_probe.cpp)
target_link_libraries(ratchet_probe PRIVATE ${PROJECT_NAME}-impl)
# combguard_check: a second Comb slot is impossible by construction (rack
# instance caps, refused at the shell's choke point). Standalone, unwired.
add_executable(combguard_check tools/combguard_check.cpp)
target_link_libraries(combguard_check PRIVATE ${PROJECT_NAME}-impl)
# fxxfade_check: B117/ADR-163 FX presence crossfade — the default is inert, a
# tail is faded rather than cut, one switch makes one fade, and the singleton
# guard counts a fading shadow. Standalone, unwired (the rack-probe ruling).
add_executable(fxxfade_check tools/fxxfade_check.cpp)
target_link_libraries(fxxfade_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(fxxfade_check PRIVATE -O2)
endif()

# robustness_matrix: the table docs/ROBUSTNESS.md publishes (B101), measured
# rather than typed — the real plugin at 44.1/48/96/192 k x 32..2048 frames:
# allocation-free process() (rtsafety_probe's counter), no NaN/Inf, tail
# decays, plus the denormal-stall section with its own can-fire control.
# Diagnostic, not a gate; ./verify does not run it.
add_executable(robustness_matrix tools/robustness_matrix.cpp)
target_link_libraries(robustness_matrix PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(robustness_matrix PRIVATE -O2)
endif()

# pitch_probe: played vs heard MIDI per new note across quantise setups
# (2026-09-11 "random note in a scale" report). Diagnostic, unwired.
add_executable(pitch_probe tools/pitch_probe.cpp)
target_link_libraries(pitch_probe PRIVATE ${PROJECT_NAME}-impl)

# morphscope_probe: WHERE does an edit land under the morph (mode x arm x
# puck position), read back corner by corner. Diagnostic for the 2026-09-03
# "globals act like corner params" report; prints, never judges.
add_executable(morphscope_probe tools/morphscope_probe.cpp)
target_link_libraries(morphscope_probe PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(ratchet_probe PRIVATE -O2)
endif()

# mod_check: matrix increment 1 invariants (mod_core.h). Core + oracle land
# together, unwired -- the glide_core order. NOT yet run by ./verify: adding a
# gate is a human decision (charter), proposed in the PR that adds this.
add_executable(mod_check tools/mod_check.cpp)
if(NOT MSVC)
  target_compile_options(mod_check PRIVATE -O2)
endif()

# intent_check: the intent-bus resolver's oracle (B89 phase 2a, ADR-176) --
# parity against reference/intent-bus.html over generated fixtures, plus
# invariants for the coupling blend the prototype does not span. Header-only
# (src/intent_core.h), links nothing. Unwired, same standing ruling as the rest.
add_executable(intent_check tools/intent_check.cpp)
# Phase 2b added section S, which drives the SHIPPED PLUGIN (the seam, the
# chunk, the bit-identity controls) — so the tool links the impl the way
# polarity_check and paramclass_check do. The parity half still links nothing
# but the header; the link is what section S needs, not what §4.5 needs.
target_link_libraries(intent_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(intent_check PRIVATE -O2)
endif()

# voicetap_check: the per-voice seam's bit-identity proof (B81/B82 inc 1,
# ADR-148). Core-direct, like mod_check. Wiring into ./verify: human decision.
add_executable(voicetap_check tools/voicetap_check.cpp)
if(NOT MSVC)
  target_compile_options(voicetap_check PRIVATE -O2)
endif()

# bendsweep_probe: B94's reproduction sweep — GlideCore-direct, spring x
# quantise x hysteresis x step-timing, both control rates. Diagnostic.
add_executable(bendsweep_probe tools/bendsweep_probe.cpp)
if(NOT MSVC)
  target_compile_options(bendsweep_probe PRIVATE -O2)
endif()

# svf_check: SvfCore's oracle-as-spec (B81 inc 2 — no reference lab exists on
# purpose; the suite IS the spec). Unwired, same standing ruling as the rest.
add_executable(svf_check tools/svf_check.cpp)
if(NOT MSVC)
  target_compile_options(svf_check PRIVATE -O2)
endif()

# measure_alias / measure_cpu: the evidence layer's two tables (B103,
# docs/MEASUREMENTS.md). Measurements, not gates — they print, never judge —
# and they drive the shipped plugin through the factory. Standalone, unwired.
add_executable(measure_alias tools/measure_alias.cpp)
target_link_libraries(measure_alias PRIVATE ${PROJECT_NAME}-impl)
add_executable(measure_cpu tools/measure_cpu.cpp)
target_link_libraries(measure_cpu PRIVATE ${PROJECT_NAME}-impl)
# Both print HYPERSAW_BUILD_STAMP so a table names the code that produced it (L0020).
target_include_directories(measure_alias PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(measure_cpu PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
if(NOT MSVC)
  target_compile_options(measure_alias PRIVATE -O2)
  target_compile_options(measure_cpu PRIVATE -O2)
endif()

# measure_modsources: ROADMAP B236 — computational cost of two candidate
# modulation sources (Kuro LFO, ORBITAL) that exist only as JS. Self-contained
# port, no CLAP link (neither law touches the shell). UNWIRED, same standing
# as measure_alias/measure_cpu: it prints, never judges (see the file header).
add_executable(measure_modsources tools/measure_modsources.cpp)
if(NOT MSVC)
  target_compile_options(measure_modsources PRIVATE -O3)
endif()

# measure_fx: ROADMAP B262 — computational cost of the rack's nine FX module
# types (src/fx_rack.h) plus the engine's own baseline for scale. FxRack is
# header-only so the module cells need no plugin link; the one engine-
# baseline cell reuses measure_cpu.cpp's own method (CLAP factory), so this
# target links ${PROJECT_NAME}-impl the same way measure_cpu does. UNWIRED,
# same standing as measure_alias/measure_cpu/measure_modsources: it prints,
# never judges (see the file's own header).
add_executable(measure_fx tools/measure_fx.cpp)
target_link_libraries(measure_fx PRIVATE ${PROJECT_NAME}-impl)
target_include_directories(measure_fx PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
if(NOT MSVC)
  target_compile_options(measure_fx PRIVATE -O3)
endif()

# ---- B147 layer 2: the SWARM SAW engine fidelity suite --------------------
# Six checks closing the six oracle gaps docs/audits/2026-09-18-saw-engine-audit.md
# §3 enumerated. ALL SIX ARE STANDALONE AND UNWIRED: ./verify is a protected
# path and wiring a gate is the human's decision (charter; the route is
# ADR-171), proposed in the PR that adds them. `subdiv_check` and
# `samplerate_check` — the two gates whose blind spots these cover — are not
# touched.
#
# alias_check and cpu_check are the SAME SOURCE as measure_alias / measure_cpu,
# built a second time with the judging section switched on. The audit's
# "reduction note" is explicit that these two gaps are closed by making two
# already-calibrated binaries JUDGE, not by writing a third FFT or a second
# stopwatch — so the judgement lives beside the measurement it judges, and
# measure_alias / measure_cpu keep their "print, never judge" contract byte for
# byte (their exit code and output are unchanged; only the extra table rows the
# audit asked for are shared).
add_executable(alias_check tools/measure_alias.cpp)
target_link_libraries(alias_check PRIVATE ${PROJECT_NAME}-impl)
target_include_directories(alias_check PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(alias_check PRIVATE ALIAS_JUDGE=1)
add_executable(cpu_check tools/measure_cpu.cpp)
target_link_libraries(cpu_check PRIVATE ${PROJECT_NAME}-impl)
target_include_directories(cpu_check PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(cpu_check PRIVATE CPU_JUDGE=1)
if(NOT MSVC)
  target_compile_options(alias_check PRIVATE -O2)
  target_compile_options(cpu_check PRIVATE -O2)
endif()

# The other four are core-direct (no shell link): they measure SwarmCore's own
# behaviour, and the shell would only add a layer between the probe and the
# quantity.
add_executable(sr_check tools/sr_check.cpp)
add_executable(blocksize_check tools/blocksize_check.cpp)
add_executable(stability_check tools/stability_check.cpp)
add_executable(denormal_check tools/denormal_check.cpp)
foreach(t sr_check blocksize_check stability_check denormal_check)
  target_include_directories(${t} PRIVATE src)
  if(NOT MSVC)
    target_compile_options(${t} PRIVATE -O2)
  endif()
endforeach()

# strata_check: STRATA's six properties, as the human's own spec states them
# (B77 increment 1). Core + oracle land together, unwired -- the glide_core /
# mod_core order. Wiring it into ./verify is a human decision (charter).
add_executable(strata_check tools/strata_check.cpp)
if(NOT MSVC)
  target_compile_options(strata_check PRIVATE -O2)
endif()

# delay_check: the standard delay's SPEC (ADR-142). Not a parity test -- this
# core has no HTML lab on purpose, so the impulse-response invariants in the
# oracle are the definition of correct. Wiring it into ./verify is a human
# decision (charter); proposed in the PR that adds it.
add_executable(delay_check tools/delay_check.cpp)
if(NOT MSVC)
  target_compile_options(delay_check PRIVATE -O2)
endif()

# station_check: the STATION port's oracle (B153 layer 3 phase 1) -- L0-1 parity
# against goldens sliced live from reference/station.html, plus the behavioural
# rows parity structurally cannot see and the five SPEC-STATION §11 divergences.
# Core-direct (no shell link): the shell has no STATION seam yet, and a shell
# layer between the probe and the quantity would only add noise. UNWIRED in
# ./verify -- that is a gate edit and a human decision (charter).
add_executable(station_check tools/station_check.cpp)
target_include_directories(station_check PRIVATE src)
if(NOT MSVC)
  target_compile_options(station_check PRIVATE -O2)
endif()

# subosc_check: the SUB OSC port's oracle -- L0-1 parity against goldens sliced
# live from reference/subosc.html at both 48 000 and 44 100 Hz, plus
# SPEC-SUBOSC §10's measured rows with their must-fail controls (phase 1), plus
# section 11's SHELL rows (B172 phase 2). It LINKS THE PLUGIN IMPL now: phase 2
# gave the sub a source row, and the claims phase 2 makes -- the gate is
# bit-inert, the row reproduces the core through the matrix, the chunk carries
# every new id, the retired sync id stays inert (B184) -- live between the core and the
# output, which a core-direct probe is structurally blind to (L0031 (B)).
# WIRED in ./verify full (ADR-180 §1).
add_executable(subosc_check tools/subosc_check.cpp)
target_link_libraries(subosc_check PRIVATE ${PROJECT_NAME}-impl)
target_include_directories(subosc_check PRIVATE src)
if(NOT MSVC)
  target_compile_options(subosc_check PRIVATE -O2)
endif()

add_executable(gain_probe tools/gain_probe.cpp)
target_link_libraries(gain_probe PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(gain_probe PRIVATE -O2)
endif()

add_executable(notefuzz_check tools/notefuzz_check.cpp)
target_link_libraries(notefuzz_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(notefuzz_check PRIVATE -O2)
endif()

# B446 P1 boundary guards (src/input_guards.h), wired into ./verify full under
# ADR-180 §1. Both link the impl so the rows drive the shipped shell paths.
# bridge_utf8_check: names are valid UTF-8 and cut at a character boundary
# before they cross the GUI bridge. hostile_events_check: out-of-range host note
# keys and expression values are handled at the event boundary, output finite.
add_executable(bridge_utf8_check tools/bridge_utf8_check.cpp)
target_link_libraries(bridge_utf8_check PRIVATE ${PROJECT_NAME}-impl)
add_executable(hostile_events_check tools/hostile_events_check.cpp)
target_link_libraries(hostile_events_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(bridge_utf8_check PRIVATE -O2)
  target_compile_options(hostile_events_check PRIVATE -O2)
endif()
# load_handoff_check (B448 B2): a host state load stays the loaded patch when
# processing stops part-way through it, and a processing load writes nothing
# into the morph corners. Single-threaded and deterministic — the ordering
# companion to tools/tsan_stress_check.py. Links the impl; unwired (its header).
add_executable(load_handoff_check tools/load_handoff_check.cpp)
target_link_libraries(load_handoff_check PRIVATE ${PROJECT_NAME}-impl)
# gui_webview_check (B446 W3a, wired into ./verify full on macOS): the shipped
# GUI on a real WKWebView — one page only, its content policy, the bridge's
# sender check, no script clipboard read, file names as text — each beside a
# control that must see the defect on a plain choc view.
if(APPLE)
  add_executable(gui_webview_check tools/gui_webview_check.mm)
  target_link_libraries(gui_webview_check PRIVATE ${PROJECT_NAME}-impl)
endif()

# trace_check: the forensic note trace (FOUNDATIONS ask (c)). Links the impl so
# it can trigger the REAL dump path through the test hook rather than rebuilding
# the ring in the test — a check that reimplements its own subject spans the
# wrong layer (L0031).
add_executable(trace_check tools/trace_check.cpp)
target_link_libraries(trace_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(trace_check PRIVATE -O2)
endif()

# Track E0 force-core oracle (host-side tool, not shipped): forcecore vs the
# effect labs' population trajectories + L0-14/15/17/19/20 population criteria.
add_executable(force_check tools/force_check.cpp)
if(NOT MSVC)
  target_compile_options(force_check PRIVATE -O2)
endif()

# Phase 4 SPECTRA oracle (host-side tool, not shipped): SpectraCore parity vs
# the JS reference + L0-6/7 behavioral checks.
add_executable(spectra_check tools/spectra_check.cpp)
if(NOT MSVC)
  target_compile_options(spectra_check PRIVATE -O2)
endif()

# Track E1 resonator-bank oracle (host-side tool, not shipped): FilterCore
# audio parity vs the JS FilterLab + L0-14 collapse->Q.
add_executable(filter_check tools/filter_check.cpp)
if(NOT MSVC)
  target_compile_options(filter_check PRIVATE -O2)
endif()

# Track E1.2 notch-swarm oracle (host-side tool, not shipped): NotchCore audio
# parity vs the JS PhaserLab + L0-16 notch exactness.
add_executable(notch_check tools/notch_check.cpp)
if(NOT MSVC)
  target_compile_options(notch_check PRIVATE -O2)
endif()

# Swarmalator engine oracle (host-side tool, not shipped): SwarmalatorCore
# stereo parity vs the JS reference + SPEC-SWARMALATOR §5 acceptance anchors.
add_executable(swarmalator_check tools/swarmalator_check.cpp)
if(NOT MSVC)
  target_compile_options(swarmalator_check PRIVATE -O2)
endif()

# Track E2 time-engine oracle (host-side tool, not shipped): TimeCore audio
# parity vs the JS TimeLab (echo + room) + L0-19/20/21.
add_executable(time_check tools/time_check.cpp)
if(NOT MSVC)
  target_compile_options(time_check PRIVATE -O2)
endif()

# ADR-006 spike benchmark (host-side tool, not shipped): bank vs iFFT renderer.
# CPU bench against the E-6 envelope — runs the real SwarmCore so a min-spec
# number can replace ADR-082's derate ESTIMATE.
# Travel-law trajectory oracle (A1 fold): parity vs bend-lab's Inertia.
add_executable(glide_check tools/glide_check.cpp)
target_include_directories(glide_check PRIVATE src)
if(NOT MSVC)
  target_compile_options(glide_check PRIVATE -O2)
endif()

add_executable(cpu_bench tools/cpu_bench.cpp)
target_include_directories(cpu_bench PRIVATE src)
if(NOT MSVC)
  target_compile_options(cpu_bench PRIVATE -O3)
endif()

add_executable(renderer_bench tools/renderer_bench.cpp)
if(NOT MSVC)
  target_compile_options(renderer_bench PRIVATE -O3)
endif()
if(APPLE)
  target_link_libraries(renderer_bench PRIVATE "-framework Accelerate")
endif()

# ---- horde 2 cores (h2/, ADR-186) ----------------------------------------------
# These targets NEVER link ${PROJECT_NAME}-impl: horde 2's cores are copied
# forward into their own directory and namespace (ADR-186 item 4), and a link
# that saw a legacy core and its horde-2 copy together is the ODR hazard the
# namespace exists to prevent. Placed ABOVE the MSVC-stack and sanitizer loops
# so both reach them (those loops only see targets already declared).
#
# h2_scalpel_parity_check (B332): the PARITY build — doubles, -O2 and
# -ffp-contract=off, because clang (and GCC in gnu++ mode) would otherwise fuse
# a*b+c, which V8 never does (ADR-187 item 6). Named apart from any shipped
# build on purpose. Wired in ./verify full.
add_executable(h2_scalpel_parity_check tools/h2_scalpel_parity_check.cpp)
if(NOT MSVC)
  target_compile_options(h2_scalpel_parity_check PRIVATE -O2 -ffp-contract=off)
endif()
# h2_scalpel_fma_control: the same source with contraction FORCED ON — the
# must-fail control for "FMA contraction on", and the ONE declared exception to
# the contraction-off rule (tools/h2_rules_check.py). Exit 0 means the parity
# gate caught the fused build over the full stream; exit 2 means the run itself
# broke, never counted as firing. On x86-64 contraction needs -mfma to have an
# instruction to emit; arm64 always has one. Wired in ./verify full.
add_executable(h2_scalpel_fma_control tools/h2_scalpel_parity_check.cpp)
target_compile_definitions(h2_scalpel_fma_control PRIVATE H2_SCALPEL_FMA_CONTROL)
if(NOT MSVC)
  target_compile_options(h2_scalpel_fma_control PRIVATE -O2 -ffp-contract=fast)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    target_compile_options(h2_scalpel_fma_control PRIVATE -mfma)
  endif()
endif()
# measure_h2_scalpel: CPU per voice vs the JS oracle (ADR-187 item 8, Layer-E).
# The RELEASE build: -O3, a separate target from the parity build — but with
# contraction OFF, like every target that compiles an h2 core (RULED by the human
# 2026-09-30, B332: the shipped build is arithmetically the tested one. At
# clang's default contraction the same core misses parity on 19 of 386
# scenarios; tools/h2_rules_check.py enforces the flag, docs/port/scalpel-phase-1a.md
# has the numbers). Prints, never judges; unwired.
add_executable(measure_h2_scalpel tools/measure_h2_scalpel.cpp)
if(NOT MSVC)
  target_compile_options(measure_h2_scalpel PRIVATE -O3 -ffp-contract=off)
endif()
# auhost (B381 stage 1): the offline AudioUnit host for the reference gauntlet,
# plus a --horde mode that times the h2 SCALPEL core through the same loop. A
# tool, not a gate (prints, never judges; unwired). macOS only: AudioToolbox.
# It compiles an h2 core, so contraction is off (tools/h2_rules_check.py rule 2).
if(APPLE)
  add_executable(auhost tools/auhost/auhost.cpp)
  target_compile_options(auhost PRIVATE -O3 -ffp-contract=off)
  target_link_libraries(auhost PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
endif()
# h2_swarm_lift_check (B379 step 1): the lifted swarm core (h2/cores/swarm/,
# ADR-186 copy-forward) compiled as an h2 target, contraction off like every
# other, and lift edit E1 re-proven: a caller-owned Voice ticked through the
# public tickVoice() forwarder matches the core's own voice bit for bit, with a
# must-fail schedule-slip control. Wired in ./verify full; the byte-identity half
# (tools/h2_lift_check.py) is in ./verify fast.
add_executable(h2_swarm_lift_check tools/h2_swarm_lift_check.cpp)
if(NOT MSVC)
  target_compile_options(h2_swarm_lift_check PRIVATE -O2 -ffp-contract=off)
endif()
# h2_swarm_parity_check (B379 rework, critic H1): the legacy L0-1 parity chain
# DUPLICATED onto the lifted core (ADR-186 item 4: "re-pointed or duplicated at
# the lift"). The source is a COPY of tools/parity_check.cpp with only its include
# and namespace changed (h2/cores/swarm/lift-ledger.json holds it, with its source
# blob, and tools/h2_lift_check.py gates it). Built as an h2 target, contraction
# OFF, because the legacy parity_check is built at clang's default contraction and
# so never measured the flags horde 2 ships. Wired in ./verify full beside it,
# reading the same goldens.
add_executable(h2_swarm_parity_check tools/h2_swarm_parity_check.cpp)
if(NOT MSVC)
  target_compile_options(h2_swarm_parity_check PRIVATE -O2 -ffp-contract=off)
endif()
# swarm48_check / h2_swarm48_check (B382): the swarm core against the JS reference
# at 48 kHz (tools/golden/gen_goldens_sr.mjs), every scenario beyond 1e-6
# reported; one body (tools/swarm_sr_parity.h), one core per TU (h2 rule 4). The
# legacy one is built like parity_check, the h2 one as every h2 target is.
add_executable(swarm48_check tools/swarm48_check.cpp)
if(NOT MSVC)
  target_compile_options(swarm48_check PRIVATE -O2)
endif()
add_executable(h2_swarm48_check tools/h2_swarm48_check.cpp)
if(NOT MSVC)
  target_compile_options(h2_swarm48_check PRIVATE -O2 -ffp-contract=off)
endif()
# h2_engine_parity_check (B385): horde 2's ONE engine, the composed engine
# (h2/engine/, horde2::engine), against its golden, the composed JS
# (tools/h2_engine_render.mjs renders it). The parity build: doubles, -O2,
# -ffp-contract=off, like every h2 target (h2 rule 7). In ./verify full (B405,
# ADR-187 A2).
add_executable(h2_engine_parity_check tools/h2_engine_parity_check.cpp)
if(NOT MSVC)
  target_compile_options(h2_engine_parity_check PRIVATE -O2 -ffp-contract=off)
endif()
# h2_engine_fma_control: the same source with contraction FORCED ON, the engine's
# must-fail control for "FMA contraction on" and a declared exception to the
# contraction-off rule (tools/h2_rules_check.py), as h2_scalpel_fma_control is for
# the blade port. Exit 0: over the full stream the fused build missed parity AND
# (where the floor is keyed) its mean bit-exact share fell under the floor; 1: not
# both; 2: the run broke. In ./verify full, after its check.
add_executable(h2_engine_fma_control tools/h2_engine_parity_check.cpp)
target_compile_definitions(h2_engine_fma_control PRIVATE H2_ENGINE_FMA_CONTROL)
if(NOT MSVC)
  target_compile_options(h2_engine_fma_control PRIVATE -O2 -ffp-contract=fast)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    target_compile_options(h2_engine_fma_control PRIVATE -mfma)
  endif()
endif()
# h2_engine_selfdigest_check (B441 phase 2): the composed engine against ITSELF,
# a bit-identity digest of every parity scenario against the committed
# h2/engine/selfdigest.parity.<key>.txt. The parity build's flags, so it digests
# the build the parity check judges. In ./verify full, after the parity check.
add_executable(h2_engine_selfdigest_check tools/h2_engine_selfdigest_check.cpp)
if(NOT MSVC)
  target_compile_options(h2_engine_selfdigest_check PRIVATE -O2 -ffp-contract=off)
endif()
# h2_engine_selfdigest_product: the same source at the release flags (-O3, as
# measure_h2_engine; ADR-187 A1, the shipped build is the tested build), without
# the fault hooks, against h2/engine/selfdigest.product.<key>.txt. In ./verify
# full, beside the parity build's.
add_executable(h2_engine_selfdigest_product tools/h2_engine_selfdigest_check.cpp)
target_compile_definitions(h2_engine_selfdigest_product PRIVATE H2_SELFDIGEST_PRODUCT)
if(NOT MSVC)
  target_compile_options(h2_engine_selfdigest_product PRIVATE -O3 -ffp-contract=off)
endif()
# measure_h2_engine (B385 checkpoint 4): the composed engine's CPU per voice
# against the golden's (ADR-187 item 8, Layer-E). The release build, -O3 with
# contraction off like every h2 target (h2 rule 7). Prints, never judges; unwired.
add_executable(measure_h2_engine tools/measure_h2_engine.cpp)
if(NOT MSVC)
  target_compile_options(measure_h2_engine PRIVATE -O3 -ffp-contract=off)
endif()
# measure_h2_engine_stages (B441): the same file with H2_ENGINE_STAGES, the
# measurement-only stage toggles in h2/engine/engine.h (--stages; the cost
# attribution in docs/port/cpu-ledger.md). Never the shipped or the parity build.
add_executable(measure_h2_engine_stages tools/measure_h2_engine.cpp)
target_compile_definitions(measure_h2_engine_stages PRIVATE H2_ENGINE_STAGES)
if(NOT MSVC)
  target_compile_options(measure_h2_engine_stages PRIVATE -O3 -ffp-contract=off)
endif()

# MSVC gives a process a 1 MB main-thread stack; macOS and Linux give 8 MB. A
# SwarmCore is 651 KB (sizeof, MSVC 14.51, 2026-09-09) and the host-side
# oracles put cores on the stack freely: trajectory_check keeps two live at
# once and waveshape_check inlines several render() frames, and both died
# with STATUS_STACK_OVERFLOW (0xC00000FD) on the first native Windows run
# while every other oracle was green. The plugin itself is unaffected — its
# cores are members of the heap-allocated Plugin. 8 MB (the POSIX default)
# was NOT enough: trajectory_check's main() holds ~25 cores in nested scopes
# and MSVC does not overlap the slots of non-trivial locals, so its frame is
# ~16 MB where clang's is one core deep. 64 MB is a virtual RESERVE (pages
# commit on touch), so it costs nothing; set it for every host tool rather
# than heap-patching two and leaving the trap for the next one (L0003).
if(MSVC)
  get_property(_hs_exes DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_t IN LISTS _hs_exes)
    get_target_property(_ty ${_t} TYPE)
    if(_ty STREQUAL "EXECUTABLE")
      target_link_options(${_t} PRIVATE /STACK:67108864)
    endif()
  endforeach()
endif()

# HYPERSAW_SANITIZE (B101): OFF, or a -fsanitize= list — "address,undefined"
# or "thread" — applied to the HOST-SIDE EXECUTABLES only: the oracles and
# probes above, never the plugin libraries. tools/sanitize_oracles.sh and the
# CI `sanitize` job build with it ON into their own tree; the shipped build is
# OFF, and OFF is bit-inert by construction — nothing here runs unless the
# value is set. The header-only cores (swarm_core.h, filter_core.h, ...) are
# compiled INTO each oracle that includes them, so they are instrumented
# there; the impl static library (the shell) is linked uninstrumented, and
# what ASan sees of it is heap misuse at malloc/free boundaries only. A
# STRING rather than option() so the list itself is the value; MSVC has
# /fsanitize=address but a different flag shape, so it is not attempted.
set(HYPERSAW_SANITIZE OFF CACHE STRING
    "OFF, or a -fsanitize= list (address,undefined | thread) for host-side executables only")
if(HYPERSAW_SANITIZE)
  if(MSVC)
    message(WARNING "HYPERSAW_SANITIZE is not wired for MSVC; ignored")
  else()
    get_property(_hs_san_exes DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _hs_san_exes)
      get_target_property(_ty ${_t} TYPE)
      if(_ty STREQUAL "EXECUTABLE")
        target_compile_options(${_t} PRIVATE -fsanitize=${HYPERSAW_SANITIZE} -fno-omit-frame-pointer -g)
        target_link_options(${_t} PRIVATE -fsanitize=${HYPERSAW_SANITIZE})
      endif()
    endforeach()
    message(STATUS "HYPERSAW_SANITIZE=${HYPERSAW_SANITIZE} on host-side executables")
  endif()
endif()

# anchor_check: the wheel lane must not transpose held notes when its anchor
# (lastNoteKey) moves — the 2026-09-11 chord-transposition report. Standalone,
# unwired (human gate).
add_executable(anchor_check tools/anchor_check.cpp)
target_link_libraries(anchor_check PRIVATE ${PROJECT_NAME}-impl)

# preset_probe: load a saved preset, run the 2026-09-11 two-note + chord symptoms. Diagnostic, unwired.
add_executable(preset_probe tools/preset_probe.cpp)
target_link_libraries(preset_probe PRIVATE ${PROJECT_NAME}-impl)

# morphlayout_check: the morph field array layout is append-only (ADR-159). Wired in ./verify full (B124; B240 T13-T15).
add_executable(morphlayout_check tools/morphlayout_check.cpp)
target_link_libraries(morphlayout_check PRIVATE ${PROJECT_NAME}-impl)

# penv_check: ENV 2 restarts its attack on every strike (ADR-161). Standalone, unwired.
add_executable(penv_check tools/penv_check.cpp)
target_link_libraries(penv_check PRIVATE ${PROJECT_NAME}-impl)

# lfoenv_check: B171's LFO 1/2 and ENV 3/4 as mod sources (ADR-181). WIRED into
# ./verify full in the PR that adds it — ADR-180 §1 inverted the default.
add_executable(lfoenv_check tools/lfoenv_check.cpp)
target_link_libraries(lfoenv_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(lfoenv_check PRIVATE -O2)
endif()

# offcorner_check: B232 / ADR-183 — in blend, a corner where the source is OFF
# gives no weight, as revision 2; revision 1 renders as before. WIRED into
# ./verify full in the PR that adds it (ADR-180 §1).
add_executable(offcorner_check tools/offcorner_check.cpp)
target_link_libraries(offcorner_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(offcorner_check PRIVATE -O2)
endif()

# modreadback_check: B241 — a modulated parameter persists at its BASE on every
# path (readback, state, history, corners, morph-on). WIRED into ./verify full
# in the PR that adds it (ADR-180 §1).
add_executable(modreadback_check tools/modreadback_check.cpp)
target_link_libraries(modreadback_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(modreadback_check PRIVATE -O2)
endif()

# fxxfade_plugin_check: B117 through the plugin param path (2026-09-14). Standalone, unwired.
add_executable(fxxfade_plugin_check tools/fxxfade_plugin_check.cpp)
target_link_libraries(fxxfade_plugin_check PRIVATE ${PROJECT_NAME}-impl)

# twocluster_check: negative K in two-cluster mode (ADR-164). Standalone, unwired.
add_executable(twocluster_check tools/twocluster_check.cpp)
target_link_libraries(twocluster_check PRIVATE ${PROJECT_NAME}-impl)

# presetstore_check: the B129 store path helper and the factory install —
# per-platform roots, first open installs, second open writes nothing, a user's
# edit or deletion survives a newer bank. Links nothing (preset_store.h is
# header-only and choc-free) but DOES consume the generated bank, so it also
# proves the real embedded array installs. Standalone, unwired (human gate).
add_executable(presetstore_check tools/presetstore_check.cpp ${FACTORY_BANK_HEADER})
target_include_directories(presetstore_check PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)
if(NOT MSVC)
  target_compile_options(presetstore_check PRIVATE -O2)
endif()

# B130 factory bank. gen_factory_bank WRITES docs/presets/factory/** from the
# table held in its own source; bank_check reads the SHIPPED FILES back and
# asserts the acceptance contract (loads, re-saves identically, makes sound, and
# the named exemplars still demonstrate the coupling laws). Both standalone and
# unwired — the standing ruling: wiring a gate is the human's decision.
add_executable(gen_factory_bank tools/gen_factory_bank.cpp)
target_link_libraries(gen_factory_bank PRIVATE ${PROJECT_NAME}-impl)
add_executable(bank_check tools/bank_check.cpp)
target_link_libraries(bank_check PRIVATE ${PROJECT_NAME}-impl)
if(NOT MSVC)
  target_compile_options(gen_factory_bank PRIVATE -O2)
  target_compile_options(bank_check PRIVATE -O2)
endif()
