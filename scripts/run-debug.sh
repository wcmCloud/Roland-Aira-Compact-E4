#!/bin/sh
set -eu
mode="${1:-run}"
case "$mode" in
    --help) printf '%s\n' 'Usage: scripts/run-debug.sh [--build-only|--debug|--help]' 'Builds Debug standalone. Default opens the app; --debug starts it under LLDB.' 'Set E4_JUCE_DIR to a local JUCE checkout if needed.'; exit 0 ;;
    run|--build-only|--debug) ;;
    *) printf '%s\n' 'Unknown option. Use --help.' >&2; exit 2 ;;
esac
project_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
juce_dir="${E4_JUCE_DIR:-}"
if [ -z "$juce_dir" ]; then
    for candidate in "$project_dir/build/_deps/juce-src" "$project_dir/build-check/_deps/juce-src"; do
        if [ -f "$candidate/CMakeLists.txt" ]; then juce_dir="$candidate"; break; fi
    done
fi
if [ -z "$juce_dir" ] || [ ! -f "$juce_dir/CMakeLists.txt" ]; then
    printf '%s\n' 'JUCE not found. Set E4_JUCE_DIR to a JUCE checkout, or configure once with -DE4_FETCH_JUCE=ON.' >&2
    exit 1
fi
cmake -S "$project_dir" -B "$project_dir/build-check" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug -DJUCE_SOURCE_DIR="$juce_dir"
cmake --build "$project_dir/build-check" --target AiraE4_Standalone --parallel 4
app_path="$project_dir/build-check/AiraE4_artefacts/Debug/Standalone/E-4 Companion.app"
case "$mode" in
    --build-only) printf 'Built: %s\n' "$app_path" ;;
    --debug) exec lldb -o run "$app_path/Contents/MacOS/E-4 Companion" ;;
    run) open "$app_path" ;;
esac
