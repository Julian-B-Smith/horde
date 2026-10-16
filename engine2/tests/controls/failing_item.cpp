// The runner's must-fail control: linked with the runner and nothing else, this one item fails,
// so the executable must exit nonzero. CMake registers it with WILL_FAIL; it is never part of the suite.
#include "suite.h"

static bool always_fails() { return false; }
HORDE_SUITE_ITEM("control: a failing item turns the runner red", always_fails);
