# Pull request

## Description

<!-- What does this PR change and why? Link the issue it closes. -->

Closes #

## Checklist

- [ ] The code and all messages/comments are in **English**
- [ ] Node: the change keeps [docs/behavior.md](../docs/behavior.md) true, or
      updates the contract deliberately (breaking changes need discussion)
- [ ] If the public API changes: `sqlite.def`, the headers **and**
      `src/sqlite/tests/test_exports.c` are updated
- [ ] Tests added or updated (`ctest` covers the change)
- [ ] `ctest` passes locally on the platform(s) affected:
      - [ ] Windows shared
      - [ ] Windows static
      - [ ] Linux
      - [ ] macOS
- [ ] Builds warning-clean (`/W4` on MSVC, `-Wall -Wextra` on GCC/Clang)
- [ ] Docs updated (`README.md`, `README.es.md`, `CHANGELOG.md` when relevant)