# horde_rack: the generic module slot (ROADMAP B714, ADR-410 r4). A self-contained library both the test plug-in
# (tools/engine-preview) and S8's shell can add_subdirectory: no engine-preview dependency, no machine path. Everything
# module-specific is data in modules.json and the files it names; this file reads them and never names a module.
#
# A consumer sets, before add_subdirectory(<repo>/tools/rack <binary dir>):
#   HORDE_RACK_CHOC_DIR    a directory holding choc/ (its header-only JSON reader)
# and may set HORDE_FOUNDATIONS_DIR and each module's tree variable (modules.json `tree.var`); unset, each defaults to
# the ~-relative path modules.json names, expanded from $HOME here. HORDE_RACK_FOUNDATIONS_PIN_FILE names the shell's
# FOUNDATIONS pin (default: the test plug-in's, tools/engine-preview/foundations.pin; a second shell passes its own).
#
# Then it calls:
#   horde_rack_library(<name> [DEFINES <plant defines>...])   a STATIC rack (a plant build passes its one fault)
#   horde_rack_registry(<name> RACK <rack library> MODULES <modules.json keys>... LEDGERS <ledger files>...)
#       a STATIC library holding the generated registry (rack::registered_modules and registered_ledgers, with each
#       module's pinned manifest, overlay and budget table embedded as text), linked to each module's adapter target.
#       MODULES may be empty: a rack with no module, the reference every "off is bit-identical" check is held to.
#   HORDE_RACK_DEFAULT_MODULES, set here: every modules.json key whose test_only is false, in order.
#
# THE PINS. Each module tree must be at its .pin's commit with its `clean` paths as that commit has them, or the
# configure stops: the module played is the module admitted. FOUNDATIONS is pinned the same way
# (tools/engine-preview/foundations.pin), by its tree rather than its HEAD: the checkout's core/include must be
# byte-for-byte the pin's, tracked and untracked. And every module that vendors foundations/fx_operator.h must hold a
# copy byte-identical to the pinned one, or the configure stops: two definitions of one header in one binary are an
# ODR hazard, and a module built against another ABI draft than the rack's is not the module the rack speaks to.
cmake_minimum_required(VERSION 3.20)

