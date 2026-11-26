@echo off
rem Runs every language container that works on Linux, one after the
rem other, removing each image afterwards (keeps docker system df flat).
rem The fifteen languages below are the ones with a verified container;
rem Visual FoxPro and Clipper are excluded on purpose: they are 16/32-bit
rem Windows-only consumers of the DLL and cannot run in a Linux container.
rem phpext is also excluded: its container is a compile probe for an
rem extension that is not finished yet, not a runnable example.
rem
rem Usage: docker\build_all.cmd            (all languages)
rem        docker\build_all.cmd go java    (only these)

setlocal EnableDelayedExpansion

set LANGS=python csharp cpp swift ruby julia kotlin nim fortran go rust nodejs java r php
if not "%~1"=="" set LANGS=%*

set PASSED=
set FAILED=
for %%L in (%LANGS%) do (
    call :runone %%L
    if !errorlevel! NEQ 0 (
        set FAILED=!FAILED! %%L
    ) else (
        set PASSED=!PASSED! %%L
    )
)

echo.
echo ============================================================
echo container matrix:  python csharp cpp swift ruby julia kotlin nim fortran go rust nodejs java r php
if not "!PASSED!"=="" echo PASSED:!PASSED!
if not "!FAILED!"=="" echo FAILED:!FAILED!
echo ============================================================
if not "!FAILED!"=="" exit /b 1
exit /b 0

:runone
echo ============================================================
echo === %1
echo ============================================================
call docker\build.cmd %1
set RC=!errorlevel!
echo RESULT %1 = exit code !RC!
exit /b !RC!