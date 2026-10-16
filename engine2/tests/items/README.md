# Suite items

Empty on purpose. Each `*.cpp` file here is a suite item: it includes `suite.h`, defines a
`bool` function that returns true when the check holds, and registers it with
`HORDE_SUITE_ITEM("name", function)`. CMake picks the file up on the next build; nothing else is
edited. The spec's section 10.1 says what the items must prove, and every item has a must-fail
control (a planted fault that turns it red).

The test author writes the items from the spec, before the engine exists.
