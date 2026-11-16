@echo off
rem Runs one fuzzer for a number of seconds.
rem
rem   run.cmd <dcmtk|gdcm> <build folder> <seed folder> <seconds>
rem
rem Run it from an x64 Visual Studio developer prompt, so that the AddressSanitizer runtime is found.
rem New inputs that reach new code go to <build folder>\corpus_<name>, and inputs that crash the
rem fuzzer to <build folder>\crashes_<name>. Run it again to carry on from that corpus.
rem
rem A file that asks for a huge allocation makes the allocator return null (as it does for the
rem application), and is not reported as an error. Everything else is.

setlocal
if "%~4"=="" (
    echo usage: run.cmd ^<dcmtk^|gdcm^> ^<build folder^> ^<seed folder^> ^<seconds^>
    exit /b 2
)
set "NAME=%~1"
set "BUILD=%~2"
set "ASAN_OPTIONS=allocator_may_return_null=1:max_allocation_size_mb=512"
if not exist "%BUILD%\corpus_%NAME%" mkdir "%BUILD%\corpus_%NAME%"
if not exist "%BUILD%\crashes_%NAME%" mkdir "%BUILD%\crashes_%NAME%"

"%BUILD%\fuzz_%NAME%.exe" -max_total_time=%~4 -max_len=400000 -timeout=20 -rss_limit_mb=2048 -artifact_prefix="%BUILD%\crashes_%NAME%/" "%BUILD%\corpus_%NAME%" "%~3"
