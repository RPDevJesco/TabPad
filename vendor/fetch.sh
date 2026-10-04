#!/bin/sh

set -e

here=$(cd "$(dirname "$0")" && pwd)
dest=${1:-$here}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

grep -v '^#' "$here/VERSIONS" | while read -r name repo commit src queries note
do
    if [ "$name" = "dejavu" ]
    then
        curl -sSL -o "$tmp/dejavu.tar.bz2" "$repo"
        echo "$commit  $tmp/dejavu.tar.bz2" | sha256sum -c - > /dev/null
        mkdir -p "$tmp/dejavu"
        tar xjf "$tmp/dejavu.tar.bz2" -C "$tmp/dejavu" --strip-components 1
        continue
    fi

    git init -q "$tmp/$name"
    git -C "$tmp/$name" fetch -q --depth 1 "$repo" "$commit"
    git -C "$tmp/$name" checkout -q FETCH_HEAD

    if [ "$src" = "-" ]
    then
        continue
    fi

    rm -rf "$dest/$name"
    mkdir -p "$dest/$name/src" "$dest/$name/queries"
    cp "$tmp/$name/$src/parser.c" "$dest/$name/src/"
    cp -r "$tmp/$name/$src/tree_sitter" "$dest/$name/src/"
    cp "$tmp/$name/$queries"/highlights*.scm "$dest/$name/queries/"
    cp "$tmp/$name"/LICENSE* "$dest/$name/"

    for extra in "$tmp/$name/$src/scanner.c" "$tmp/$name/$src"/*.h
    do
        if [ -f "$extra" ]
        then
            cp "$extra" "$dest/$name/src/"
        fi
    done
done

rm -rf "$dest/tree-sitter" "$dest/stb" "$dest/common" "$dest/dejavu"
mkdir -p "$dest/tree-sitter/lib" "$dest/stb" "$dest/common" "$dest/dejavu"

cp -r "$tmp/tree-sitter/lib/src" "$tmp/tree-sitter/lib/include" "$dest/tree-sitter/lib/"
cp "$tmp/tree-sitter/LICENSE" "$dest/tree-sitter/"
rm -rf "$dest/tree-sitter/lib/src/wasm-stdlib"
cp "$tmp/stb/stb_truetype.h" "$tmp/stb/LICENSE" "$dest/stb/"
cp "$tmp/dejavu/ttf/DejaVuSansMono.ttf" "$tmp/dejavu/LICENSE" "$dest/dejavu/"
cp "$tmp/tree-sitter-xml/common/scanner.h" "$dest/common/"
cp "$tmp/tree-sitter-apex/sosl/queries/highlights.scm" "$dest/tree-sitter-apex/queries/highlights-sosl.scm"
cp "$tmp/tree-sitter-apex/soql/queries/highlights.scm" "$dest/tree-sitter-apex/queries/highlights-soql.scm"

echo "vendored into $dest"
