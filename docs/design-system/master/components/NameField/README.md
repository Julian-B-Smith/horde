# Name field

Asks for a name in the page (ADR-319 r1). A plugin's webview may have no `window.prompt`, and a browser dialog stops everything else.

**The consumer provides** a host, and `{ max }`. `open({ label, value, save, onSave, onCancel })` shows the field with the value selected. `close()` and `isOpen()` do what they say.

**Anatomy** a label, a text input in a capsule (`well`, the ink stroke, `size-hit` tall), a Save button named by the request, and Cancel. Empty or spaces can't be saved, and Save is disabled until there's a name.

**Gestures** Enter saves and Escape cancels. A key typed here never reaches the page's keyboard mode.

**Sources** the shell's save buttons, which used `window.prompt` (`docs/design/gui3.html`, ADR-319).

The preview draws SAMPLE names.
