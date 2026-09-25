#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$project_dir/build-gui-tests"
cd "$project_dir/build-gui-tests"
qmake "$project_dir/tests/gui.pro"
make -j"${JOBS:-2}"
QT_QPA_PLATFORM=offscreen ./canbench-gui-tests
