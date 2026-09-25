#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
jobs="${JOBS:-2}"
command -v qmake >/dev/null
command -v make >/dev/null
qmake -v
for target in app cli tests identity normal continuous recovery; do
    case "$target" in
        app) project_file="$project_dir/app/can.pro"; build_dir="$project_dir/build-app" ;;
        cli) project_file="$project_dir/cli/cli.pro"; build_dir="$project_dir/build-cli" ;;
        tests) project_file="$project_dir/tests/tests.pro"; build_dir="$project_dir/build-tests" ;;
        identity) project_file="$project_dir/tests/identity.pro"; build_dir="$project_dir/build-identity-tests" ;;
        normal) project_file="$project_dir/tests/normal.pro"; build_dir="$project_dir/build-normal-tests" ;;
        continuous) project_file="$project_dir/tests/continuous.pro"; build_dir="$project_dir/build-continuous-tests" ;;
        recovery) project_file="$project_dir/tests/recovery.pro"; build_dir="$project_dir/build-recovery-tests" ;;
    esac
    if [[ ! -f "$project_file" ]]; then
        printf 'Missing project: %s\n' "$project_file" >&2
        exit 2
    fi
    mkdir -p "$build_dir"
    (cd "$build_dir" && qmake "$project_file" && make -j"$jobs")
done
make -C "$project_dir/firmware" test
"$project_dir/build-tests/canbench-tests"
"$project_dir/build-identity-tests/canbench-identity-tests"
"$project_dir/build-normal-tests/canbench-normal-tests"
"$project_dir/build-continuous-tests/canbench-continuous-tests"
"$project_dir/build-recovery-tests/canbench-recovery-tests"
printf '\nBuild and native tests completed. SocketCAN acceptance is a separate step.\n'
