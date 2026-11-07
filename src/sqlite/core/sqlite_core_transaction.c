#include "sqlite_core_transaction.h"

int SqlLite_Core_BeginTransaction(sqlite3 *db, char *err_buf, int err_buf_max) {
     /* 
     * [txexpected = 1]: Para iniciar una nueva transacción (BEGIN), el estado de 
     * Autocommit esperado debe ser 1 (Encendido/Libre). 
     * Si actualmente está en 0 (Transacción activa), significa que el desarrollador 
     * intentó hacer un doble BEGIN secuencial. El validador interceptará esta 
     * discrepancia y abortará inmediatamente con código -1, impidiendo el error del motor.
     */
    return SqlLite_Core_ExecuteTransaction(db, "BEGIN IMMEDIATE TRANSACTION;",1, err_buf, err_buf_max);
}

int SqlLite_Core_Commit(sqlite3 *db, char *err_buf, int err_buf_max) {
    /* 
     * [txexpected = 0]: Para confirmar y guardar datos (COMMIT), el estado de 
     * Autocommit esperado debe ser 0 (Apagado/Transacción activa). 
     * Si actualmente está en 1 (Libre), significa que no existe ninguna transacción 
     * previa en este handle. El validador detectará la anomalía, evitará inyectar un 
     * comando COMMIT huérfano a SQLite y abortará devolviendo -1.
     */
    return SqlLite_Core_ExecuteTransaction(db, "COMMIT TRANSACTION;",0, err_buf, err_buf_max);
}

int SqlLite_Core_Rollback(sqlite3 *db, char *err_buf, int err_buf_max) {
      /* 
     * [txexpected = 0]: Para deshacer operaciones en memoria (ROLLBACK), el estado de 
     * Autocommit esperado debe ser de igual forma 0 (Apagado/Transacción activa). 
     * Si actualmente está en 1 (Libre), significa que el motor no está procesando ningún 
     * bloque transaccional que requiera ser revertido. Al igual que el Commit, el validador 
     * frena la llamada devolviendo -1 y protegiendo el flujo lógico de la sesión.
     */
    return SqlLite_Core_ExecuteTransaction(db, "ROLLBACK TRANSACTION;", 0, err_buf, err_buf_max);
}