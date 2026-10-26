#!/bin/bash

. "$SCRIPTS_ROOT/libs/cmakebuild.sh"

# Starviewer links against vtkgdcm, so the SDK is unusable without it.
if [[ $(uname) == 'MSYS_NT'* ]]
then
    VTKGDCM_LIBRARY="$SDK_INSTALL_PREFIX/bin/vtkgdcm-9.2.dll"
else
    VTKGDCM_LIBRARY="$SDK_INSTALL_PREFIX/lib/libvtkgdcm-9.2.so"
fi
if [[ ! -e $VTKGDCM_LIBRARY ]]
then
    echo "ERROR: $VTKGDCM_LIBRARY was not installed"
    find "$BUILD_DIR" "$SDK_INSTALL_PREFIX" -iname '*vtkgdcm*' -not -iname '*.h'
    exit 1
fi
