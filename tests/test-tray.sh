#!/bin/sh
set -eu

for tool in xvfb-run Xvfb xauth; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool is required for the tray integration test"
        exit 77
    fi
done

run_test() {
    xvfb-run -a sh -c '
        ./wmsystemtray "$@" &
        tray=$!
        trap "kill \"$tray\" 2>/dev/null || :; wait \"$tray\" 2>/dev/null || :" EXIT HUP INT TERM
        ./test-tray
        kill "$tray"
        wait "$tray"
        trap - EXIT HUP INT TERM
    ' sh "$@"
}

run_test --non-wmaker -w 2
run_test -w 2 --small
