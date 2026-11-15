#!/usr/bin/env python3
"""Comment census and strict-policy check.

Line-based: `//` inside a string literal is counted as a comment; `/*...*/`
comments are not counted at all.
"""

import argparse
import re
from pathlib import Path

AREAS = [
    "src/game/public",
    "src/game/private",
    "src/thirdparty",
    "tests/unit-tests",
    "tests/integration-tests",
    "tests/e2e",
]
EXTS = {".h", ".hpp", ".cpp", ".cc"}
BANNED_HISTORY = re.compile(
    r"\b(plans/|used to|no longer|formerly|previously|replaced by|before the fix)\b",
    re.IGNORECASE,
)
BANNER = re.compile(r"^([-=#])\1{2,}$")
BANNER_PREFIX = re.compile(r"^--- ")
TODO = re.compile(r"\b(TODO|FIXME|HACK|XXX)\b")


def iter_files(root: Path, area: str):
    base = root / area
    if not base.is_dir():
        return
    for path in sorted(base.rglob("*")):
        if path.suffix in EXTS and path.is_file():
            yield path.relative_to(root).as_posix(), path


def census(lines):
    full = trail = 0
    for line in lines:
        idx = line.find("//")
        if idx == -1:
            continue
        if line[:idx].strip() == "":
            full += 1
        else:
            trail += 1
    return full, trail


def banned_lines(rel, lines):
    out = []
    for i, line in enumerate(lines, 1):
        idx = line.find("//")
        if idx == -1:
            continue
        text = line.strip()
        body = text[2:].strip()
        if BANNED_HISTORY.search(body) or BANNER.match(body) or BANNER_PREFIX.match(text) \
                or TODO.search(body):
            out.append(f"{rel}:{i}: {text}")
    return out


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report banned comment lines")
    parser.add_argument("--files", action="store_true",
                        help="per-file census sorted by full-line comments")
    args = parser.parse_args()

    stats = {}
    per_file = {}
    violations = []
    for area in AREAS:
        for rel, path in iter_files(root, area):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            full, trail = census(lines)
            s = stats.setdefault(area, [0, 0, 0, 0])
            s[0] += 1
            s[1] += len(lines)
            s[2] += full
            s[3] += trail
            per_file[rel] = (len(lines), full, trail)
            if args.check and not rel.startswith("src/thirdparty"):
                violations.extend(banned_lines(rel, lines))

    if args.files:
        print(f"{'files':<52} {'lines':>6} {'full':>5} {'trail':>6}")
        for rel, (lines, full, trail) in sorted(per_file.items(), key=lambda kv: -kv[1][1]):
            print(f"{rel:<52} {lines:>6} {full:>5} {trail:>6}")
    elif not args.check:
        print(f"{'area':<28} {'files':>6} {'lines':>7} {'full':>6} {'trail':>7} {'share':>7}")
        for area in AREAS:
            if area not in stats:
                continue
            files, lines, full, trail = stats[area]
            total = full + trail
            share = 100.0 * total / lines if lines else 0.0
            print(f"{area:<28} {files:>6} {lines:>7} {full:>6} {trail:>7} {share:>6.1f}%")
        tf, tl, tc, tr = (sum(s[i] for s in stats.values()) for i in range(4))
        share = 100.0 * (tc + tr) / tl if tl else 0.0
        print(f"{'TOTAL':<28} {tf:>6} {tl:>7} {tc:>6} {tr:>7} {share:>6.1f}%")

    if args.check:
        if violations:
            print()
            for v in sorted(violations):
                print(v)
            return 1
        print("\nno banned comment lines")
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
