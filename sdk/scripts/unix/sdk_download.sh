#!/bin/bash
SCRIPTS_ROOT=$(cd $(dirname $BASH_SOURCE) && pwd)
. "$SCRIPTS_ROOT/config/setup.sh"

mkdir -p "${DOWNLOAD_PREFIX}"
# ALIB instead of LIB to avoid an enviroment variable naming conflict with MSVC
for ALIB in $LIBS
do
    case $ALIB in
        qt)
            if [[ $(uname) == 'Linux' ]]
            then
                download_and_verify b5ae1ad8f0bd288b0d5b239669dc077725c5d919bc7f217c67b709a4e9702e6e \
                                    'qt-opensource-linux-x64-5.12.6.run' \
                                    'https://download.qt.io/archive/qt/5.12/5.12.6/qt-opensource-linux-x64-5.12.6.run'
            elif [[ $(uname) == 'Darwin' ]]
            then
                download_and_verify 7eb75902fac1ebea0a31e4a3b6988f454c410427dafb522797ba09d4a5688a22 \
                                    'qt-opensource-mac-x64-5.12.6.dmg' \
                                    'https://download.qt.io/archive/qt/5.12/5.12.6/qt-opensource-mac-x64-5.12.6.dmg'
            elif [[ $(uname) == 'MSYS_NT'* ]]
            then
                download_and_verify 7e7fd6bea5b1992771eb705188061acdf2121a59e9b2ccfc355016e22dded7a3 \
                                    'qt-opensource-windows-x86-5.12.6.exe' \
                                    'https://download.qt.io/archive/qt/5.12/5.12.6/qt-opensource-windows-x86-5.12.6.exe'
            fi
            ;;
        openssl)
            # Only Windows builds it. Perl and NASM are build tools, and are used only for this library.
            download_and_verify cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8 \
                                'openssl-1.1.1w.tar.gz' \
                                'https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1w/openssl-1.1.1w.tar.gz'
            download_and_verify 754f3e2a8e473dc68d1540c7802fb166a025f35ef18960c4564a31f8b5933907 \
                                'strawberry-perl-5.40.0.1-64bit-portable.zip' \
                                'https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54001_64bit_UCRT/strawberry-perl-5.40.0.1-64bit-portable.zip'
            download_and_verify 3ee4782247bcb874378d02f7eab4e294a84d3d15f3f6ee2de2f47a46aa7226e6 \
                                'nasm-2.16.03-win64.zip' \
                                'https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip'
            ;;
        dcmtk)
            if [[ $(uname) == 'MSYS_NT'* ]]
            then
                download_and_verify 512042d1e385868e48c589bccc4cac629a8723407bfe13d79df1f3bfff319513 \
                                    'dcmtk-3.6.7-win64-support-MD-iconv-msvc-15.9.zip' \
                                    'https://dicom.offis.de/download/dcmtk/dcmtk367/support/dcmtk-3.6.7-win64-support-MD-iconv-msvc-15.9.zip'
            fi
            download_and_verify 17705dcdb2047d1266bb4e92dbf4aa6d4967819e8e3e94f39b7df697661b4860 \
                                'dcmtk-DCMTK-3.6.7.tar.gz' \
                                'https://github.com/DCMTK/dcmtk/archive/refs/tags/DCMTK-3.6.7.tar.gz'
            ;;
        vtk)
            download_and_verify 1ec54e992cdb1d36472aa2cc24f5b69c500e0a1c254c26e59ff17710e52a477e \
                                'VTK-9.2.5.tar.gz' \
                                'https://github.com/Kitware/VTK/archive/refs/tags/v9.2.5.tar.gz'
            ;;
        gdcm)
            download_and_verify d88519a094797c645ca34797a24a14efc10965829c4c3352c8ef33782a556336 \
                                'GDCM-3.0.24.tar.gz' \
                                'https://github.com/malaterre/GDCM/archive/refs/tags/v3.0.24.tar.gz'
            ;;
        itk)
            download_and_verify 57a4471133dc8f76bde3d6eb45285c440bd40d113428884a1487472b7b71e383 \
                                'InsightToolkit-5.3.0.tar.gz' \
                                'https://github.com/InsightSoftwareConsortium/ITK/releases/download/v5.3.0/InsightToolkit-5.3.0.tar.gz'
            ;;
        ecm)
            download_and_verify 6495fb18f8736cc3411fe3a6aab1f6f04c26e3a574ef1725378b7cb1c508a388 \
                                'extra-cmake-modules-5.64.0.tar.gz' \
                                'https://github.com/KDE/extra-cmake-modules/archive/v5.64.0.tar.gz'
            ;;
        threadweaver)
            download_and_verify 3c0051535d1d0755c64394d6a7bcd6f0dd4ccdfeaa292dfe80feef4916b2bac8 \
                                'threadweaver-5.64.0.tar.gz' \
                                'https://github.com/KDE/threadweaver/archive/v5.64.0.tar.gz'
            ;;
    esac
done
