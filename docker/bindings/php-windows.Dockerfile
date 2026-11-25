# libsqlite3 — PHP (FFI) binding test container, Windows variant.
#
# Same flow as php.Dockerfile, against the Windows build: shared library
# (libsqlite3.dll + libsqlite3.lib), the CTest suite, the compiled shim and
# the PHP canonical example.
#
# NOT PART OF CI — and it cannot be: this container needs a Windows host to
# run (Windows containers share the host kernel), so it is launched from
# docker/build.cmd on a Windows machine and documented rather than wired
# into .github/workflows/ci.yml. It has also not been verified on a Windows
# host yet: the recipe below is the intended build, and any Windows-specific
# fix lands in this file with the reason, the way the other containers' notes
# do.
#
# Toolchain choice: MinGW-w64 (gcc) instead of MSVC. The shim is plain C, so
# gcc is enough, and MinGW can link straight against the DLL that CMake
# produces (-l:libsqlite3.dll) without needing the MSVC import library.

FROM mcr.microsoft.com/windows/servercore:ltsc2022

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

# 1. toolchain + PHP. php (chocolatey) also pulls the extensions ini layout;
#    php8.2-ffi is enabled through PHP itself (ffi.enable=preload is NOT
#    needed: the binding uses FFI::cdef at runtime).
RUN choco install -y php cmake ninja mingw --no-progress

WORKDIR C:\src
COPY . .

# 2. library + suites (release, shared). Ninja + gcc: cmake's Visual Studio
#    generator is not installed, and this avoids it.
RUN cmake -S src\sqlite -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_C_COMPILER=gcc `
    ; if ($LASTEXITCODE -ne 0) { throw 'configure failed' }

RUN cmake --build build --parallel `
    ; if ($LASTEXITCODE -ne 0) { throw 'build failed' }

RUN ctest --test-dir build --output-on-failure `
    ; if ($LASTEXITCODE -ne 0) { throw 'ctest failed' }

# 3. the shim: ~120 lines of C, compiled here for the platform that runs it.
#    -l:libsqlite3.dll links against the DLL itself (MinGW), so no import
#    library conversion is needed. The DLL must be next to the example or on
#    PATH at run time, hence the copy below.
RUN $lib = Get-ChildItem -Path C:\src\bin -Recurse -Filter libsqlite3.dll | Select-Object -First 1 -ExpandProperty FullName `
    ; Write-Host \"shim libdir: $(Split-Path $lib)\" `
    ; gcc -shared -O2 -I C:\src\src\sqlite\core C:\src\bindings\php\src\php_shim.c `
        -L (Split-Path $lib) -l:libsqlite3.dll -o C:\src\bindings\php\libsqlite_shim.dll `
    ; if ($LASTEXITCODE -ne 0) { throw 'shim build failed' } `
    ; Copy-Item $lib -Destination C:\src\bindings\php\libsqlite3.dll -Force

# 4. PHP canonical example (FFI + shim, no PHP packages needed). The exit
#    code is captured and re-raised so "docker run" fails when the example
#    fails, instead of being masked by the trailing echo.
WORKDIR C:\src\bindings\php
CMD ["powershell", "-Command", "$lib = Get-ChildItem -Path C:\\src\\bin -Recurse -Filter libsqlite3.dll | Select-Object -First 1 -ExpandProperty FullName; Write-Host \\\"dll at: $lib\\\"; php example_full.php $lib .\\libsqlite_shim.dll; $rc = $LASTEXITCODE; Write-Host \\\"PHP_EXIT=$rc\\\"; exit $rc"]