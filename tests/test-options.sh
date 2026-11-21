#!/bin/sh
set -eu

for option in -c --fgcolor --bgcolor; do
    if output=$(./wmsystemtray "$option" 2>&1); then
        echo "$option unexpectedly succeeded without an argument" >&2
        exit 1
    fi
    case "$output" in
        *"$option requires an argument"*) ;;
        *) echo "Unexpected error for $option: $output" >&2; exit 1 ;;
    esac
done

./wmsystemtray --fgcolor black --bgcolor white --version >/dev/null
