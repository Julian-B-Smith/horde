# The WebAssembly target: a stub that refuses to configure (spec IF-10; ADR-225 item 10).
# The toolchain is built with one pinned version, and the human approves the exact version
# before any is installed or named here. Until then, asking for this target is an error, so no
# build can quietly proceed without one.
message(FATAL_ERROR
  "engine2/wasm: the WebAssembly toolchain version has not been approved by the human (see wasm/README.md)")
