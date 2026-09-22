#!/usr/bin/env python3
"""Report dead legacy console code in the tree.

Context
-------
Idea 10 was "remove dead X360/PS3 code to cut compile time and file size".
Measuring first showed that framing was wrong:

  * Neither _X360 nor _PS3 is ever defined by this build (checked against
    wscript), so the preprocessor already drops those blocks.
  * IsX360() and IsPS3() are compile-time false on POSIX, so the optimiser
    already deletes those branches.

So the build is already unaffected. The real cost is comprehension, and the
real deliverable is knowing exactly how much dead weight is there and where.

This script measures it rather than guessing, and --delete-unbuilt can
remove the directories that no build target references at all.

Usage:
  report_legacy_consoles.py            # report only
  report_legacy_consoles.py --delete-unbuilt   # also delete unbuilt xbox dirs
"""
import os, re, sys, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SOURCE_EXT = {".cpp", ".h", ".c", ".hxx", ".cxx", ".hpp", ".inl", ".mm"}
CONSOLE_TOKENS = ["_X360", "_PS3", "_GAMECONSOLE"]


def iter_sources():
    """Walk sources, skipping directories that are never compiled."""
    SKIP_DIRS = {".git", "thirdparty", "dx9sdk", "external", "lib", "build"}
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            if os.path.splitext(fn)[1] in SOURCE_EXT:
                yield os.path.join(dirpath, fn)


def count_guarded_lines(path):
    """Count lines inside #if(def) blocks gated on a console token.

    Fast path: most files contain no console token at all, so read once and
    bail out before doing any line-by-line work.
    """
    try:
        with open(path, errors="ignore") as f:
            text = f.read()
    except OSError:
        return 0

    if not any(t in text for t in CONSOLE_TOKENS):
        return 0

    total, depth, guard_depth = 0, 0, None
    for line in text.splitlines():
        s = line.lstrip()
        if s.startswith("#"):
            if re.match(r"#\s*(if|ifdef|ifndef)\b", s):
                depth += 1
                if guard_depth is None and any(t in s for t in CONSOLE_TOKENS):
                    guard_depth = depth
                continue
            if re.match(r"#\s*endif\b", s):
                if guard_depth is not None and depth == guard_depth:
                    guard_depth = None
                depth -= 1
                continue
        if guard_depth is not None:
            total += 1
    return total


def wscript_referenced_dirs():
    """Directories any wscript actually feeds to the compiler."""
    referenced = set()
    for dirpath, dirnames, filenames in os.walk(ROOT):
        if ".git" in dirpath:
            continue
        if "wscript" not in filenames:
            continue
        try:
            text = open(os.path.join(dirpath, "wscript"), errors="ignore").read()
        except OSError:
            continue
        # Strip comments so commented-out [$X360] entries do not count.
        text = re.sub(r"#.*$", "", text, flags=re.M)
        for m in re.finditer(r"['\"]([^'\"]+\.(?:cpp|c|mm))['\"]", text):
            referenced.add(os.path.normpath(os.path.join(dirpath, os.path.dirname(m.group(1)))))
    return referenced


def main():
    delete = "--delete-unbuilt" in sys.argv

    print("=" * 66)
    print("Legacy console code report")
    print("=" * 66)

    # 1. Confirm the build never defines the console platforms.
    print("\n[1] Build configuration")
    defined = []
    for name in ("wscript",):
        p = os.path.join(ROOT, name)
        if os.path.exists(p):
            t = open(p, errors="ignore").read()
            for tok in CONSOLE_TOKENS:
                if re.search(r"define\(\s*['\"]" + tok, t):
                    defined.append(tok)
    if defined:
        print("    WARNING: build defines " + ", ".join(defined))
    else:
        print("    None of _X360 / _PS3 / _GAMECONSOLE are defined by the build.")
        print("    => these blocks are already removed by the preprocessor.")
        print("    => compile time and binary size are ALREADY unaffected.")

    # 2. How much guarded source a reader has to skip.
    print("\n[2] Dead code inside compiled sources")
    per_file, total_lines = [], 0
    for path in iter_sources():
        n = count_guarded_lines(path)
        if n:
            per_file.append((n, os.path.relpath(path, ROOT)))
            total_lines += n
    per_file.sort(reverse=True)
    print(f"    {total_lines} lines across {len(per_file)} files")
    for n, rel in per_file[:10]:
        print(f"      {n:6d}  {rel}")

    # 3. Directories that are not built at all.
    print("\n[3] Unbuilt console directories")
    referenced = wscript_referenced_dirs()
    unbuilt, unbuilt_bytes = [], 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        if ".git" in dirpath:
            continue
        if os.path.basename(dirpath).lower() != "xbox":
            continue
        if os.path.normpath(dirpath) in referenced:
            continue
        size = 0
        for dp, _, fns in os.walk(dirpath):
            for fn in fns:
                try:
                    size += os.path.getsize(os.path.join(dp, fn))
                except OSError:
                    pass
        unbuilt.append((dirpath, size))
        unbuilt_bytes += size

    print(f"    {len(unbuilt)} directories, {unbuilt_bytes / 1048576:.1f} MB, referenced by no wscript")
    for d, size in sorted(unbuilt, key=lambda x: -x[1])[:10]:
        print(f"      {size/1024:8.0f} KB  {os.path.relpath(d, ROOT)}")

    if delete:
        print("\n[4] Deleting unbuilt directories")
        for d, _ in unbuilt:
            subprocess.run(["rm", "-rf", d], check=False)
            print(f"      removed {os.path.relpath(d, ROOT)}")
        print(f"    reclaimed {unbuilt_bytes / 1048576:.1f} MB")
    else:
        print("\n    (pass --delete-unbuilt to remove the directories in [3])")

    print("\nSummary: the build is already unaffected by this code. The win is")
    print("readability, not performance -- do not expect a faster build.")


if __name__ == "__main__":
    main()
