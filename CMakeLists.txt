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
# The VST3 SDK is CPM-fetched at configure time by clap-wrapper (v3.8.0 pinned there).
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)
add_subdirectory(libs/clap EXCLUDE_FROM_ALL)
add_subdirectory(libs/clap-wrapper EXCLUDE_FROM_ALL)

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
target_include_directories(${PROJECT_NAME}-impl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(${PROJECT_NAME}-impl PUBLIC clap clap-wrapper-extensions)
if(APPLE)
  target_sources(${PROJECT_NAME}-impl PRIVATE src/gui/hypersaw_gui.mm ${GUI_HTML_HEADER})
  target_include_directories(${PROJECT_NAME}-impl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_link_libraries(${PROJECT_NAME}-impl PUBLIC "-framework WebKit" "-framework Cocoa")
elseif(WIN32)
  # WebView2 via choc (COM declarations embedded — no external SDK).
  # CI-compile-verified; runtime validation is a recorded residual.
  target_sources(${PROJECT_NAME}-impl PRIVATE src/gui/hypersaw_gui_win.cpp ${GUI_HTML_HEADER})
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

        AUV2_MANUFACTURER_NAME "Lifted Truck"
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
        AUV2_MANUFACTURER_NAME "Lifted Truck"
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
