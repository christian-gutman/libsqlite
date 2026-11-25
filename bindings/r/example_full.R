# Canonical libsqlite3 flow in R (see bindings/README.md).
#
# Usage: Rscript example_full.R [path-to-libsqlite3]
# The shim must be built first: src/build.sh (or the container does it).
# Exit code = number of failed checks.

DB <- "r_demo.db"
FIXTURE <- "r_fixture.bin"
OVERSIZE <- "r_oversize.bin"

checks <- 0
failures <- 0
check <- function(cond, msg) {
  checks <<- checks + 1
  cat(if (isTRUE(cond)) "  ok   - " else "  FAIL - ", msg, "\n", sep = "")
  if (!isTRUE(cond)) failures <<- failures + 1
}

source(file.path("libsqlite.R"))

arg <- commandArgs(trailingOnly = TRUE)
libsqlite_load(if (length(arg) >= 1) arg[1] else NULL)

cat("libsqlite3 R binding\n")
cat("  library ", library_version(), " / engine ", sqlite_version(), "\n", sep = "")

for (f in c(DB, FIXTURE, OVERSIZE)) if (file.exists(f)) unlink(f)
fixture <- as.raw(rep(0:255, 4))          # 1024 bytes, includes 0x00
writeBin(fixture, FIXTURE)

# 1. open_or_create
opened <- libsqlite_open(DB, create = TRUE)
db <- opened$db
check(!is.null(db) && opened$rc == 0L, "open_or_create")
if (is.null(db)) quit(status = failures)

# 2. DDL
rc <- execute_command(db, "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)")
check(rc$rc == 0L, "create table")

# 3. named parameters
rc <- execute_non_query(db,
  "INSERT INTO person(id, name) VALUES (:id, :name)",
  params = list(int_param("id", 42), text_param("name", "Ada")))
check(rc$rc == 1L, "named parameter insert")

# 4. reader: rows are visited (the trampoline counts them)
rc <- execute_reader(db, "SELECT id, name FROM person ORDER BY id")
check(rc$rc == 1L && rc$rows == 1, "reader visits the row")

# 5. scalar
rc <- execute_scalar(db, "SELECT COUNT(*) FROM person")
check(rc$rc == 0L && rc$type == 2L, "scalar counts rows")
check(rc$value == 1, "scalar value is 1")

# 6. reader to json
res <- reader_to_json(db, "SELECT id, name FROM person ORDER BY id")
check(res$rows == 1L && !is.null(res$json) && startsWith(res$json, "["),
      "reader to json")
cat("  json: ", res$json, "\n", sep = "")

# 6b. same document through sqlite_core_json_writer_callback
res2 <- reader_json_callback(db, "SELECT id, name FROM person ORDER BY id")
check(res2$rows == 1L && identical(res2$json, res$json),
      "json_writer_callback matches reader_to_json")

# 7. json in
res <- insert_json(db, "person",
  '[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]')
check(res$rows == 2L, "insert_json batch")

# 8. blob round trip (chunked streaming write + read back)
res <- insert_blob(db, "person", "data", 2, FIXTURE)
cat("  (insert_blob rc=", res$rc, " written=", res$written, " err=",
    res$err, ")\n", sep = "")
check(res$rc == length(fixture) && res$written == length(fixture),
      "insert_blob streams the file")

size <- length(fixture)
res <- get_blob(db, "SELECT data FROM person WHERE id = 2")
check(res$rc == 0L && length(res$data) == size, "get_blob reads it back")
check(identical(res$data, fixture), "byte-for-byte round trip")

check(last_insert_rowid(db) > 0, "last_insert_rowid")

# 9. transactions
rc <- commit_transaction(db)
check(rc$rc != 0L, "commit without transaction fails")
rc <- begin_transaction(db)
check(rc$rc == 1L, "begin")
execute_non_query(db, "INSERT INTO person(name) VALUES ('Temp')")
rc <- rollback_transaction(db)
check(rc$rc == 1L, "rollback")
rc <- execute_scalar(db, "SELECT COUNT(*) FROM person")
check(rc$value == 3, "rollback discarded the row")

# 10. oversize update is reported (column smaller than the file)
writeBin(as.raw(rep(0L, size * 2)), OVERSIZE)
res <- update_blob(db, "person", "data", 2, OVERSIZE)
check(res$rc == -3L, "update_blob refuses to exceed the column")
unlink(OVERSIZE)

# 11. error paths
opened2 <- libsqlite_open("missing.db", readonly = TRUE)
check(is.null(opened2$db) && opened2$rc == 14L, "open missing file fails")
rc <- execute_non_query(db, "SELECT 1; SELECT 2")
check(rc$rc == 21L, "multi-statement rejected")

# error diagnostics + busy timeout: the three one-liner symbols every
# binding must exercise (bindings/CONTRACT.md). error_message is static
# library data, so it is copied and never freed.
check(set_busy_timeout(db, 1000L) == 1L, "set_busy_timeout")
check(error_code(db) != 0L, "error_code reports the engine failure")
check(error_code_extended(db) != 0L, "error_code_extended reports the engine failure")
check(nzchar(error_message(db)), "error_message reports the last failure")

# 12. close
rc <- libsqlite_close(db)
check(rc == 1L, "close")

for (f in c(DB, FIXTURE)) if (file.exists(f)) unlink(f)
cat("\n", checks, " checks, ", failures, " failures\n", sep = "")
quit(status = failures)