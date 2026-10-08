#!/usr/bin/env python3
"""deny_push.py -- the PreToolUse(Bash) policy behind pretool-deny.sh (B446, ADR-194 D-S6).

Reads the hook JSON on stdin. Exit 0 allows the command; exit 2 blocks it and the
stderr line is shown to the agent. FAIL CLOSED: input that cannot be parsed, a Bash
call with no command, or any internal error blocks. An allow-by-default on a parse
failure was the old hook's hole (it read malformed input as an empty command).

WHY A PARSER. The old hook grepped the raw string, so `git push origin --force
HEAD:main`, `git -C . push -f`, a `:branch` delete and flag-order variants all
passed. This tokenises with shell-word rules (shlex, punctuation-aware), splits the
chain on && || ; | and newlines, and judges EVERY git invocation in it, after env
assignments, wrapper words and git's own global options.

WHAT IS BLOCKED (each with a reason on stderr):
- git push that could write main/master, any tag, or delete anything: `main`,
  `HEAD:main`, `refs/heads/main`, a `+` (force) or `:dst` (delete) refspec, a
  wildcard refspec, `--force*`, `-f` (also inside a short-flag cluster), `--mirror`,
  `--all`, `--tags`, `--follow-tags`, `--delete`/`-d`, `--prune`, `--receive-pack`,
  `--exec`. A push with no refspec, or `HEAD`, pushes the CURRENT branch, so the
  branch is resolved in the command's directory (hook cwd, then any `cd X` earlier
  in the chain, then `git -C X`); main, master or an unresolvable branch blocks.
- git config / git -c that redirects pushes or hooks: remote.*.url, remote.*.pushurl,
  url.*.insteadOf / pushInsteadOf, core.hooksPath, alias.*; and `git remote set-url`.
- a git push the parser cannot see through: inside `sh|bash|zsh -c`, `eval`,
  `$(...)`, backticks or `xargs` (those are parsed recursively where the text is
  literal, and blocked when it is not), or inside `python -c` / `node -e` / `perl -e`
  code that mentions both git and push.
- gh pr merge, gh repo delete (agents never merge; the human does).
- the old hook's destructive list, made flag-order-proof: recursive+force rm of /,
  ~ or $HOME; git reset --hard; git clean with -f and -d in any order; chmod -R 777;
  curl/wget piped to a shell or interpreter; a redirect onto /dev/sd*.

ALLOWED, deliberately: pushing a named feature branch (`git push -u origin feat`,
`git push origin feat:feat`, `git push origin HEAD` off main) -- the lead's and the
agents' daily workflow. Everything not matched above passes to settings.json's own
allow/deny rules, which still apply.

STATED LIMITS: a git alias defined outside this command, a script FILE that pushes,
and a push assembled from variables are not seen; the GitHub ruleset on main is the
server-side backstop for exactly those (B446 Tier A). tools/deny_hook_check.py runs a
table of must-block and must-allow commands against this hook in ./verify fast.
"""
import json
import os
import re
import shlex
import subprocess
import sys

PROTECTED = {"main", "master"}
SEPARATORS = {"&&", "||", ";", "|", "&", "\n", ";;", "|&"}
WRAPPERS = {"command", "builtin", "exec", "nohup", "time", "env", "sudo", "nice"}
GIT_OPT_WITH_ARG = {"-C", "-c", "--git-dir", "--work-tree", "--namespace", "--exec-path",
                    "--super-prefix", "--config-env"}
PUSH_BAD_LONG = ("--force", "--mirror", "--all", "--tags", "--follow-tags", "--delete",
                 "--prune", "--receive-pack", "--exec", "--force-with-lease",
                 "--force-if-includes")
PUSH_OPT_WITH_ARG = {"-o", "--push-option", "--repo", "--receive-pack", "--exec"}
CONFIG_BAD = re.compile(r"^(remote\..+\.(url|pushurl)|url\..+\.(insteadof|pushinsteadof)"
                        r"|core\.hookspath|alias\..+)$", re.I)