get_filename_component(HORDE_RACK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." REALPATH)
set(HORDE_RACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(READ "${HORDE_RACK_DIR}/modules.json" HORDE_RACK_MODULES_JSON)
if(NOT HORDE_RACK_CHOC_DIR)
  message(FATAL_ERROR "set HORDE_RACK_CHOC_DIR to a directory holding choc/ before adding tools/rack")
endif()

# "~/x" from modules.json, expanded at configure time and never written into a tracked file.
function(horde_rack_expand OUT PATH)
  string(REGEX REPLACE "^~" "$ENV{HOME}" expanded "${PATH}")
  set(${OUT} "${expanded}" PARENT_SCOPE)
endfunction()

# ---- FOUNDATIONS, pinned by tree ------------------------------------------------------------------------------------
if(NOT DEFINED HORDE_FOUNDATIONS_DIR)
  if(DEFINED ENV{HORDE_FOUNDATIONS_DIR})
    set(HORDE_FOUNDATIONS_DIR "$ENV{HORDE_FOUNDATIONS_DIR}" CACHE PATH "FOUNDATIONS checkout (read only)")
  else()
    set(HORDE_FOUNDATIONS_DIR "$ENV{HOME}/Documents/Claude/synthetic-worlds/FOUNDATIONS" CACHE PATH "FOUNDATIONS checkout (read only)")
  endif()
endif()
if(NOT HORDE_RACK_FOUNDATIONS_PIN_FILE)
  set(HORDE_RACK_FOUNDATIONS_PIN_FILE "${HORDE_RACK_ROOT}/tools/engine-preview/foundations.pin")
endif()
file(STRINGS "${HORDE_RACK_FOUNDATIONS_PIN_FILE}" HORDE_RACK_FOUNDATIONS_PIN LIMIT_COUNT 1)
execute_process(COMMAND git -C "${HORDE_FOUNDATIONS_DIR}" cat-file -e "${HORDE_RACK_FOUNDATIONS_PIN}^{commit}" RESULT_VARIABLE rack_f_rc ERROR_QUIET)
if(NOT rack_f_rc EQUAL 0)
  message(FATAL_ERROR "FOUNDATIONS at ${HORDE_FOUNDATIONS_DIR} has no commit ${HORDE_RACK_FOUNDATIONS_PIN} (${HORDE_RACK_FOUNDATIONS_PIN_FILE}). "
                      "Set HORDE_FOUNDATIONS_DIR to a FOUNDATIONS checkout that holds it (read only).")
endif()
# Tracked: the working tree's core/include against the pin's. Untracked: --untracked-files=all is load-bearing, a
# stray header beside the pinned ones would be included before anyone saw it.
execute_process(COMMAND git -C "${HORDE_FOUNDATIONS_DIR}" diff --quiet "${HORDE_RACK_FOUNDATIONS_PIN}" -- core/include RESULT_VARIABLE rack_f_diff)
execute_process(COMMAND git -C "${HORDE_FOUNDATIONS_DIR}" status --porcelain --untracked-files=all -- core/include
                OUTPUT_VARIABLE rack_f_dirty RESULT_VARIABLE rack_f_status)
if(NOT rack_f_diff EQUAL 0 OR NOT rack_f_status EQUAL 0 OR NOT rack_f_dirty STREQUAL "")
  message(FATAL_ERROR "FOUNDATIONS' core/include at ${HORDE_FOUNDATIONS_DIR} is not the pin ${HORDE_RACK_FOUNDATIONS_PIN}'s: "
                      "the build would not be the pinned ABI.\n${rack_f_dirty}")
endif()
set(HORDE_RACK_FOUNDATIONS_INCLUDE "${HORDE_FOUNDATIONS_DIR}/core/include")
file(SHA256 "${HORDE_RACK_FOUNDATIONS_INCLUDE}/foundations/fx_operator.h" HORDE_RACK_FX_OPERATOR_SHA)
message(STATUS "FOUNDATIONS pinned at ${HORDE_RACK_FOUNDATIONS_PIN} (fx_operator.h ${HORDE_RACK_FX_OPERATOR_SHA})")

# The interface every adapter and the rack compile against: admission::Module (tools/admission/module.h) over the
# pinned FOUNDATIONS header, and "rack/..." includes.
add_library(horde_rack_iface INTERFACE)
target_include_directories(horde_rack_iface INTERFACE "${HORDE_RACK_ROOT}/tools" "${HORDE_RACK_ROOT}/tools/admission" "${HORDE_RACK_FOUNDATIONS_INCLUDE}")

set(HORDE_RACK_SOURCES ${HORDE_RACK_DIR}/slot_table.cpp ${HORDE_RACK_DIR}/rack.cpp ${HORDE_RACK_DIR}/chunk.cpp ${HORDE_RACK_DIR}/descriptor.cpp)

function(horde_rack_library NAME)
  cmake_parse_arguments(ARG "" "" "DEFINES" ${ARGN})
  add_library(${NAME} STATIC ${HORDE_RACK_SOURCES})
  target_link_libraries(${NAME} PUBLIC horde_rack_iface)
  target_include_directories(${NAME} PRIVATE "${HORDE_RACK_CHOC_DIR}")
  if(ARG_DEFINES)
    target_compile_definitions(${NAME} PRIVATE ${ARG_DEFINES})
  endif()
  set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

# Every non-test module, in order.
set(HORDE_RACK_DEFAULT_MODULES "")
string(JSON rack_count LENGTH "${HORDE_RACK_MODULES_JSON}" modules)
math(EXPR HORDE_RACK_LAST "${rack_count} - 1")
foreach(i RANGE ${HORDE_RACK_LAST})
  string(JSON rack_key GET "${HORDE_RACK_MODULES_JSON}" modules ${i} key)
  string(JSON rack_test GET "${HORDE_RACK_MODULES_JSON}" modules ${i} test_only)
  if(NOT rack_test)
    list(APPEND HORDE_RACK_DEFAULT_MODULES ${rack_key})
  endif()
endforeach()
# The functions below run in the CALLER's scope (CMake functions see the variables where they are called, not where
# they are defined), so what they read is handed up to the directory that added this one.
foreach(v HORDE_RACK_DEFAULT_MODULES HORDE_RACK_ROOT HORDE_RACK_DIR HORDE_RACK_MODULES_JSON HORDE_RACK_LAST HORDE_RACK_SOURCES
          HORDE_RACK_CHOC_DIR HORDE_RACK_FOUNDATIONS_PIN HORDE_RACK_FX_OPERATOR_SHA HORDE_RACK_FOUNDATIONS_INCLUDE)
  set(${v} "${${v}}" PARENT_SCOPE)
endforeach()

# One module's entry: its tree checked at its pin, its adapter target made (once), its texts read.
function(horde_rack_module KEY)
  set(found -1)
  foreach(i RANGE ${HORDE_RACK_LAST})
    string(JSON k GET "${HORDE_RACK_MODULES_JSON}" modules ${i} key)
    if(k STREQUAL KEY)
      set(found ${i})
    endif()
  endforeach()
  if(found EQUAL -1)
    message(FATAL_ERROR "tools/rack/modules.json has no module \"${KEY}\"")
  endif()
  macro(field OUT)
    string(JSON ${OUT} GET "${HORDE_RACK_MODULES_JSON}" modules ${found} ${ARGN})
  endmacro()
  field(name name)
  field(factory factory)
  field(manifest manifest)
  field(overlay overlay)
  field(recipe recipe)
  field(budget_file budget file)
  field(budget_key budget key)
  field(vendored vendored_foundations)
  string(JSON tree_type TYPE "${HORDE_RACK_MODULES_JSON}" modules ${found} tree)
  set(pin "")
  if(tree_type STREQUAL "NULL")
    set(dir "${HORDE_RACK_ROOT}")   # an in-repo module: its manifest path is the repo's
  else()
    field(var tree var)
    field(default tree default)
    field(pin_file tree pin)
    field(untracked_ok tree untracked_ok)
    string(JSON n_clean LENGTH "${HORDE_RACK_MODULES_JSON}" modules ${found} tree clean)
    set(clean "")
    math(EXPR last_clean "${n_clean} - 1")
    foreach(c RANGE ${last_clean})
      string(JSON p GET "${HORDE_RACK_MODULES_JSON}" modules ${found} tree clean ${c})
      list(APPEND clean ${p})
    endforeach()
    if(NOT DEFINED ${var})
      if(DEFINED ENV{${var}})
        set(${var} "$ENV{${var}}" CACHE PATH "${name} checkout (read only), at its pin")
      else()
        horde_rack_expand(d "${default}")
        set(${var} "${d}" CACHE PATH "${name} checkout (read only), at its pin")
      endif()
    endif()
    set(dir "${${var}}")
    file(STRINGS "${HORDE_RACK_ROOT}/${pin_file}" pin LIMIT_COUNT 1)
    execute_process(COMMAND git -C "${dir}" rev-parse HEAD OUTPUT_VARIABLE head RESULT_VARIABLE rc OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT rc EQUAL 0 OR NOT head STREQUAL pin)
      message(FATAL_ERROR "${name} at ${dir} is at '${head}', but ${pin_file} pins ${pin}. Set ${var} to a checkout at that commit "
                          "(read only: a clone or worktree at the pin, never an edit in the sibling's tree), or move the pin in the same change as the re-test.")
    endif()
    if(untracked_ok)
      execute_process(COMMAND git -C "${dir}" diff --quiet HEAD -- ${clean} RESULT_VARIABLE dirty_rc)
      set(dirty "")
    else()
      execute_process(COMMAND git -C "${dir}" status --porcelain --untracked-files=all -- ${clean} OUTPUT_VARIABLE dirty RESULT_VARIABLE dirty_rc)
    endif()
    if(NOT dirty_rc EQUAL 0 OR NOT dirty STREQUAL "")
      message(FATAL_ERROR "${name}'s ${clean} at ${dir} differ from its HEAD ${pin}: the build would not be the pinned module.\n${dirty}")
    endif()
  endif()
  if(NOT vendored STREQUAL "NOTFOUND" AND NOT vendored STREQUAL "")
    file(SHA256 "${dir}/${vendored}" vendored_sha)
    if(NOT vendored_sha STREQUAL HORDE_RACK_FX_OPERATOR_SHA)
      message(FATAL_ERROR "${name} vendors foundations/fx_operator.h (${vendored}, sha256 ${vendored_sha}), not byte-identical to the pinned "
                          "FOUNDATIONS ${HORDE_RACK_FOUNDATIONS_PIN}'s (${HORDE_RACK_FX_OPERATOR_SHA}): two ABI drafts in one binary (ODR).")
    endif()
  endif()
  set(adapter rack_adapter_${KEY})
  if(NOT TARGET ${adapter})
    set(RACK_MODULE_KEY ${KEY})
    set(RACK_MODULE_DIR "${dir}")
    set(RACK_ADAPTER_TARGET ${adapter})
    include("${HORDE_RACK_ROOT}/${recipe}")
    if(NOT TARGET ${adapter})
      message(FATAL_ERROR "${recipe} did not define the target ${adapter}")
    endif()
    message(STATUS "rack module ${KEY} (${name}) at ${pin}")
  endif()
  file(READ "${dir}/${manifest}" manifest_text)
  file(READ "${HORDE_RACK_ROOT}/${overlay}" overlay_text)
  file(READ "${HORDE_RACK_ROOT}/${budget_file}" budget_text)
  set(RACK_M_NAME "${name}" PARENT_SCOPE)
  set(RACK_M_FACTORY "${factory}" PARENT_SCOPE)
  set(RACK_M_PIN "${pin}" PARENT_SCOPE)
  set(RACK_M_MANIFEST "${manifest_text}" PARENT_SCOPE)
  set(RACK_M_OVERLAY "${overlay_text}" PARENT_SCOPE)
  set(RACK_M_BUDGETS "${budget_text}" PARENT_SCOPE)
  set(RACK_M_BUDGET_KEY "${budget_key}" PARENT_SCOPE)
  set(RACK_M_ADAPTER ${adapter} PARENT_SCOPE)
endfunction()

function(horde_rack_registry NAME)
  cmake_parse_arguments(ARG "" "RACK" "MODULES;LEDGERS" ${ARGN})
  set(src "${CMAKE_BINARY_DIR}/rack-registry/${NAME}.cpp")
  set(text "// Generated by tools/rack/CMakeLists.txt from tools/rack/modules.json (horde_rack_registry ${NAME}). Do not edit.\n")
  string(APPEND text "#include <memory>\n#include <string>\n#include <vector>\n\n#include \"module.h\"\n#include \"rack/slot_table.h\"\n\n")
  set(decls "")
  set(entries "")
  set(adapters "")
  foreach(key ${ARG_MODULES})
    horde_rack_module(${key})
    string(APPEND decls "ModuleInfo ${RACK_M_FACTORY}(const std::string&);\n")
    string(APPEND entries "    {\"${key}\", \"${RACK_M_NAME}\", \"${RACK_M_PIN}\",\n     R\"rack(${RACK_M_MANIFEST})rack\",\n     R\"rack(${RACK_M_OVERLAY})rack\",\n"
                          "     R\"rack(${RACK_M_BUDGETS})rack\", \"${RACK_M_BUDGET_KEY}\",\n     [] { return admission::${RACK_M_FACTORY}(\"\").make(); }},\n")
    list(APPEND adapters ${RACK_M_ADAPTER})
  endforeach()
  set(ledgers "")
  foreach(ledger ${ARG_LEDGERS})
    file(READ "${ledger}" ledger_text)
    string(APPEND ledgers "    R\"rack(${ledger_text})rack\",\n")
  endforeach()
  string(APPEND text "namespace admission {\n${decls}}  // namespace admission\n\nnamespace rack {\n\n")
  string(APPEND text "const std::vector<ModuleSource>& registered_modules() {\n  static const std::vector<ModuleSource> modules = {\n${entries}  };\n  return modules;\n}\n\n")
  string(APPEND text "const std::vector<std::string>& registered_ledgers() {\n  static const std::vector<std::string> ledgers = {\n${ledgers}  };\n  return ledgers;\n}\n\n")
  string(APPEND text "const SlotTable& registered_table() {\n  static const SlotTable table = build_slot_table(registered_modules(), registered_ledgers());\n  return table;\n}\n\n}  // namespace rack\n")
  file(WRITE "${src}.tmp" "${text}")
  configure_file("${src}.tmp" "${src}" COPYONLY)
  add_library(${NAME} STATIC "${src}")
  target_link_libraries(${NAME} PUBLIC ${ARG_RACK} ${adapters})
  set_target_properties(${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  # Reconfigure when the data changes: the texts are embedded at configure time.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HORDE_RACK_DIR}/modules.json" ${ARG_LEDGERS})
endfunction()
