#!/bin/bash
# Lists the DLLs that the given Windows executables need, directly or through
# other DLLs, and that cannot be found on PATH. Windows only reports a missing
# DLL as exit code 0xc0000135, without naming it.
# Needs dumpbin, so run it in a Visual Studio environment.
#
# Usage: list-missing-dlls.sh <executable>... [-- <directory to search>...]
# A missing DLL is looked for in the directories after --, to show where it
# was installed instead.

declare -A SEEN
MISSING=0

check()
{
    local DEPENDENCY
    for DEPENDENCY in $(dumpbin -nologo -dependents "$1" | tr -d '\r' | grep -i '^ *[^ ]*\.dll$')
    do
        local KEY=${DEPENDENCY,,}
        # API sets are resolved by the loader and do not exist as files.
        if [[ -n ${SEEN[$KEY]} || $KEY == api-ms-* || $KEY == ext-ms-* ]]
        then
            continue
        fi
        SEEN[$KEY]=1
        local FOUND
        FOUND=$(type -P "$DEPENDENCY")
        if [[ -z $FOUND ]]
        then
            echo "MISSING: $DEPENDENCY (needed by $(basename "$1"))"
            if [[ ${#SEARCH_DIRECTORIES[@]} -gt 0 ]]
            then
                find "${SEARCH_DIRECTORIES[@]}" -iname "$DEPENDENCY" -printf '  found at %p\n'
            fi
            MISSING=1
        elif [[ $FOUND != /c/[Ww]indows/* ]]
        then
            check "$FOUND"
        fi
    done
}

EXECUTABLES=()
SEARCH_DIRECTORIES=()
while [[ $# -gt 0 && $1 != -- ]]
do
    EXECUTABLES+=("$1")
    shift
done
if [[ $1 == -- ]]
then
    shift
    SEARCH_DIRECTORIES=("$@")
fi

for EXECUTABLE in "${EXECUTABLES[@]}"
do
    check "$EXECUTABLE"
done
if [[ $MISSING -eq 0 ]]
then
    echo "All DLL dependencies were found."
fi
exit $MISSING
