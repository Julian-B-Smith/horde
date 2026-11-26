# The test module's adapter (ROADMAP B714): tools/rack/test/passthrough_adapter.cpp, horde's own, no sibling tree.
# Included once by horde_rack_module (see weir.cmake for its variables).
add_library(${RACK_ADAPTER_TARGET} STATIC ${HORDE_RACK_ROOT}/tools/rack/test/passthrough_adapter.cpp)
target_link_libraries(${RACK_ADAPTER_TARGET} PUBLIC horde_rack_iface)
set_target_properties(${RACK_ADAPTER_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
