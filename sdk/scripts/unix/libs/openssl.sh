#!/bin/bash
# Windows only: Linux uses the system's OpenSSL, and Qt 5.15.2's Windows build loads the 1.1.1 libraries by name.
if [[ $(uname) != 'MSYS_NT'* ]]
then
    echo "ERROR: The openssl SDK library is only for Windows"
    exit 1
fi

pushd "$SOURCE_DIR" || exit 1

# Only perl.exe is taken from Strawberry Perl: its other directories have a compiler and a make.
export PATH="$TOOLS_DIR/perl/perl/bin:$TOOLS_DIR/nasm-2.16.03:$PATH"
unset MAKEFLAGS

MSYS2_ARG_CONV_EXCL='*' perl.exe Configure VC-WIN64A shared no-tests "--prefix=$OPENSSL_PREFIX" "--openssldir=$OPENSSL_DIR"
if [[ $? -ne 0 ]]
then
    echo "ERROR: Configuring OpenSSL failed"
    exit 1
fi

nmake.exe -nologo
if [[ $? -ne 0 ]]
then
    echo "ERROR: Building OpenSSL failed"
    exit 1
fi

mkdir -p "$SDK_INSTALL_PREFIX/bin" "$SDK_INSTALL_PREFIX/lib" "$SDK_INSTALL_PREFIX/include"
cp libcrypto-1_1-x64.dll libssl-1_1-x64.dll "$SDK_INSTALL_PREFIX/bin" &&
cp libcrypto.lib libssl.lib "$SDK_INSTALL_PREFIX/lib" &&
cp -r include/openssl "$SDK_INSTALL_PREFIX/include"
if [[ $? -ne 0 ]]
then
    echo "ERROR: Installing OpenSSL failed"
    exit 1
fi

# DCMTK looks for an OpenSSL under its support library names, in the layout of the support library package.
DCMTK_OPENSSL="$SDK_INSTALL_PREFIX/dcmtk-support/openssl-1.1.1w"
mkdir -p "$DCMTK_OPENSSL/lib" "$DCMTK_OPENSSL/include"
cp -r include/openssl "$DCMTK_OPENSSL/include" &&
cp libssl.lib "$DCMTK_OPENSSL/lib/dcmtkssl_o.lib" &&
cp libssl.lib "$DCMTK_OPENSSL/lib/dcmtkssl_d.lib" &&
cp libcrypto.lib "$DCMTK_OPENSSL/lib/dcmtkcrypto_o.lib" &&
cp libcrypto.lib "$DCMTK_OPENSSL/lib/dcmtkcrypto_d.lib"
if [[ $? -ne 0 ]]
then
    echo "ERROR: Installing OpenSSL for DCMTK failed"
    exit 1
fi

popd
