#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir"

g++ -std=c++17 -O2 -Wall -Wextra -I. src/*.cpp -o build.exe