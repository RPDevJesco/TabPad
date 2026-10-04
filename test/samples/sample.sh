#!/bin/bash
# backs a folder up
set -e

dest="$HOME/backup"

backup() {
    local src=$1
    if [ -d "$src" ]; then
        tar czf "$dest/$(basename "$src").tgz" "$src"
    fi
}

for d in "$@"; do
    backup "$d" || echo "failed: $d" >&2
done
