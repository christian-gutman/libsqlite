# SQL Server for the tests

A local SQL Server 2022 (Express) instance used only as the **reference
engine** when checking the `.NET parity` contract: the same operations the
bindings run against `libsqlite3` can be run against SQL Server through
`Microsoft.Data.SqlClient` (the real ADO.NET provider) to compare
behavior side by side.

This stack is intentionally isolated from other projects in the machine:

| Aspect | Value |
|--------|-------|
| Service name | `libsqlite-sqlserver` |
| Container name | `libsqlite-mssql` |
| Network | `libsqlite-net` (bridge, dedicated) |
| Host port | `14333` (container `1433`) |
| Database | `dblibsqlite` |
| Login | `libsqliteuser` (member of `db_owner`) |
| SA password | `Libsqlite#Pass2026!` |

## Usage

```bash
docker compose -f docker/sqlserver/docker-compose.yaml up -d
# wait ~20 s, then
docker compose -f docker/sqlserver/docker-compose.yaml down -v   # stop and drop the volumes
```

Connection string for the tests:

```
Server=localhost,14333;Database=dblibsqlite;User Id=libsqliteuser;Password=Libsqlite#User2026!;TrustServerCertificate=True
```

`init.sql` creates `dbo.person` / `dbo.person_json` with the same shape as
the C examples (including a `VARBINARY(MAX)` column for the BLOB path), so
`examples/basic.c` and `bindings/` results can be compared row by row.