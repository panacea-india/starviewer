@echo off
rem Builds the two fuzzers of fuzz_dicom.cpp with libFuzzer and AddressSanitizer.
rem
rem Run it from an x64 Visual Studio developer prompt (vcvars64.bat). It needs the SDK that
rem Starviewer is built with, with static DCMTK and GDCM libraries.
rem
rem   build.cmd <output folder>            SDK folder defaults to C:\sdk\sdk-install
rem   set SDK=D:\sdk\sdk-install & build.cmd <output folder>
rem
rem AddressSanitizer only instruments this file, not the libraries, so a memory error inside
rem DCMTK or GDCM is caught when it touches memory that the runtime tracks (heap blocks), but not
rem for every kind of error. Building the libraries with /fsanitize=address would find more.

setlocal
if "%~1"=="" (
    echo usage: build.cmd ^<output folder^>
    exit /b 2
)
if "%SDK%"=="" set "SDK=C:\sdk\sdk-install"
set "OUT=%~1"
if not exist "%OUT%" mkdir "%OUT%"

set "COMMON=/nologo /EHsc /std:c++17 /MD /O1 /Zi /fsanitize=fuzzer /fsanitize=address /I"%SDK%\include" /D_CRT_SECURE_NO_WARNINGS /D_DISABLE_STRING_ANNOTATION /D_DISABLE_VECTOR_ANNOTATION"

cl %COMMON% /DFUZZ_DCMTK /I"%SDK%\include\dcmtk" "%~dp0fuzz_dicom.cpp" /Fe:"%OUT%\fuzz_dcmtk.exe" /Fo:"%OUT%\fuzz_dcmtk.obj" /Fd:"%OUT%\fuzz_dcmtk.pdb" ^
    /link /LIBPATH:"%SDK%\lib" /LIBPATH:"%SDK%\dcmtk-support\zlib-1.2.12\lib" /LIBPATH:"%SDK%\dcmtk-support\libiconv-1.16\lib" zlib_o.lib libiconv_o.lib charset_o.lib dcmjpls.lib dcmtkcharls.lib dcmjpeg.lib ijg8.lib ijg12.lib ijg16.lib dcmimage.lib dcmimgle.lib dcmdata.lib oflog.lib ofstd.lib ws2_32.lib netapi32.lib iphlpapi.lib wsock32.lib advapi32.lib
if errorlevel 1 exit /b 1

cl %COMMON% /DFUZZ_GDCM /I"%SDK%\include\gdcm-3.0" "%~dp0fuzz_dicom.cpp" /Fe:"%OUT%\fuzz_gdcm.exe" /Fo:"%OUT%\fuzz_gdcm.obj" /Fd:"%OUT%\fuzz_gdcm.pdb" ^
    /link /LIBPATH:"%SDK%\lib" gdcmMSFF.lib gdcmIOD.lib gdcmDSED.lib gdcmDICT.lib gdcmCommon.lib gdcmjpeg8.lib gdcmjpeg12.lib gdcmjpeg16.lib gdcmopenjp2.lib gdcmcharls.lib gdcmzlib.lib gdcmexpat.lib rpcrt4.lib crypt32.lib ws2_32.lib advapi32.lib
if errorlevel 1 exit /b 1

echo Built %OUT%\fuzz_dcmtk.exe and %OUT%\fuzz_gdcm.exe
