# Examples

Usage examples for the libsqlite3 connector (C API).

This folder is copied next to the built binaries by the CMake post-build step.

Planned content (see project milestones):

- `basic.c` — open, execute, read, close
- `parameters.c` — named parameters and directions
- `transaction.c` — begin / commit / rollback
- `json.c` — JSON reader callback and JSON insert

Each example is plain C and compiles against the public headers in
`src/sqlite/core/`.