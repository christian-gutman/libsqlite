<?php
/**
 * Canonical libsqlite3 flow through the Zend extension (bindings/CONTRACT.md).
 *
 * This is the same canonical flow every other binding implements, run against
 * the native extension instead of PHP FFI. The extension exists precisely
 * because of the two things FFI cannot do: build a C function pointer for
 * sqlite_core_execute_reader, and reach sqlite_core_json_writer_callback.
 *
 * Usage: php example_full.php
 * Exit code = number of failed checks.
 *
 * No library path is needed: the extension links against libsqlite3 itself,
 * which is the whole difference from bindings/php/example_full.php.
 */

declare(strict_types=1);

use LibSqlite\Connection;
use LibSqlite\Exception;

/* LIBSQLITE_TYPE_* are global constants the extension itself registers
 * (REGISTER_LONG_CONSTANT in libsqlite.c), so they are available as-is:
 * declaring them here again would only raise "already defined", and a
 * `use const` on a non-compound name is a no-op that PHP warns about.
 *
 * The SQLite result codes below are the engine's own values. No constant
 * is re-exported for them on purpose: a binding that mirrors engine codes
 * with its own numbers eventually drifts from them. */
const SQLITE_OK = 0;
const SQLITE_MISUSE = 21;
const SQLITE_CANTOPEN = 14;
const LIBSQLITE_BLOB_TOO_SMALL = -3;

$DB = 'phpext_demo.db';
$FIXTURE = 'phpext_fixture.bin';
$BIG = 'phpext_oversize.bin';

$checks = [0, 0]; // [total, failures]

function check(bool $cond, string $msg): void
{
    global $checks;
    $checks[0]++;
    echo str_pad(($cond ? '  ok   - ' : '  FAIL - '), 12) . $msg . "\n";
    if (!$cond) {
        $checks[1]++;
    }
}

foreach ([$DB, $FIXTURE, $BIG] as $f) {
    if (file_exists($f)) {
        unlink($f);
    }
}

// 1024-byte fixture with embedded 0x00 bytes
file_put_contents($FIXTURE, str_repeat("\x00\x01\x02\xff\xfe", 204));
$fixtureSize = filesize($FIXTURE);

echo "libsqlite3 PHP extension binding\n";
echo "  extension version " . libsqlite_version() .
     " / wrapper " . libsqlite_version() .
     " / engine " . libsqlite_sqlite_version() . "\n";
echo "  loaded extensions: " .
     (extension_loaded('libsqlite') ? 'libsqlite' : 'NOT LOADED') . "\n";

/* The connection is an object: the handle lives inside it and the destructor
 * closes it, exactly like the C wrappers do. */
$db = Connection::openOrCreate($DB);
check($db instanceof Connection, 'openOrCreate returns a Connection');

// ---- 1. is_readonly (a read-write handle answers 0) ----
check($db->isReadonly() === 0, 'is_readonly false on a read-write handle');

// ---- 2. execute_command (DDL) ----
check($db->command(
    'CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)'
) === SQLITE_OK, 'create table');

// ---- 3. execute_non_query with named parameters ----
$affected = $db->nonQuery(
    'INSERT INTO person(id, name) VALUES (:id, :name)',
    [
        ['name' => 'id',   'type' => LIBSQLITE_TYPE_INT,  'value' => 42],
        ['name' => 'name', 'type' => LIBSQLITE_TYPE_TEXT, 'value' => 'Ada'],
    ]
);
check($affected === 1, 'named parameter insert');

// ---- 4. execute_reader through a real PHP callable ----
// The reason this extension exists: the callback is a C function pointer
// built from a PHP callable, which FFI cannot do.
$seen = 0;
$columns = 0;
$rc = $db->reader(
    function (int $stmt, int $cols, int $hasrows) use (&$seen, &$columns): int {
        if ($hasrows === 1) {
            $seen++;
            $columns = $cols;
        }
        return 0;
    },
    'SELECT id, name FROM person ORDER BY id'
);
check($rc === 1 && $seen === 1 && $columns === 2, 'reader visits the row');

// The abort check needs a result set with more than one row, so it runs
// after the batch insert (below) rather than here, where person has one.

// ---- 5. execute_scalar ----
$scalar = $db->scalar('SELECT COUNT(*) FROM person');
check($scalar['rc'] === SQLITE_OK
      && $scalar['type'] === LIBSQLITE_TYPE_INT
      && $scalar['value'] === 1, 'scalar counts rows');

// A text scalar needs a destination buffer; empty results are a success
// with TYPE_NULL.
$textScalar = $db->scalar("SELECT name FROM person WHERE id = 42");
check($textScalar['rc'] === SQLITE_OK
      && $textScalar['type'] === LIBSQLITE_TYPE_TEXT
      && $textScalar['value'] === 'Ada', 'scalar reads text');

$emptyScalar = $db->scalar("SELECT name FROM person WHERE id = 999");
check($emptyScalar['rc'] === SQLITE_OK
      && $emptyScalar['type'] === LIBSQLITE_TYPE_NULL
      && $emptyScalar['value'] === null, 'empty scalar is a success with TYPE_NULL');

// ---- 6. execute_reader_to_json ----
$json = $db->readerToJson('SELECT id, name FROM person ORDER BY id');
check($json['rc'] === 1 && $json['rows'] === 1
      && is_string($json['json'])
      && str_starts_with($json['json'], '['), 'readerToJson returns the document');
check(str_contains((string) $json['json'], '"id": 42'), 'readerToJson carries the row');
echo "  json: " . ($json['json'] ?? '') . "\n";

// ---- 7. insert_json batch ----
check($db->insertJson(
    'person',
    '[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]'
) === 2, 'insert_json batch');

