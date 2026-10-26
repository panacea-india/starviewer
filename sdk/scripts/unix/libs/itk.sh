#!/bin/bash

. "$SCRIPTS_ROOT/libs/cmakebuild.sh"

# ITK installs its internal Eigen with a nested build whose result it ignores,
# and on Windows that install silently does nothing. Run it here so that its
# output is visible, and fail if Eigen's package config is still missing.
EIGEN_CONFIG="$ITKCMAKEDIR/Modules/ITKInternalEigen3Config.cmake"
if [[ ! -f "$EIGEN_CONFIG" ]]
then
    echo "ITKInternalEigen3 was not installed, installing it explicitly."
    $CMAKE --build "$BUILD_DIR/ITKInternalEigen3-build" --target install
fi
if [[ ! -f "$EIGEN_CONFIG" ]]
then
    echo "ERROR: $EIGEN_CONFIG was not installed."
    exit 1
fi
