#!/bin/bash
SOURCE_DIR="$SDK_SOURCE_DIR_BASE/openssl-1.1.1w"
TOOLS_DIR="$SDK_SOURCE_DIR_BASE/openssl-tools"

# OpenSSL reads its configuration and engines from these fixed paths, so they must be places that only
# administrators can write to: Starviewer must not load a file left by another user. Nothing is installed
# there, and libs/openssl.sh copies the files out of the build tree instead of running "nmake install".
OPENSSL_PREFIX='C:\Program Files\Starviewer\OpenSSL'
OPENSSL_DIR='C:\Program Files\Common Files\SSL'
