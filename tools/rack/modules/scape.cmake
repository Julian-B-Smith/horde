# Scape's adapter (ROADMAP B714, B710): Scape's own core/ built by ITS OWN core/CMakeLists.txt (EXCLUDE_FROM_ALL: only
# scape_core, which the adapter links), so its arithmetic flags are Scape's (-ffp-contract=fast, Scape Q-026, PRIVATE
# to scape_core): the module played is the module admitted (tools/admission_check.py at the same pin). The binary
# directory is under the consumer's build: nothing is written into Scape's tree. Then tools/admission/scape_adapter.cpp,
# the same adapter the admission harness runs. Included once by horde_rack_module (see weir.cmake for its variables).
add_subdirectory(${RACK_MODULE_DIR}/core ${CMAKE_BINARY_DIR}/rack-scape EXCLUDE_FROM_ALL)
get_target_property(rack_scape_opts scape_core COMPILE_OPTIONS)
string(FIND "${rack_scape_opts}" "-ffp-contract=fast" rack_scape_fp)
if(rack_scape_fp EQUAL -1)
  message(FATAL_ERROR "scape_core is not built with Scape's own -ffp-contract=fast (Scape Q-026): the module played would not be the module admitted")
endif()
add_library(${RACK_ADAPTER_TARGET} STATIC ${HORDE_RACK_ROOT}/tools/admission/scape_adapter.cpp)
target_link_libraries(${RACK_ADAPTER_TARGET} PUBLIC scape_core horde_rack_iface)
set_target_properties(${RACK_ADAPTER_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