// A non-zero return from the callback aborts the iteration (contract step
// 4). person now holds three rows, so the callback is asked exactly once
// and the reader reports the rows it actually processed.
$abortedRows = 0;
$rcAbort = $db->reader(
    function (int $stmt, int $cols, int $hasrows) use (&$abortedRows): int {
        $abortedRows++;
        /* stop on the first real row */
        return $hasrows === 1 ? 1 : 0;
    },
    'SELECT id, name FROM person ORDER BY id'
);
check($abortedRows === 1 && $rcAbort === 1,
      'a non-zero callback return aborts the iteration');

// ---- 8. insert_blob (chunked) + get_blob round trip ----
$ins = $db->insertBlob('person', 'data', 2, $FIXTURE);
check($ins['rc'] === $fixtureSize && $ins['written'] === $fixtureSize,
      'insert_blob streams the file');

$blob = $db->getBlob('SELECT data FROM person WHERE id = 2', $fixtureSize);
check($blob['rc'] === SQLITE_OK && $blob['copied'] === $fixtureSize,
      'get_blob reads it back');
check(hash_equals((string) file_get_contents($FIXTURE), (string) $blob['data']),
      'byte-for-byte round trip (embedded 0x00 included)');

check($db->lastInsertRowid() > 0, 'last_insert_rowid');

// ---- 9. transaction paths ----
check($db->commit() !== 1, 'commit without transaction fails');
check($db->begin() === 1, 'begin');
$db->nonQuery("INSERT INTO person(name) VALUES ('Temp')");
check($db->rollback() === 1, 'rollback');

// execute_transaction is a guard, not a runner: txexpected is compared
// against sqlite3_get_autocommit(db), and autocommit is 1 when NO
// transaction is open. So txexpected = 1 means "there must be no
// transaction", and txexpected = 0 means "there must be one" - the
// opposite of what the name suggests, which is worth asserting rather than
// assuming.
check($db->executeTransaction('SELECT 1', 1) === 1,
      'execute_transaction runs when no transaction is open');

$db->begin();
check($db->executeTransaction("SELECT 1", 1) === -1,
      'execute_transaction refuses when a transaction is already active');
check($db->executeTransaction(
    "INSERT INTO person(name) VALUES ('TxOne')", 0
) === 1, 'execute_transaction runs inside a transaction');
check($db->rollback() === 1, 'rollback discards the execute_transaction row');

$afterRollback = $db->scalar('SELECT COUNT(*) FROM person');
check($afterRollback['value'] === 3, 'rollback discarded the row');

// ---- update_blob refuses to exceed the column ----
file_put_contents($BIG, str_repeat('x', $fixtureSize * 2));
$over = $db->updateBlob('person', 'data', 2, $BIG);
check($over['rc'] === LIBSQLITE_BLOB_TOO_SMALL, 'update_blob refuses to exceed the column');
unlink($BIG);

// ---- 10. error paths ----
try {
    Connection::openReadonly('missing_phpext.db');
    check(false, 'open_readonly of a missing file fails');
} catch (Exception $e) {
    check(true, 'open_readonly of a missing file fails');
}

check($db->nonQuery('SELECT 1; SELECT 2') === SQLITE_MISUSE,
      'multi-statement rejected');

// ---- 11. the string helpers, all of them library-freeing ----
check(libsqlite_param_clean_prefix(':name') === 'name', 'param_clean_prefix');
check(libsqlite_string_concat('libsqlite', ' says hi') === 'libsqlite says hi',
      'string_concat');
check(libsqlite_string_utf8_to_ansi('plain ascii') === 'plain ascii',
      'utf8_to_ansi round trips ascii');
check(libsqlite_string_ansi_to_utf8('plain ascii') === 'plain ascii',
      'ansi_to_utf8 round trips ascii');

/* sqlite_core_string_format is a plain C variadic: PHP cannot build a
 * va_list, so the wrapper forwards a fixed maximum of three arguments with
 * the literal each PHP type maps to. */
$formatted = libsqlite_string_format('%s=%d', 'n', 42);
check($formatted === 'n=42', 'string_format');
$formattedOne = libsqlite_string_format('%s says hi', 'phpext');
check($formattedOne === 'phpext says hi', 'string_format with one argument');

// The exported callback's address is reachable from PHP here - and it is
// an actual pointer, which is what made an extension necessary.
$callbackAddress = libsqlite_json_writer_callback();
check(is_int($callbackAddress) && $callbackAddress !== 0,
      'json_writer_callback address resolved');

// ---- 12. error diagnostics, read after a real engine failure ----
// The precedence rule (bindings/CONTRACT.md, sqlite_core_error.h): the
// return value decides; the accessors add engine detail only for failures
// the engine produced. A multi-statement rejection above never reached the
// engine, so the error is provoked here instead.
check($db->setBusyTimeout(1000) === 1, 'set_busy_timeout');
$db->nonQuery('SELECT * FROM no_such_table');
check($db->errorCode() !== 0, 'error_code reports the engine failure');
check($db->errorCodeExtended() !== 0, 'error_code_extended reports the failure');
check($db->errorMessage() !== '', 'error_message reports the last failure');

// ---- 13. close ----
check($db->close() === 1, 'close');

// A closed connection refuses further calls instead of passing a stale
// handle to the engine.
try {
    $db->command('SELECT 1');
    check(false, 'a closed connection throws instead of using a stale handle');
} catch (Exception $e) {
    check(true, 'a closed connection throws instead of using a stale handle');
}

foreach ([$DB, $FIXTURE] as $f) {
    if (file_exists($f)) {
        unlink($f);
    }
}

echo "\n" . $checks[0] . " checks, " . $checks[1] . " failures\n";
exit($checks[1]);