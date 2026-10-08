#!/usr/bin/env python3
"""Byte-array embeds for build-time resources. Two modes:

  embed_file.py <input> <output.h> <symbol>
      one file (the GUI HTML). Hex array rather than a raw string literal so
      the content can never collide with a delimiter or hit MSVC's
      string-length cap. An .html input gets a Content-Security-Policy meta
      inserted first (with_csp, below): the embedded page may run only the
      inline scripts it was built with, and may load nothing from anywhere.

  embed_file.py --bank <dir> <output.h> <symbol>
      every *.json under <dir>, recursively, as one hypersaw::FactoryFile
      array plus a <symbol>_version string (B129). The version is a digest of
      the bank's own contents, so adding or editing a preset bumps it without
      anyone remembering to; the install path uses it to decide whether the
      bank on disk is already current. An EMPTY or ABSENT <dir> is legal and
      yields a zero-length array — the bank content lands separately (B130)
      and the build must not depend on it existing.
"""
import base64
import hashlib
import os
import re
import sys

# THE GUI PAGE'S POLICY (B446). Every inline <script> is allowed by its sha256,
# computed below from the bytes being embedded, so the policy cannot drift from
# the page: editing a script changes its hash in the same build. A script added
# at run time (by markup injection or a DOM call) has no hash and does not run.
# The rest is the minimum the page uses, measured by tools/gui_webview_check on
# the real WKWebView:
#   style-src 'unsafe-inline'  the page is styled by one <style> block plus
#                              style="" attributes; styles cannot run script.
#   everything else 'none'     no images, fonts, frames, network or forms.
# The bridge is not affected: choc's bindings are WKUserScripts and its replies
# arrive through evaluateJavaScript, and the engine runs neither under the
# page's policy (checked by the same probe: every binding still resolves).
# tools/gui_sink_check.py asserts the policy's shape on both GUI pages.
CSP_TEMPLATE = ("default-src 'none'; script-src {hashes}; style-src 'unsafe-inline'; "
                "base-uri 'none'; form-action 'none'")

_SCRIPT_RE = re.compile(r"<script\b([^>]*)>(.*?)</script\s*>", re.I | re.S)
_STYLE_RE = re.compile(r"<style\b[^>]*>.*?</style\s*>", re.I | re.S)
# An inline handler attribute (onclick="…") would be refused by the policy at
# run time and the control would silently do nothing, so the build refuses it
# instead. Matched in MARKUP only (scripts and styles are blanked first), where
# ` on<letters>=` can only be an attribute.
_HANDLER_RE = re.compile(r"<[a-z][^>]*\son[a-z]+\s*=", re.I | re.S)


def csp_for(html):
    """The policy for `html` (str): one sha256 source per inline script."""
    hashes = []
    for m in _SCRIPT_RE.finditer(html):
        # The HTML parser turns CR LF and lone CR into LF before a script's text
        # exists, and the browser hashes that text; so must we.
        body = m.group(2).replace("\r\n", "\n").replace("\r", "\n")
        digest = hashlib.sha256(body.encode("utf-8")).digest()
        hashes.append("'sha256-" + base64.b64encode(digest).decode("ascii") + "'")
    return CSP_TEMPLATE.format(hashes=" ".join(hashes) if hashes else "'none'")


def with_csp(html):
    """`html` with the policy meta inserted where it governs the whole page:
    right after <meta charset>, or right after <head>. Raises ValueError on a
    page the policy would break or could not cover — the build stops rather
    than ship a GUI with dead controls or an unhashed script."""
    if re.search(r"http-equiv\s*=\s*[\"']?content-security-policy", html, re.I):
        raise ValueError("page already carries a Content-Security-Policy; embed_file.py owns it")
    for m in _SCRIPT_RE.finditer(html):
        if re.search(r"\bsrc\s*=", m.group(1), re.I):
            raise ValueError("external <script src> in the GUI page; it must be inline and embedded")
    markup = _STYLE_RE.sub("", _SCRIPT_RE.sub("", html))
    if _HANDLER_RE.search(markup):
        raise ValueError("inline event-handler attribute (on…=) in the GUI page; "
                         "bind it with addEventListener inside the script")
    if re.search(r"javascript\s*:", markup, re.I):
        raise ValueError("javascript: URL in the GUI page markup")
    meta = '<meta http-equiv="Content-Security-Policy" content="' + csp_for(html) + '">'
    anchor = re.search(r"<meta\s+charset\s*=[^>]*>", html, re.I) or re.search(r"<head\b[^>]*>", html, re.I)
    if not anchor:
        raise ValueError("no <head> or <meta charset> to anchor the policy before any script")
    if _SCRIPT_RE.search(html[:anchor.end()]):
        raise ValueError("a <script> precedes the policy anchor and would run unpoliced")
    return html[:anchor.end()] + meta + html[anchor.end():]


def emit_bytes(f, sym, data):
    f.write(f"static const unsigned char {sym}[] = {{\n")
    for i in range(0, len(data), 24):
        f.write("  " + ",".join(str(b) for b in data[i:i + 24]) + ",\n")
    f.write("};\n")


def single(src, dst, sym):
    data = open(src, "rb").read()
    if src.lower().endswith(".html"):
        try:
            data = with_csp(data.decode("utf-8")).encode("utf-8")
        except ValueError as e:
            sys.exit(f"embed_file.py: {src}: {e}")
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("// generated by tools/embed_file.py — do not edit\n#pragma once\n#include <cstddef>\n")
        emit_bytes(f, sym + "_data", data)
        f.write(f"static const size_t {sym}_size = {len(data)};\n")


def bank(root, dst, sym):
    # Sorted so the generated header — and therefore the version digest — is a
    # function of the CONTENT alone, never of directory-walk order.
    files = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for fn in sorted(filenames):
            if fn.endswith(".json"):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                files.append((rel, full))
    files.sort()

    digest = hashlib.sha256()
    entries = []
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("// generated by tools/embed_file.py --bank — do not edit\n#pragma once\n")
        f.write('#include <cstddef>\n#include "gui/preset_store.h"\n')
        for i, (rel, full) in enumerate(files):
            data = open(full, "rb").read()
            digest.update(rel.encode("utf-8"))
            digest.update(b"\0")
            digest.update(str(len(data)).encode("ascii"))
            digest.update(b"\0")
            digest.update(data)
            category, _, name = rel.rpartition("/")
            name = name[:-len(".json")]
            emit_bytes(f, f"{sym}_b{i}", data)
            entries.append((category, name, f"{sym}_b{i}", len(data)))
        f.write(f"static const hypersaw::FactoryFile {sym}[] = {{\n")
        for category, name, symbol, size in entries:
            # C++ forbids a zero-length array; a dummy keeps the empty bank
            # compiling, and _count 0 keeps it inert.
            f.write(f'  {{"{category}", "{name}", {symbol}, {size}}},\n')
        if not entries:
            f.write('  {"", "", nullptr, 0},\n')
        f.write("};\n")
        f.write(f"static const size_t {sym}_count = {len(entries)};\n")
        f.write(f'static const char {sym}_version[] = "{digest.hexdigest()[:16]}";\n')


if __name__ == "__main__":
    if sys.argv[1] == "--bank":
        bank(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        single(sys.argv[1], sys.argv[2], sys.argv[3])