SHELLS = {"sh", "bash", "zsh", "dash", "ksh"}
CODE_RUNNERS = {"python", "python3", "node", "perl", "ruby"}


class Block(Exception):
    pass


def tokens(cmd):
    lex = shlex.shlex(cmd, posix=True, punctuation_chars=";&|")
    lex.whitespace_split = True
    lex.commenters = ""
    out = []
    for t in lex:
        out.append(t)
    return out


def segments(cmd):
    """Split into simple commands; newlines separate commands too."""
    segs, cur = [], []
    for line in cmd.replace("\r", "\n").split("\n"):
        try:
            toks = tokens(line)
        except ValueError:
            # An unbalanced quote across lines (a heredoc body, a multi-line string):
            # parse the whole command once instead; if that fails too, fail closed.
            toks = None
        if toks is None:
            try:
                toks = tokens(cmd)
            except ValueError:
                raise Block("command could not be parsed (unbalanced quotes)")
            segs, cur = [], []
            for t in toks:
                if t in SEPARATORS:
                    if cur:
                        segs.append(cur)
                    cur = []
                else:
                    cur.append(t)
            if cur:
                segs.append(cur)
            return segs
        for t in toks:
            if t in SEPARATORS:
                if cur:
                    segs.append(cur)
                cur = []
            else:
                cur.append(t)
        if cur:
            segs.append(cur)
        cur = []
    return segs


def mentions_push(text):
    return re.search(r"\bgit\b", text) is not None and re.search(r"\bpush\b", text) is not None


def current_branch(directory):
    try:
        r = subprocess.run(["git", "-C", directory, "symbolic-ref", "--short", "-q", "HEAD"],
                           capture_output=True, text=True, timeout=5)
    except Exception:
        return None
    b = r.stdout.strip()
    return b if r.returncode == 0 and b else None


def strip_ref(ref):
    if ref.startswith("refs/heads/"):
        return ref[len("refs/heads/"):]
    return ref


def judge_push(args, directory):
    positional = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--":
            positional.extend(args[i + 1:])
            break
        if a.startswith("--"):
            name = a.split("=", 1)[0]
            if name.startswith(PUSH_BAD_LONG):
                raise Block(f"git push {name} is not allowed")
            if name in PUSH_OPT_WITH_ARG and "=" not in a:
                i += 1
            i += 1
            continue
        if a.startswith("-") and len(a) > 1:
            if a in PUSH_OPT_WITH_ARG:
                i += 2
                continue
            cluster = a[1:]
            if "f" in cluster or "d" in cluster:
                raise Block(f"git push {a} (force or delete) is not allowed")
            i += 1
            continue
        positional.append(a)
        i += 1
    # A remote or refspec built from a command substitution cannot be judged before
    # bash expands it (`git push origin $(echo main)`), so it blocks. Name the
    # branch literally instead.
    if any("__SUBST__" in a for a in positional):
        raise Block("git push with a target built from a command substitution cannot be judged")
    refspecs = positional[1:]
    if not refspecs:
        refspecs = ["HEAD"]
    for spec in refspecs:
        if spec.startswith("+"):
            raise Block(f"git push {spec}: a '+' refspec is a force push")
        if "*" in spec:
            raise Block(f"git push {spec}: wildcard refspecs are not allowed")
        if ":" in spec:
            src, dst = spec.split(":", 1)
            # `:dst` (empty SOURCE) is git's delete-remote-branch spelling; an empty
            # destination is malformed. Both block.
            if not src or not dst:
                raise Block(f"git push {spec}: an empty side deletes or is malformed")
        else:
            src, dst = spec, spec
        if src.startswith("refs/tags/") or dst.startswith("refs/tags/"):
            raise Block(f"git push {spec}: pushing tags is not allowed")
        if dst == "HEAD" or (":" not in spec and src == "HEAD"):
            b = current_branch(directory)
            if b is None:
                raise Block("git push of the current branch: branch could not be resolved")
            dst = b
        if strip_ref(dst) in PROTECTED:
            raise Block(f"git push to {strip_ref(dst)} is not allowed; open a PR")


