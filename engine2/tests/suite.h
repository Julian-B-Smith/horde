#pragma once
// The suite's registry: an item is a name and a function that returns true when it holds.
// Items register themselves from a static initialiser (HORDE_SUITE_ITEM), so adding one is adding
// a file under tests/items/ and nothing else.
#include <vector>

namespace horde_suite {

struct Item {
  const char* name;
  bool (*run)();
};

// A function-local static, because static initialisers in different files run in no fixed order
// and a namespace-scope vector could be used before it is constructed.
inline std::vector<Item>& registry() {
  static std::vector<Item> items;
  return items;
}

struct Register {
  explicit Register(Item item) { registry().push_back(item); }
};

}  // namespace horde_suite

#define HORDE_SUITE_ITEM(item_name, function) \
  static const ::horde_suite::Register horde_suite_registration_##function({item_name, function})
