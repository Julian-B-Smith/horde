/*
 * plugin_identity.h — WHO this build is: to a host, and on the user's disk (B456).
 *
 * The one place that spells the CLAP id, the display name and the per-user
 * store folder. The descriptor (hypersaw_clap.cpp) and the store path
 * (gui/preset_store.h) read kIdentity and nothing else, so a build cannot be
 * one plugin to the host and another on disk.
 *
 * TWO IDENTITIES, ONE SOURCE TREE. kReal is the shipped plugin. kTest is the
 * same code under a second identity that installs BESIDE it, for trying
 * unmerged changes in a real host without replacing the plugin real projects
 * load. It is chosen by ONE compile definition, HORDE_TEST_IDENTITY, which
 * CMake sets from the option of the same name (default OFF). The identity
 * values CMake itself consumes (bundle identifier, AU subtype, output file
 * name) sit in CMakeLists.txt's identity block beside that option;
 * tools/test_identity_check.py holds the two files to each other.
 *
 * Deliberately dependency-free, like preset_store.h, which includes it: no
 * standard header, so every consumer of the store path can include it.
 */
#pragma once

namespace hypersaw::identity
{

struct Identity
{
  const char *clapId;       // what a host stores to re-find the plugin; the VST3 UID derives from it
  const char *displayName;  // what a host's browser shows; free to change
  const char *storeFolder;  // leaf of the per-user store path (preset_store.h presetRootFor)
};

/* FROZEN. `com.lifted-truck.hypersaw` is how every host re-finds this plugin in
   an already-saved session (see the descriptor's comment in hypersaw_clap.cpp),
   and HYPERSAW is the folder the user's presets already live in. Changing either
   orphans saved projects or the preset library. tools/test_identity_check.py
   pins all three values, so an edit here is red. */
inline constexpr Identity kReal = {"com.lifted-truck.hypersaw", "horde", "HYPERSAW"};

/* The side-by-side test identity. Every field differs from kReal's, which is
   what lets both be installed at once and keeps the test build out of the real
   preset folder. Not frozen: nothing saved against it is meant to last. */
inline constexpr Identity kTest = {"com.lifted-truck.hypersaw.test", "horde TEST", "HYPERSAW-TEST"};

/* The ONLY #ifdef on identity in the tree. #ifdef, not #if: CMake defines the
   macro only when the option is ON, so "defined" is the whole signal. */
inline constexpr const Identity &kIdentity =
#ifdef HORDE_TEST_IDENTITY
    kTest;
#else
    kReal;
#endif

/* Compile-time form of the rule the check enforces from outside: the two
   identities share no field. A test build that reused the real id would be
   loaded IN PLACE of the real plugin by a host, and one that reused the store
   folder would write the user's presets. */
constexpr bool sameText(const char *a, const char *b)
{
  for (; *a && *a == *b; ++a, ++b) {}
  return *a == *b;
}
static_assert(!sameText(kReal.clapId, kTest.clapId), "the test identity reuses the real CLAP id");
static_assert(!sameText(kReal.displayName, kTest.displayName), "the test identity reuses the real name");
static_assert(!sameText(kReal.storeFolder, kTest.storeFolder), "the test identity reuses the real store folder");

}  // namespace hypersaw::identity
