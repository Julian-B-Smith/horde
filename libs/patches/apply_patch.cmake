# apply_patch.cmake — copy a vendored tree into the build tree and apply its
# patches to the copy, in order, or stop the configure. Script mode:
#
#   cmake -DSRC=<tree to copy> -DOUT=<build-tree dir> "-DPATCH=<a.patch>[;<b.patch>...]" -P apply_patch.cmake
#
# PATCH is a CMake list (quote the -D argument, or `;` splits it in the caller's
# COMMAND). Each patch is checked and applied to the copy the previous one left,
# so a later patch must apply on top of the earlier ones (B446, ADR-196).
#
# The copy lands at OUT/<last component of SRC>, so SRC=libs/choc/choc gives
# OUT/choc/..., and `#include <choc/...>` resolves to the patched copy once OUT
# is on the include path. The submodule's working tree is never written: its
# pin stays the upstream commit, and `git status` stays clean.
#
# Called by CMakeLists.txt at configure time and by tools/choc_patch_check.py,
# so the check exercises this mechanism rather than a copy of it.
#
# FAILS LOUDLY, never silently: a patch that no longer applies (an upstream bump
# moved its context, or upstream already contains it) is a FATAL_ERROR, and so
# is a run that reports success but leaves a target file unchanged.
#
# Line endings: the patch is stored LF (.gitattributes). A submodule checked out
# under core.autocrlf=true (Git for Windows' default; the submodule has no
# .gitattributes of its own) has CRLF files, which an LF patch cannot match, so
# each file the patch names is normalised to LF in the copy before applying.
# `git apply` is used as a plain patch tool here; it needs no repository.
cmake_minimum_required(VERSION 3.16)

foreach(v SRC OUT PATCH)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "apply_patch.cmake: -D${v}=... is required")
  endif()
endforeach()

if(NOT IS_DIRECTORY "${SRC}")
  message(FATAL_ERROR "apply_patch.cmake: ${SRC} is missing. If it is a submodule, "
                      "run: git submodule update --init --recursive")
endif()
if(PATCH STREQUAL "")
  message(FATAL_ERROR "apply_patch.cmake: -DPATCH names no patch")
endif()
foreach(p IN LISTS PATCH)
  if(NOT EXISTS "${p}")
    message(FATAL_ERROR "apply_patch.cmake: patch ${p} is missing")
  endif()
endforeach()

find_program(APPLY_PATCH_GIT git)
if(NOT APPLY_PATCH_GIT)
  message(FATAL_ERROR "apply_patch.cmake: git is required to apply ${PATCH}")
endif()

# A fresh copy every run, so re-running a configure never applies twice.
file(REMOVE_RECURSE "${OUT}")
file(MAKE_DIRECTORY "${OUT}")
file(COPY "${SRC}" DESTINATION "${OUT}")

# One pass per patch, in list order, each against the copy as the previous
# pass left it. The CRLF normalisation is a no-op after the first pass.
foreach(p IN LISTS PATCH)
  # The files the patch writes, from its `+++ b/<path>` lines.
  file(STRINGS "${p}" targets REGEX "^\\+\\+\\+ b/")
  if(NOT targets)
    message(FATAL_ERROR "apply_patch.cmake: ${p} names no target file (+++ b/...)")
  endif()
  set(rels "")
  foreach(line IN LISTS targets)
    string(REGEX REPLACE "^\\+\\+\\+ b/([^\t]*).*$" "\\1" rel "${line}")
    if(NOT EXISTS "${OUT}/${rel}")
      message(FATAL_ERROR "apply_patch.cmake: ${p} targets ${rel}, which the copy of ${SRC} does not have")
    endif()
    # Rewritten unconditionally. A test for "\r\n" in `text` never fires: file(READ)
    # has already dropped the CR of each CRLF pair (CMake 4.1, measured 2026-10-08),
    # so the CRLF file used to be left as it was and the patch did not apply. The
    # REPLACE covers a CMake that keeps the CRs. An LF file round-trips unchanged.
    file(READ "${OUT}/${rel}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    file(WRITE "${OUT}/${rel}" "${text}")
    file(SHA256 "${OUT}/${rel}" before_${rel})
    list(APPEND rels "${rel}")
  endforeach()

  # --whitespace=nowarn: the result must not depend on the user's apply.whitespace.
  execute_process(COMMAND "${APPLY_PATCH_GIT}" apply --check --whitespace=nowarn "${p}"
                  WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE rc
                  OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "apply_patch.cmake: ${p} NO LONGER APPLIES to ${SRC}.\n"
                        "${out}${err}\n"
                        "The vendored tree changed under the patch. Read the patch's header "
                        "(its removal condition) and libs/patches/README.md: either upstream "
                        "now provides the change (remove the patch) or the patch must be "
                        "re-made against the new pin. Never build without it.")
  endif()
  execute_process(COMMAND "${APPLY_PATCH_GIT}" apply --whitespace=nowarn "${p}"
                  WORKING_DIRECTORY "${OUT}" RESULT_VARIABLE rc
                  OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "apply_patch.cmake: git apply ${p} failed:\n${out}${err}")
  endif()

  # `git apply` run inside some repository's subdirectory skips paths it judges to
  # lie outside it, and still exits 0. A target that did not change is therefore
  # a failure, not a success.
  foreach(rel IN LISTS rels)
    file(SHA256 "${OUT}/${rel}" after)
    if(after STREQUAL before_${rel})
      message(FATAL_ERROR "apply_patch.cmake: git apply exited 0 but ${rel} is unchanged")
    endif()
  endforeach()
endforeach()
