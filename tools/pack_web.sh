#!/bin/sh

set -e

wasm=$1
page=$2
css=$3
js=$4
out=$5
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

{
    printf "const WASM = '"
    gzip -9 -n -c "$wasm" | base64 | tr -d '\n'
    printf "';\n"
    cat "$js"
} > "$tmp/script"

script_hash=$(openssl dgst -sha256 -binary "$tmp/script" | base64)
style_hash=$(openssl dgst -sha256 -binary "$css" | base64)
sed -e "s|@SCRIPT_HASH@|$script_hash|" -e "s|@STYLE_HASH@|$style_hash|" "$page" > "$tmp/page"
style_at=$(grep -n '^@STYLE@$' "$tmp/page" | cut -d: -f1)
script_at=$(grep -n '^@SCRIPT@$' "$tmp/page" | cut -d: -f1)

{
    head -n $((style_at - 1)) "$tmp/page"
    printf '<style>'
    cat "$css"
    printf '</style>\n'
    sed -n "$((style_at + 1)),$((script_at - 1))p" "$tmp/page"
    printf '<script>'
    cat "$tmp/script"
    printf '</script>\n'
    tail -n +$((script_at + 1)) "$tmp/page"
} > "$out"

echo "the page: $out ($(wc -c < "$out") bytes), one file, nothing fetched"
