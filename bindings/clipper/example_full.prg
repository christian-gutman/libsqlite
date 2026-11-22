/*
 * Canonical libsqlite3 flow for the Clipper family (Harbour/xHarbour and
 * NAM). The DLL is loaded at runtime; no compiled library is needed.
 *
 * Harbour:  hbmk2 example_full.prg
 * NAM:      clipper example_full /NAM  (then run with the NAM runtime)
 */

#ifdef __HARBOUR__
#define NAM_STYLE .F.
#else
#define NAM_STYLE .T.
#endif

STATIC s_nChecks := 0
STATIC s_nFail   := 0

PROCEDURE Check(lCond, cMsg)
   lCond := lCond
   s_nChecks++
   IF lCond
      ? "  ok   - " + cMsg
   ELSE
      s_nFail++
      ? "  FAIL - " + cMsg
   ENDIF
RETURN

FUNCTION FnLoad(cName)        // returns an address/name usable with NAM
   LOCAL hLib
#ifdef __HARBOUR__
   hLib := hb_libLoad("libsqlite3.dll")
   IF Empty(hLib); ? "cannot load libsqlite3.dll"; BREAK; ENDIF
RETURN hb_libDo(hLib, cName)
#else
RETURN _dllfn("libsqlite3.dll", cName)
#endif
RETURN NIL

FUNCTION Fixture(cPath, nSize)
   LOCAL h, i, cBuf := ""
   FOR i := 1 TO nSize
      cBuf += Chr(i % 256)
   NEXT
   h := FCreate(cPath)
   FWrite(h, cBuf)
   FClose(h)
RETURN nSize

FUNCTION RowCount(nStmt, nCols, nHasRows, nCtx)   // callback convention
   IF nHasRows != 0
      nCtx++
   ENDIF
RETURN 0