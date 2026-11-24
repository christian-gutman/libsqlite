<?php
/**
 * Canonical libsqlite3 flow in PHP (see bindings/README.md).
 *
 * Usage: php example_full.php [path-to-libsqlite3]
 * Exit code = number of failed checks.
 */

declare(strict_types=1);

require __DIR__ . '/libsqlite.php';

use FFI;

const SQLITE_OK = 0;
const SQLITE_MISUSE = 21;
const SQLITE_CANTOPEN = 14;
const SQLITE_CORE_TYPE_INT = 2;
const SQLITE_CORE_TYPE_TEXT = 6;
const SQLITE_CORE_BUFFER_TOO_SMALL = -2;
const SQLITE_CORE_BLOB_TOO_SMALL = -3;

$DB = 'php_demo.db';
$FIXTURE = 'php_fixture.bin';
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

$libPath = $argv[1] ?? null;
$lib = new LibSqlite($libPath);

echo "libsqlite3 PHP binding\n";
echo "  library " . $lib->staticString($lib->lib->sqlite_core_version()) .
     " / engine " . $lib->staticString($lib->lib->sqlite_core_sqlite_version()) . "\n";

// Clean up from previous runs
foreach ([$DB, $FIXTURE] as $f) {
    if (file_exists($f)) {
        unlink($f);
    }
}

// 1024-byte fixture with embedded 0x00 bytes
file_put_contents($FIXTURE, str_repeat("\x00\x01\x02\xff\xfe", 204));

// ---- 1. open_or_create ----
$rc = FFI::new('int');
$db = $lib->lib->sqlite_core_open_or_create($DB, FFI::addr($rc));
check($db !== null && $rc->cdata === 0, 'open_or_create');
if ($db === null) {
    exit($checks[1]);
}

$err = FFI::new('char[256]');

// ---- 2. execute_command (DDL) ----
check($lib->lib->sqlite_core_execute_command(
    $db,
    'CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)',
    $err,
    256
) === 0, 'create table');

// ---- 3. execute_non_query with named parameters ----
$params = $lib->paramArray([
    $lib->intParam('id', 42),
    $lib->textParam('name', 'Ada'),
]);
$affected = $lib->lib->sqlite_core_execute_non_query(
    $db,
    'INSERT INTO person(id, name) VALUES (:id, :name)',
    $params,
    2,
    $err,
    256
);
check($affected === 1, 'named parameter insert');

// ---- 4. execute_reader (callback counts rows) ----
$seen = 0;
$cb = $lib->countRowsCallback($seen);
$rcRead = $lib->lib->sqlite_core_execute_reader(
    $db,
    'SELECT id, name FROM person ORDER BY id',
    null,
    0,
    $cb,
    null,
    $err,
    256
);
check($rcRead === 1 && $seen === 1, 'reader visits the row');

// ---- 5. execute_scalar (COUNT(*)) ----
$out = FFI::new('sqlite_core_param_t');
$out->type = SQLITE_CORE_TYPE_INT;
$out->Direction = 1; // INPUT
$rcScalar = $lib->lib->sqlite_core_execute_scalar(
    $db,
    'SELECT COUNT(*) FROM person',
    null,
    0,
    FFI::addr($out),
    $err,
    256
);
check($rcScalar === 0 && $out->value->iVal === 1, 'scalar counts rows');

// ---- 6. execute_reader_to_json ----
$jsonPtr = FFI::new('char*');
$rows = $lib->lib->sqlite_core_execute_reader_to_json(
    $db,
    'SELECT id, name FROM person ORDER BY id',
    null,
    0,
    FFI::addr($jsonPtr),
    $err,
    256
);
$doc = $jsonPtr->cdata !== null ? $lib->takeString($jsonPtr) : null;
check($rows === 1 && $doc !== null && str_starts_with($doc, '['), 'reader to json');
echo "  json: " . ($doc ?? '') . "\n";

// ---- 7. insert_json batch ----
check($lib->lib->sqlite_core_insert_json(
    $db,
    'person',
    '[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]',
    $err,
    256
) === 2, 'insert_json batch');

// ---- 8. insert_blob (chunked) + get_blob round trip ----
$written = FFI::new('int');
$fixtureSize = filesize($FIXTURE);
$rcBlob = $lib->lib->sqlite_core_insert_blob(
    $db,
    'person',
    'data',
    2, // rowid 2 -> upsert on existing row id=2
    $FIXTURE,
    0, // default chunk size
    FFI::addr($written),
    $err,
    256
);
check($rcBlob === $fixtureSize && $written->cdata === $fixtureSize, 'insert_blob streams the file');

$sink = FFI::new('char[' . $fixtureSize . ']');
$copied = FFI::new('int');
$rcGet = $lib->lib->sqlite_core_get_blob(
    $db,
    'SELECT data FROM person WHERE id = 2',
    $sink,
    $fixtureSize,
    FFI::addr($copied)
);
check($rcGet === 0 && $copied->cdata === $fixtureSize, 'get_blob reads it back');
$roundTrip = file_get_contents($FIXTURE) === FFI::string($sink, $copied->cdata);
check($roundTrip, 'byte-for-byte round trip');

check($lib->lib->sqlite_core_last_insert_rowid($db) > 0, 'last_insert_rowid');

// ---- 9. transaction paths ----
check($lib->lib->sqlite_core_commit_transaction($db, $err, 256) !== 1, 'commit without transaction fails');
check($lib->lib->sqlite_core_begin_transaction($db, $err, 256) === 1, 'begin');
$lib->lib->sqlite_core_execute_non_query(
    $db,
    "INSERT INTO person(name) VALUES ('Temp')",
    null,
    0,
    $err,
    256
);
check($lib->lib->sqlite_core_rollback_transaction($db, $err, 256) === 1, 'rollback');

$out2 = FFI::new('sqlite_core_param_t');
$out2->type = SQLITE_CORE_TYPE_INT;
$out2->Direction = 1;
$lib->lib->sqlite_core_execute_scalar(
    $db,
    'SELECT COUNT(*) FROM person',
    null,
    0,
    FFI::addr($out2),
    $err,
    256
);
check($out2->value->iVal === 3, 'rollback discarded the row');

// ---- oversize update is reported ----
$BIG = 'php_oversize.bin';
file_put_contents($BIG, str_repeat('x', $fixtureSize * 2));
$rcOver = $lib->lib->sqlite_core_update_blob(
    $db,
    'person',
    'data',
    2,
    $BIG,
    0,
    FFI::addr($written),
    $err,
    256
);
check($rcOver === SQLITE_CORE_BLOB_TOO_SMALL, 'update_blob refuses to exceed the column');
unlink($BIG);

// ---- 10. error paths ----
$rc2 = FFI::new('int');
$missing = $lib->lib->sqlite_core_open_readonly('missing.db', FFI::addr($rc2));
check(!$missing && $rc2->cdata === 14, 'open missing file fails'); // SQLITE_CANTOPEN=14
check($lib->lib->sqlite_core_execute_non_query(
    $db,
    'SELECT 1; SELECT 2',
    null,
    0,
    $err,
    256
) === SQLITE_MISUSE, 'multi-statement rejected');

// ---- close ----
check($lib->lib->sqlite_core_close($db) === 1, 'close');

foreach ([$DB, $FIXTURE] as $f) {
    if (file_exists($f)) {
        unlink($f);
    }
}

echo "\n" . $checks[0] . " checks, " . $checks[1] . " failures\n";
exit($checks[1]);
