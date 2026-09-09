#!/usr/bin/env python3
"""Find stray CONTROL BYTES in text files -- the shell-escape casualties.

WHY THIS EXISTS: THREE TIMES, SAME MECHANISM
--------------------------------------------
A backslash escape written inside a shell heredoc gets interpreted before the
file is written, so `\\b` lands on disk as a single 0x08 BACKSPACE byte. The text
still LOOKS almost right in an editor -- `tools\\budget` renders as `toolsudget`
-- and the code around it still parses, so nothing complains.

CLAUDE.md records the first instance: a self-check "written with a word-boundary
escape that a shell heredoc turned into a literal backspace character -- so it
matched nothing and printed reassurance for its whole life".

2026-09-09 found the second, in `tools/quartus/run_calib.ps1`:

    $destPath = Join-Path $RepoRoot 'tools<0x08>udget\\calibration.json'

`-SkipMeasured` therefore looked for a file that cannot exist, found nothing to
skip, and silently re-measured all 123 calibration points -- about two hours of
the scarcest resource in this project, every time the flag was used. The flag's
own comment says "this lane lost 97 measurements once and will not do it twice".
The protection had never once worked.

And the third instance happened while repairing the second: the python script
written to strip the backspace had its own `\\b` collapsed the same way, so the
replacement string contained a backspace too. Its assertion caught it. That is
the whole argument for this file -- the mechanism catches you again while you are
looking directly at it.

WHAT IT FLAGS, and what it does not:

  0x00 NUL, 0x07 BEL (\\a), 0x08 BS (\\b), 0x0B VT (\\v), 0x1B ESC (\\e)

Those are exactly the bytes a mangled escape produces. TAB, LF and CR are normal.
0x0C form feed is left alone -- it is a legitimate page break in some older
sources and flagging it would train people to ignore the output.

Usage:
    python tools/maintenance/no_control_bytes.py            # scan the usual trees
    python tools/maintenance/no_control_bytes.py path ...
    python tools/maintenance/no_control_bytes.py --self-test
"""
import io
import os
import sys

# The bytes a collapsed backslash-escape leaves behind.
BAD = {0x00: "NUL", 0x07: r"BEL (\a)", 0x08: r"BACKSPACE (\b)",
       0x0B: r"VTAB (\v)", 0x1B: r"ESC (\e)"}

TEXT_EXT = (".py", ".ps1", ".sh", ".sv", ".svh", ".v", ".vh", ".cpp", ".hpp",
            ".h", ".c", ".md", ".yml", ".yaml", ".json", ".txt", ".tcl",
            ".qsf", ".sdc", ".cmake", ".toml", ".ts", ".js")

DEFAULT_ROOTS = ("tools", "design", "fpga/rtl", "fpga/quartus", "tests")

SKIP_DIRS = {".git", "build", "node_modules", "__pycache__", "build-budget",
             "renders", "captures", "reports"}


def scan_file(path):
    try:
        data = io.open(path, "rb").read()
    except OSError:
        return []
    hits = []
    for off, byte in enumerate(data):
        if byte in BAD:
            line = data.count(b"\n", 0, off) + 1
            start = max(0, off - 26)
            ctx = data[start:off + 26].decode("utf-8", errors="replace")
            hits.append((line, BAD[byte], ctx.replace("\n", " ")))
    return hits


def walk(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for d, dirs, names in os.walk(root):
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
            for n in names:
                if n.endswith(TEXT_EXT):
                    yield os.path.join(d, n)


def self_test():
    """A scanner that finds nothing reports a clean tree, so prove it can see.

    Both polarities: a file containing a backspace must be flagged, and one
    containing only tab/LF/CR must not.
    """
    import tempfile
    fd, p = tempfile.mkstemp(suffix=".ps1")
    os.close(fd)
    io.open(p, "wb").write(b"$x = 'tools" + bytes([8]) + b"udget'\n")
    try:
        hits = scan_file(p)
        assert len(hits) == 1, "backspace not found: %s" % hits
        assert "BACKSPACE" in hits[0][1], "misclassified: %s" % hits[0][1]
        io.open(p, "wb").write(b"ok\tfine\r\nnormal\n")
        assert scan_file(p) == [], "tab/CR/LF must not be flagged"
    finally:
        os.unlink(p)
    return True


def main():
    self_test()
    args = [a for a in sys.argv[1:] if a != "--self-test"]
    if "--self-test" in sys.argv[1:]:
        print("no_control_bytes self-test: finds a planted backspace, and does "
              "not flag tab/CR/LF.")
        return 0

    roots = args or [r for r in DEFAULT_ROOTS if os.path.exists(r)]
    total = 0
    files = 0
    for path in walk(roots):
        files += 1
        for line, what, ctx in scan_file(path):
            total += 1
            print("%s:%d: %s" % (path.replace(os.sep, "/"), line, what))
            print("    %s" % ctx)
    print()
    if total:
        print("%d stray control byte(s) in %d scanned file(s). Each is almost "
              "certainly a backslash escape that a shell ate before the file was "
              "written -- the text will look nearly right and the code will still "
              "parse." % (total, files))
        return 1
    print("no stray control bytes in %d scanned file(s)." % files)
    return 0


if __name__ == "__main__":
    sys.exit(main())
