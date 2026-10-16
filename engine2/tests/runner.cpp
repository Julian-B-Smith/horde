// The suite's runner: runs every registered item and exits 0 only if all of them hold.
// An empty suite is green and says so; the runner's ability to fail is proved by the
// runner_control test (tests/controls/), not assumed.
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "suite.h"

int main() {
  auto& items = horde_suite::registry();
  // Sorted by name: registration order is the linker's, and a report whose order moves between
  // builds cannot be diffed.
  std::sort(items.begin(), items.end(), [](const horde_suite::Item& a, const horde_suite::Item& b) {
    return std::strcmp(a.name, b.name) < 0;
  });

  int failed = 0;
  for (const auto& item : items) {
    const bool held = item.run();
    std::printf("%s  %s\n", held ? "PASS" : "FAIL", item.name);
    if (!held) ++failed;
  }
  std::printf("suite: %zu items, %d failed\n", items.size(), failed);
  return failed == 0 ? 0 : 1;
}
