# The clean-room recipe

The builder works in a tree that holds this directory and the spec, and nothing else (spec BD-10).
The tree is a separate git worktree with a sparse checkout, so it is the same repository at the
same commit, only with every other path left out.

## Make the builder's tree

From a checkout of the repository:

```
tools/engine2_cleanroom.sh /absolute/path/for/the/tree [<ref>]
```

`<ref>` defaults to `HEAD`. The path must not exist yet. The script lists the sparse patterns in
its own header; they are the whole recipe:

| pattern | why |
|---|---|
| `/engine2/` | the new engine's tree: the spec's counterpart in code, the input data, the click detector, the build scaffolding |
| `!/engine2/data/reference-renders/`, `!/engine2/data/version0-states/` | the test author's input data; the builder does not receive it (spec 10.1, IF-4) |
| `/specs/SPEC-ENGINE.md` | the spec, the only reference |

The CLAP headers are not in the recipe: the engine's API is plain data in the spec's units, and
CLAP belongs to the plugin shell. Cone-mode sparse checkout is not used, because it always
materialises every file at the repository root.

**What a sparse checkout does not do.** It filters the files on disk, not the repository behind
them: the git history and the index of that worktree still name every other path, and
`git show <commit>:<path>` still reads it. The recipe keeps the tree clean; it does not stop an
agent that runs git against the history. The builder's instructions must say not to, and a
history-free copy (an export of the two paths, or a one-commit repository) is the stricter option
if that is wanted.

Build and test inside it:

```
engine2/build.sh /absolute/path/to/a/build/dir
```

Remove it with `git worktree remove --force /absolute/path/for/the/tree`.

## The check

`tools/engine2_cleanroom_check.py` runs on every `./verify fast`. It makes a fresh recipe tree and
fails if the tree holds any path outside `engine2/` and the spec, anything from the denied
directories of the old engine, any file named like the old engine's twin or prototypes, a symlink,
or any file whose text carries one of the old engine's identifiers. Its must-fail controls (a recipe
variant that adds the old engine's directory, one that adds a source file, one that hands the builder
the reference renders, and a planted identifier, name, root file and symlink) run every time. It
also keeps `tools/click_detector.mjs` here byte-identical to the tools copy, and the envelope data
equal to the spec's Appendix A. Run it alone with `python3 tools/engine2_cleanroom_check.py`
(`--build` also builds and tests the recipe's tree).
