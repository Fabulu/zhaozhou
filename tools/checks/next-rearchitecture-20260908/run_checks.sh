#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror independent_checks.cpp -o "$work/checks"
"$work/checks"
