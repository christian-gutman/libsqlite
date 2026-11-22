@echo off
rem Builds and runs one language test container, then removes the image
rem (keeps docker system df flat). Usage: docker\build.cmd <language>
rem   e.g. docker\build.cmd go

if "%1"=="" (
    echo usage: docker\build.cmd ^<language^>
    exit /b 1
)

set LANG=%1
set IMAGE=libsqlite-%LANG%:test

echo === building %IMAGE% ===
docker build -f docker/bindings/%LANG%.Dockerfile -t %IMAGE% .
if errorlevel 1 goto :failed

echo === running %IMAGE% ===
docker run --rm %IMAGE%
set RC=%errorlevel%

echo === cleaning image %IMAGE% ===
docker rmi %IMAGE% >nul 2>&1
docker system df
exit /b %RC%

:failed
echo BUILD_FAILED
exit /b 1