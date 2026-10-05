#!/usr/bin/env python3
"""Runs the ether-lsp integration suites.

    python lsp/test/run.py              # everything
    python lsp/test/run.py features     # one suite

Both the compiler and the language server must be built first:

    build.bat        (or cmake --build build)
    lsp\\build.cmd    (or ./lsp/build.sh)
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import client  # noqa: E402
import test_features  # noqa: E402
import test_resilience  # noqa: E402
import test_scheduling  # noqa: E402

SUITES = {
    "features": test_features.run,
    "scheduling": test_scheduling.run,
    "resilience": test_resilience.run,
}


def main(argv):
    complaint = client.preflight()
    if complaint:
        print(f"cannot run: {complaint}")
        return 1

    wanted = argv or list(SUITES)
    unknown = [name for name in wanted if name not in SUITES]
    if unknown:
        print(f"unknown suite(s): {', '.join(unknown)}")
        print(f"available: {', '.join(SUITES)}")
        return 1

    started = time.monotonic()
    failures = 0
    for name in wanted:
        failures += SUITES[name]()

    elapsed = time.monotonic() - started
    print()
    if failures:
        print(f"{failures} failure(s) in {elapsed:.1f}s")
    else:
        print(f"all suites passed in {elapsed:.1f}s")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
