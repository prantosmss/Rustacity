#!/usr/bin/env python3
"""Check that the Rust DSP core and its C header declare the same functions.

Why this script exists
----------------------
``rust/dsp/include/rustacity_dsp.h`` is written by hand: it mirrors the
``#[no_mangle] pub extern "C"`` functions in ``rust/dsp/src/ffi.rs``.  Nothing
in the build notices when the two drift apart - a function exported by Rust but
missing from the header, or declared in the header but no longer exported,
still compiles and links, and only shows up later as a link error or a crash in
a platform build that cannot be reproduced locally.

The check is textual: it compares the two name sets and fails when they differ,
so the header is updated in the same commit as the change that made them
disagree.  Run it from the repository root:

    python3 scripts/check_dsp_abi.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FFI = ROOT / "rust" / "dsp" / "src" / "ffi.rs"
HEADER = ROOT / "rust" / "dsp" / "include" / "rustacity_dsp.h"

BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT = re.compile(r"//[^\n]*")

# `pub extern "C" fn name(` and `pub unsafe extern "C" fn name(`.
EXPORT = re.compile(r'pub\s+(?:unsafe\s+)?extern\s+"C"\s+fn\s+([A-Za-z_][A-Za-z0-9_]*)')

# A declaration: the line starts with a return type (void/int/float/double or a
# rustacity_ type), optionally a pointer star, then the function name and "(".
DECLARATION = re.compile(
    r"^\s*(?:void|int|float|double|rustacity_[A-Za-z0-9_]+)\s*\*?\s*"
    r"(rustacity_[A-Za-z0-9_]+)\s*\(",
    re.MULTILINE,
)


def strip_comments(text: str) -> str:
    """Remove C and C++ comments so prose is never mistaken for code."""
    return LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text))


def exported_functions(text: str) -> dict[str, bool]:
    """Map each exported function name to whether it carries #[no_mangle]."""
    source = strip_comments(text)
    found: dict[str, bool] = {}
    for match in EXPORT.finditer(source):
        name = match.group(1)
        # Without #[no_mangle] the symbol is mangled and C cannot call it.
        found[name] = source[: match.start()].rstrip().endswith("#[no_mangle]")
    return found


def declared_functions(text: str) -> set[str]:
    """The function names the C header declares."""
    return set(DECLARATION.findall(strip_comments(text)))


def main() -> int:
    functions = exported_functions(FFI.read_text(encoding="utf-8"))
    exported = set(functions)
    mangled = sorted(name for name, unmangled in functions.items() if not unmangled)
    declared = declared_functions(HEADER.read_text(encoding="utf-8"))

    header = HEADER.relative_to(ROOT).as_posix()
    ffi = FFI.relative_to(ROOT).as_posix()

    problems = []
    missing = sorted(exported - declared)
    if missing:
        problems.append(
            f"exported by {ffi} but not declared in {header}:\n"
            + "".join(f"   {name}\n" for name in missing)
        )
    extra = sorted(declared - exported)
    if extra:
        problems.append(
            f"declared in {header} but not exported by {ffi}:\n"
            + "".join(f"   {name}\n" for name in extra)
        )
    if mangled:
        problems.append(
            "exported without #[no_mangle], so C cannot link them:\n"
            + "".join(f"   {name}\n" for name in mangled)
        )

    print(f"{ffi}: {len(exported)} exports")
    print(f"{header}: {len(declared)} declarations")
    for name in sorted(exported & declared):
        print(f"   {name}")

    if problems:
        for problem in problems:
            print(f"\nERROR: {problem}", file=sys.stderr)
        return 1

    print("\nThe DSP ABI and its header declare the same functions.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
