#!/usr/bin/env python3
"""Hide *.context.md companion files (Strict C++ Comment Policy).

Companion files hold the why-comments moved out of src/ and must stay
invisible: gitignored (see .gitignore) + Windows Hidden attribute (which
git does not track, so re-run this after creating new ones).

Usage: python tools/hide_context_files.py [--dry-run]
"""

import os
import sys
from pathlib import Path

HIDDEN_FLAG = 0x2  # FILE_ATTRIBUTE_HIDDEN


def hide(path: Path, dry_run: bool) -> bool:
    if os.name != "nt":
        print(f"[-] skipping (not Windows): {path}")
        return False
    import ctypes

    attrs = ctypes.windll.kernel32.GetFileAttributesW(str(path))
    if attrs == -1:
        print(f"[-] cannot read attributes: {path}")
        return False
    if attrs & HIDDEN_FLAG:
        return True
    if dry_run:
        print(f"[dry-run] would hide: {path}")
        return True
    if not ctypes.windll.kernel32.SetFileAttributesW(str(path), attrs | HIDDEN_FLAG):
        print(f"[-] cannot hide: {path}")
        return False
    print(f"[+] hidden: {path}")
    return True


def main() -> int:
    dry_run = "--dry-run" in sys.argv[1:]
    root = Path(__file__).parent.parent.resolve()
    targets = sorted(root.rglob("*.context.md"))
    if not targets:
        print("No *.context.md files found (nothing to do).")
        return 0
    ok = True
    for path in targets:
        if path.is_file():
            ok = hide(path, dry_run) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
