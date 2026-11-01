#!/bin/bash
# Builds the patched SDK one library at a time, so that CI can cache each
# library separately and a change to one library's scripts or patches only
# rebuilds that library and the libraries built against it.
#
# Usage:
#   sdk-library.sh keys <cache key prefix> <library>...
#       Writes the cache key of each library to $GITHUB_OUTPUT as
#       <library>=<key>.
#   sdk-library.sh install <library>
#       If the library's files were restored from the cache into
#       $SDK_BASE_PREFIX/sdk-stage/<library>, copies them into sdk-install.
#       Otherwise builds the library into sdk-install and copies the files it
#       installed into sdk-stage/<library>, for the cache to save.
#
# Libraries must be installed in dependency order, the order of DEPENDENCIES.

# Increase to discard every cached library, e.g. after changing how
# libraries are staged.
CACHE_VERSION=1

SDK_SCRIPTS=sdk/scripts/unix

# The libraries each library is built against, directly or indirectly.
# Changing any of them rebuilds the library.
declare -A DEPENDENCIES=(
    [dcmtk]=""
    [vtk]=""
    [gdcm]="vtk"
    [itk]="dcmtk vtk gdcm"
    [threadweaver]=""
)

# The SDK script libraries each library is built with. ECM is only needed to
# build ThreadWeaver and installs outside sdk-install.
declare -A SDK_LIBS=(
    [threadweaver]="ecm threadweaver"
)

# Scripts that every library build goes through.
COMMON_FILES=(
    "$SDK_SCRIPTS/config/setup.sh"
    "$SDK_SCRIPTS/config/vcvarsall.sh"
    "$SDK_SCRIPTS/libs/cmakebuild.sh"
    "$SDK_SCRIPTS/sdk_build.sh"
    "$SDK_SCRIPTS/sdk_download.sh"
    "$SDK_SCRIPTS/sdk_extract.sh"
)

check_library()
{
    if [[ -z ${DEPENDENCIES[$1]+set} ]]
    then
        echo "ERROR: Unknown SDK library $1"
        exit 1
    fi
}

# Prints the files that determine how a library is built.
library_files()
{
    local LIBRARY SDK_LIB
    printf '%s\n' "${COMMON_FILES[@]}"
    for LIBRARY in ${DEPENDENCIES[$1]} $1
    do
        for SDK_LIB in ${SDK_LIBS[$LIBRARY]:-$LIBRARY}
        do
            echo "$SDK_SCRIPTS/config/$SDK_LIB.sh"
            echo "$SDK_SCRIPTS/libs/$SDK_LIB.sh"
            if [[ -d sdk/patches/$SDK_LIB ]]
            then
                find "sdk/patches/$SDK_LIB" -type f
            fi
        done
    done
}

keys()
{
    local PREFIX=$1 LIBRARY FILE HASH
    shift
    for LIBRARY in "$@"
    do
        check_library "$LIBRARY"
        HASH=$(
            {
                echo "$CACHE_VERSION"
                library_files "$LIBRARY" | LC_ALL=C sort | while read -r FILE
                do
                    echo "$FILE"
                    cat "$FILE"
                done
            } | sha256sum | cut -d ' ' -f 1
        )
        echo "$LIBRARY=$PREFIX-$LIBRARY-$HASH" | tee -a "$GITHUB_OUTPUT"
    done
}

list_installed_files()
{
    (cd "$SDK_INSTALL" && find . \( -type f -o -type l \) | LC_ALL=C sort)
}

install_library()
{
    local LIBRARY=$1
    check_library "$LIBRARY"

    # MSYS2 tools such as tar read "D:/..." as host:path.
    if [[ $(uname) == 'MSYS_NT'* ]]
    then
        SDK_BASE_PREFIX=$(cygpath -u "$SDK_BASE_PREFIX")
    fi
    export SDK_BASE_PREFIX
    SDK_INSTALL="$SDK_BASE_PREFIX/sdk-install"
    local STAGE="$SDK_BASE_PREFIX/sdk-stage/$LIBRARY"
    mkdir -p "$SDK_INSTALL"

    if [[ -d $STAGE ]]
    then
        echo "Installing $LIBRARY from the cache."
        cp -a "$STAGE/." "$SDK_INSTALL"
        exit $?
    fi

    echo "Building $LIBRARY."
    local BEFORE AFTER
    BEFORE=$(mktemp)
    AFTER=$(mktemp)
    list_installed_files > "$BEFORE"

    export LIBS="${SDK_LIBS[$LIBRARY]:-$LIBRARY}"
    (
        cd "$SDK_SCRIPTS" &&
        ./sdk_download.sh &&
        ./sdk_extract.sh > /dev/null &&
        ./sdk_build.sh
    )
    if [[ $? -ne 0 ]]
    then
        echo "ERROR: Building $LIBRARY failed"
        exit 1
    fi

    # Files a library overwrites belong to a library it depends on, so only
    # new files are staged.
    list_installed_files > "$AFTER"
    local NEW_FILES
    NEW_FILES=$(LC_ALL=C comm -13 "$BEFORE" "$AFTER")
    if [[ -z $NEW_FILES ]]
    then
        echo "ERROR: $LIBRARY installed no files"
        exit 1
    fi
    mkdir -p "$STAGE"
    echo "$NEW_FILES" | tar -C "$SDK_INSTALL" -cf - -T - | tar -C "$STAGE" -xf -
    local STATUS=("${PIPESTATUS[@]}")
    if [[ ${STATUS[1]} -ne 0 || ${STATUS[2]} -ne 0 ]]
    then
        echo "ERROR: Staging $LIBRARY for the cache failed"
        exit 1
    fi
    echo "Staged $(echo "$NEW_FILES" | wc -l) files of $LIBRARY for the cache."
}

COMMAND=$1
shift
case $COMMAND in
    keys) keys "$@" ;;
    install) install_library "$@" ;;
    *) echo "Usage: $0 keys <prefix> <library>... | install <library>"; exit 1 ;;
esac
