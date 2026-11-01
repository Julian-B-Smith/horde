# Info box (ADR-335 r4, ADR-345)

One box, one size, one place, so reading about a control never moves the layout. **The host owns its size and what it says.** The box owns the lock and the auto-scroll.

`Horde.InfoBox(el, S, opts)` returns `{ show(content), lock(content), release(), locked(), step(dt), parts, el }`. Content is `{ name, tag, key, text, meta, live }`; `show(null)` is `opts.idle`. Pass the same object again to refresh a live value without moving the scroll.

**The lock (ADR-345 r2).** The host calls `lock(content)` on any selection, including a press that goes on to drag. A `show` is then ignored until the next `lock` or a release, because the pointer has to cross other controls on its way to the box. A LOCKED mark and a Release button sit in the head. The host calls `release()` on a press outside any control. Escape releases too, and its layer sits behind map and pin and in front of keyboard mode, so Escape still leaves MAP first. `opts.onRelease` tells the host, so it can show what is under the pointer.

**The auto-scroll (ADR-345 r3).** When the description (`text`) runs past the box's edge, it starts after `opts.delay` (2 s) at `opts.speed` (8 px/s) and stops once the description is all in view. The `meta` line under it can stay cut off: a box whose only overflow is that line doesn't move. It stops for good, for this content, when the pointer enters the box or the scroll moves any other way, and it never runs under reduced motion. It runs on the session's frame time, and `step(dt)` lets a test drive it.

**Rules** The host gives the box a fixed size and keeps it there on every page. A hover must never resize anything, so the size never follows the text. A host should keep what is showing while the pointer is on the box, or the box clears before it can be scrolled.

**Sources** ADR-335 ruling 4, ADR-345, `docs/design/foot-row-lab.html` (F1).
