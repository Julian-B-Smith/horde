# Weir's adapter (ROADMAP B714): Weir's header-only SVF 12 (include/weir/svf.h, at weir.pin) behind horde's operator,
# tools/admission/weir_adapter.cpp. Included once by horde_rack_module with RACK_MODULE_DIR (the checked tree),
# RACK_ADAPTER_TARGET (the target to define) and HORDE_RACK_ROOT set. No flags of its own: Weir declares none.
add_library(${RACK_ADAPTER_TARGET} STATIC ${HORDE_RACK_ROOT}/tools/admission/weir_adapter.cpp)
target_include_directories(${RACK_ADAPTER_TARGET} PRIVATE ${RACK_MODULE_DIR}/include)
target_link_libraries(${RACK_ADAPTER_TARGET} PUBLIC horde_rack_iface)
set_target_properties(${RACK_ADAPTER_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