def judge_config_pairs(pairs):
    for key in pairs:
        if CONFIG_BAD.match(key):
            raise Block(f"git config {key} could redirect pushes or hooks")


def judge_git(seg, directory):
    i = 1
    cdir = directory
    cfg = []
    while i < len(seg) and seg[i].startswith("-"):
        opt = seg[i]
        name = opt.split("=", 1)[0]
        if name in GIT_OPT_WITH_ARG and "=" not in opt:
            val = seg[i + 1] if i + 1 < len(seg) else ""
            if name == "-C":
                cdir = os.path.join(cdir, os.path.expanduser(val))
            if name == "-c":
                cfg.append(val.split("=", 1)[0])
            i += 2
            continue
        i += 1
    judge_config_pairs(cfg)
    if i >= len(seg):
        return
    sub, rest = seg[i], seg[i + 1:]
    if sub == "push":
        judge_push(rest, cdir)
    elif sub == "config":
        keys = [a for a in rest if not a.startswith("-")]
        if keys:
            judge_config_pairs([keys[0]])
    elif sub == "remote" and rest[:1] == ["set-url"]:
        raise Block("git remote set-url is not allowed")
    elif sub == "reset" and "--hard" in rest:
        raise Block("git reset --hard is not allowed")
    elif sub == "clean":
        flags = "".join(a[1:] for a in rest if a.startswith("-") and not a.startswith("--"))
        longs = set(a for a in rest if a.startswith("--"))
        if ("f" in flags or "--force" in longs) and ("d" in flags):
            raise Block("git clean -f -d is not allowed")


def split_substitutions(text):
    """Return (inners, outer): the text of every LIVE command substitution -- $(...) or
    `...` that bash would actually run -- for recursive judging, and the command with
    each one replaced by a placeholder word. Inside single quotes, and when escaped
    (\\` or \\$), they are literal text (markdown backticks in a PR body) and are left
    alone. Replacing them before tokenising is what lets `"$(cmd -q '...')"` parse:
    shlex knows nothing of bash's fresh quoting context inside $(...), and failing
    closed on every such command would block ordinary agent work."""
    inners, out = [], []
    i, n = 0, len(text)
    squote = dquote = False
    while i < n:
        c = text[i]
        if c == "\\" and not squote:
            out.append(text[i:i + 2])
            i += 2
            continue
        if c == "'" and not dquote:
            squote = not squote
        elif c == '"' and not squote:
            dquote = not dquote
        elif not squote and c == "`":
            j = i + 1
            while j < n and text[j] != "`":
                j += 2 if text[j] == "\\" else 1
            inners.append(text[i + 1:j])
            out.append("__SUBST__")
            i = j + 1
            continue
        elif not squote and text.startswith("$(", i):
            depth, j, sq, dq = 1, i + 2, False, False
            while j < n and depth:
                ch = text[j]
                if ch == "\\" and not sq:
                    j += 2
                    continue
                if ch == "'" and not dq:
                    sq = not sq
                elif ch == '"' and not sq:
                    dq = not dq
                elif not sq and not dq:
                    if text.startswith("$(", j):
                        depth += 1
                        j += 2
                        continue
                    if ch == "(":
                        depth += 1
                    elif ch == ")":
                        depth -= 1
                j += 1
            inners.append(text[i + 2:j - 1])
            out.append("__SUBST__")
            i = j
            continue
        out.append(c)
        i += 1
    return inners, "".join(out)


def inner_texts(text):
    return split_substitutions(text)[0]


