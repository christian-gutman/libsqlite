# Test containers

SQLite-only: every container here builds the library from this repository
and runs the suites (and one language example) against the real artifact.

| File | Purpose |
|------|---------|
| `Dockerfile` | Plain Linux build: shared + static, full CTest suite. |
| `bindings/<lang>.Dockerfile` | Builds the library, runs the suites, then compiles and runs the `bindings/<lang>` canonical example. |
| `build.cmd <lang>` | Build the image, run it, **remove the image** and print `docker system df` (keeps disk usage flat). |

```bat
docker\build.cmd go
docker\build.cmd java      # downloads the JNA jar, runs javac + the example
```

Flow of a language container:

1. `FROM` the official toolchain image (`golang`, `python`, `rust`, ...).
2. `COPY . .` (the repository is the build context; `.dockerignore` keeps
   it small).
3. `cmake` configure + build + `ctest` (tests and examples included).
4. Compile and run the binding example against the produced
   `bin/x64-Linux/shared/Release/libsqlite3.so`.
5. The wrapper removes the image afterwards, so nothing accumulates.

The examples mutate files only inside the container; the host repository
is never written to.