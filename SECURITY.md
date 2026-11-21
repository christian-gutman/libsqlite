# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 1.0.x   | ✅ Security fixes |
| < 1.0   | ❌ No fixes |

## Reporting a vulnerability

Please **do not open a public issue** for security problems.

Report privately through GitHub Security Advisories
(**Security → Report a vulnerability** in this repository) or contact the
maintainer directly. Include:

- Affected version (both strings from `sqlite_core_version()` and
  `sqlite_core_sqlite_version()`), platform and linkage (shared/static).
- A minimal reproduction or proof of concept.
- The expected vs. actual behavior.

You will receive an acknowledgement within 72 hours and an assessment with a
fix timeline within 7 days. Credit is given in the release notes unless you
prefer to stay anonymous.

## Notes for users

- SQL text is always prepared through the SQLite statement engine — values
  are bound as parameters and never concatenated into SQL
  (`sqlite_core_param_t` binding, `sqlite_core_insert_json`).
- Table and column identifiers are quoted as SQLite identifiers.
- The library opens files through SQLite's own file access; no network or
  extension loading is compiled in (`SQLITE_OMIT_LOAD_EXTENSION`).