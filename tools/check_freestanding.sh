#!/bin/sh
# the freestanding rule, checked: across the given objects and archives,
# every undefined symbol must be the module's port contract (<prefix>_*)
# or one of the four mem* functions the compiler itself calls. Anything
# else means the core has started to lean on a libc.
#
#   check_freestanding.sh <nm> <prefix> <file>...

nm=$1
prefix=$2
shift 2

for f in "$@"
do
    if [ ! -f "$f" ]
    then
        echo "FAIL freestanding $prefix: $f is not there to check"
        exit 1
    fi
done

defined=$("$nm" -g --defined-only "$@" 2>/dev/null | awk 'NF >= 3 { print $3 }' | sort -u)
wanted=$("$nm" -u "$@" 2>/dev/null | awk '$1 == "U" { print $2 }' | sort -u)
leaks=$(printf '%s\n' "$wanted" | grep -v -x -F "$defined" | grep -v -E "^_?(${prefix}_[a-z0-9_]+|memcpy|memmove|memset|memcmp|_GLOBAL_OFFSET_TABLE_|__stack_pointer)\$")

if [ -n "$leaks" ]
then
    echo "FAIL freestanding $prefix: undefined symbols outside the port contract:"
    echo "$leaks"
    exit 1
fi

echo "freestanding $prefix: ok"
