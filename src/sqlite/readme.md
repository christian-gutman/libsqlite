rmdir /s /q build
cmake -S src/sqlite -B build -A Win32
cmake --build build --config Release

funding