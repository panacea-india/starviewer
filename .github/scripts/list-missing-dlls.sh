#!/bin/bash
# Lists the DLLs that the given Windows executables need, directly or through
# other DLLs, and that cannot be found on PATH. Windows only reports a missing
# DLL as exit code 0xc0000135, without naming it.
# Needs dumpbin, so run it in a Visual Studio environment.

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
            MISSING=1
        elif [[ $FOUND != /c/[Ww]indows/* ]]
        then
            check "$FOUND"
        fi
    done
}

for EXECUTABLE in "$@"
do
    check "$EXECUTABLE"
done
if [[ $MISSING -eq 0 ]]
then
    echo "All DLL dependencies were found."
fi
exit $MISSING