HEREDOC = re.compile(r"<<-?\s*(['\"]?)([A-Za-z_][A-Za-z0-9_]*)\1[^\n]*\n.*?\n\s*\2(?=\s|$|\))", re.S)


def strip_heredocs(text):
    """A heredoc body is data (a commit message, a file being written), not commands;
    judging it would block a commit message that merely MENTIONS a push."""
    return HEREDOC.sub(lambda m: m.group(0).split("\n", 1)[0] + "\n", text)


def judge(cmd, cwd, level=0):
    if level > 4:
        raise Block("command nesting too deep to judge")
    cmd = strip_heredocs(cmd)
    raw = cmd
    # The old destructive list, made order-proof on the raw text.
    if re.search(r"\b(curl|wget)\b[^|]*\|\s*(sudo\s+)?(ba|z|da|k)?sh\b", raw) or \
       re.search(r"\b(curl|wget)\b[^|]*\|\s*(sudo\s+)?(python3?|perl|ruby|node)\b", raw):
        raise Block("piping a download into a shell or interpreter is not allowed")
    if re.search(r">\s*/dev/sd", raw):
        raise Block("writing to a raw disk device is not allowed")
    inners, outer = split_substitutions(raw)
    for text in inners:
        judge(text, cwd, level + 1)
    directory = cwd
    for seg in segments(outer):
        # Leading env assignments and wrapper words.
        while seg and (re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", seg[0]) or seg[0] in WRAPPERS):
            seg = seg[1:]
        if not seg:
            continue
        head = os.path.basename(seg[0])
        if head == "cd" and len(seg) > 1:
            directory = os.path.join(directory, os.path.expanduser(seg[1]))
            continue
        if head == "git":
            judge_git(seg, directory)
        elif head in SHELLS and "-c" in seg:
            k = seg.index("-c")
            if k + 1 < len(seg):
                judge(seg[k + 1], directory, level + 1)
        elif head == "eval":
            judge(" ".join(seg[1:]), directory, level + 1)
        elif head == "xargs":
            if mentions_push(" ".join(seg)):
                raise Block("git push through xargs cannot be judged")
        elif head in CODE_RUNNERS:
            if mentions_push(" ".join(seg[1:])):
                raise Block(f"{head} code that runs git push cannot be judged")
        elif head == "gh" and seg[1:3] in (["pr", "merge"], ["repo", "delete"]):
            raise Block(f"gh {' '.join(seg[1:3])} is not allowed; the human does this")
        elif head == "rm":
            flags = "".join(a[1:] for a in seg[1:] if a.startswith("-") and not a.startswith("--"))
            longs = set(a for a in seg[1:] if a.startswith("--"))
            rec = "r" in flags or "R" in flags or "--recursive" in longs
            force = "f" in flags or "--force" in longs
            targets = [a for a in seg[1:] if not a.startswith("-")]
            if rec and force and any(t in ("/", "~", "~/", "$HOME", "${HOME}", "/*", "~/*")
                                     for t in targets):
                raise Block("recursive forced rm of / or ~ is not allowed")
        elif head == "chmod" and "-R" in seg and "777" in seg:
            raise Block("chmod -R 777 is not allowed")


def main():
    try:
        data = json.loads(sys.stdin.read())
        if not isinstance(data, dict):
            raise ValueError("hook input is not an object")
        cmd = (data.get("tool_input") or {}).get("command")
        if data.get("tool_name", "Bash") == "Bash" and not isinstance(cmd, str):
            raise Block("Bash hook input carries no command")
        if not isinstance(cmd, str):
            return 0
        cwd = data.get("cwd") or os.getcwd()
        judge(cmd, cwd)
        return 0
    except Block as e:
        print(f"BLOCKED by harness: {e}. If it is genuinely needed, ask the human to run it.",
              file=sys.stderr)
        return 2
    except Exception as e:  # fail closed on anything unexpected
        print(f"BLOCKED by harness: the deny hook could not judge this command ({e.__class__.__name__}).",
              file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
