#!/usr/bin/env python3
"""Executable status source for check_prod_manifest freshness controls."""

import sys


STATUS_TEXT = {
    1: "fresh output would be incomplete: SKIPPED zhao_control_top no port list",
    2: "gen_prod_top: refusing to write unresolved control type",
    3: "STALE: generated control output does not match",
}


def main():
    status = int(sys.argv[1])
    print(STATUS_TEXT.get(status, "unexpected generator control status"))
    return status


if __name__ == "__main__":
    sys.exit(main())
