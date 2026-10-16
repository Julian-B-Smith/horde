# The WebAssembly build (stub)

The labs load the engine as WebAssembly, from the same source, with the same API (spec IF-10;
acceptance item 15 compares its render with the native build's). It is built with **one pinned
toolchain version**.

**Status: not available.** The toolchain awaits the human's approval of an exact version
(ADR-225 item 10). Nothing here installs one. `CMakeLists.txt` in this directory refuses to
configure, so `-DHORDE_ENGINE2_WASM=ON` is an error today, on purpose.

When the version is approved, this directory gets: the entry the labs load (an AudioWorklet
wrapper over the engine's API, no engine code of its own), the toolchain file naming the approved
version, and the build target. Build stage 8 (spec BD-8) is where that happens.
