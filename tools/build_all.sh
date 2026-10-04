#!/bin/sh

set -e

make T=native test app smoke SDL_STATIC=--static
make T=win64 all app
make web web-smoke

echo
echo "the editor for Windows:  build/win64/tabpad.exe   (keep SDL3.dll beside it)"
echo "the editor for Linux:    build/native/tabpad"
echo "the editor for a browser: build/web/tabpad.html   (one file; open it from disk)"
echo

set +e
wine cmd /c exit 0
own=$?
wine build/win64/hlcat.exe > /dev/null 2>&1
made=$?
set -e
echo "wine: its own program ended with $own, one built here with $made"

if [ "$own" != 0 ] || [ "$made" != 2 ]
then
    echo "wine cannot run programs in this container: the Windows tests were NOT run"
    exit 0
fi

make T=win64 test smoke
