#!/usr/bin/env python3
"""Verifies every binding against the frozen sqlite.def export list.

For each folder under bindings/ it greps the source files for the 34
public symbols and reports the missing ones. A language is "complete"
when every symbol appears.

Usage: python verify_symbols.py [--root path-to-repo]
Exit code 0 = every binding covers every symbol.
"""

import os
import re
import sys

EXTENSIONS = (".py", ".cs", ".go", ".rs", ".js", ".dart", ".java", ".R",
              ".hpp", ".cpp", ".swift", ".kt", ".pas", ".vb", ".d", ".nim",
              ".zig", ".rb", ".php", ".lua", ".pl", ".jl", ".f", ".f90",
              ".hs", ".tcl", ".m", ".prg", ".cmd", ".ps1")

SOURCE_FILES = ("binding.py", "libsqlite.go", "LibSqlite.cs", "LibSqlite.hs",
                "libsqlite.d", "libsqlite.nim", "libsqlite.zig", "libsqlite.R",
                "libsqlite.jl", "libsqlite.f90", "libsqlite.tcl", "libsqlite.m")


def symbols_from_def(repo_root):
    def_path = os.path.join(repo_root, "src", "sqlite", "core", "sqlite.def")
    with open(def_path, "r", encoding="utf-8") as fh:
        text = fh.read()
    names = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith(("LIBRARY", "EXPORTS", ";")):
            continue
        m = re.match(r"([A-Za-z_][A-Za-z0-9_]*)", line)
        if m:
            names.append(m.group(1))
    return names


def binding_files(root, language):
    folder = os.path.join(root, "bindings", language)
    files = []
    for dirpath, _dirs, names in os.walk(folder):
        for n in sorted(names):
            if n.lower().endswith(EXTENSIONS):
                files.append(os.path.join(dirpath, n))
    return files


def main():
    repo_root = sys.argv[sys.index("--root") + 1] if "--root" in sys.argv else os.getcwd()
    symbols = symbols_from_def(repo_root)
    print(f"sqlite.def export list: {len(symbols)} symbols\n")

    if not os.path.isdir(os.path.join(repo_root, "bindings")):
        print("no bindings/ folder", file=sys.stderr)
        return 2

    languages = sorted(d for d in os.listdir(os.path.join(repo_root, "bindings"))
                       if os.path.isdir(os.path.join(repo_root, "bindings", d)))
    worst = 0
    for lang in languages:
        files = binding_files(repo_root, lang)
        if not files:
            print(f"{lang:<12} (no source files)")
            continue
        blob = ""
        for f in files:
            with open(f, "r", encoding="utf-8", errors="replace") as fh:
                blob += fh.read() + "\n"
        # Strip only line comments: a cgo preamble keeps its declarations
        # inside /* */ blocks, so those must stay.
        blob = re.sub(r"(?m)^\s*(//|#(?!\w+\s+import)).*$", "", blob)
        missing = [s for s in symbols if s not in blob]
        status = "OK " if not missing else f"{len(missing)} MISSING"
        print(f"{lang:<12} {len(files)} file(s)  {status}")
        for m in missing:
            print(f"    - {m}")
        worst = max(worst, len(missing))

    print()
    if worst == 0:
        print("every binding covers all 34 exported symbols")
        return 0
    print("bindings are incomplete (see the lists above)")
    return 1


if __name__ == "__main__":
    sys.exit(main())