#!/bin/bash

if [[ $(uname) == 'MSYS_NT'* ]]
then
    mkdir -p "$SUPPORT_LIBS_PATH" "$SDK_INSTALL_PREFIX/lib"
    cp -r "$SUPPORT_LIBS_SOURCE_PATH/zlib-1.2.12" "$SUPPORT_LIBS_SOURCE_PATH/libiconv-1.16" "$SUPPORT_LIBS_PATH"
    if [[ $? -ne 0 ]]
    then
        echo "ERROR: Copying the DCMTK support libraries failed"
        exit 1
    fi
    cp "$SUPPORT_LIBS_PATH/libiconv-1.16/lib/charset_o.lib" "$SDK_INSTALL_PREFIX/lib"
    cp "$SUPPORT_LIBS_PATH/libiconv-1.16/lib/libiconv_o.lib" "$SDK_INSTALL_PREFIX/lib"
fi

. "$SCRIPTS_ROOT/libs/cmakebuild.sh"
