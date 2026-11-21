#!/bin/sh
set -eu

if ! command -v valgrind >/dev/null 2>&1; then
    echo "Valgrind is required for the icon leak test"
    exit 77
fi

valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1 ./test-icons
