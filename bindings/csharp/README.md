# C# / .NET

`LibSqlite.cs` declares the full frozen API (34 functions) with
P/Invoke; `Param` reproduces the `sqlite_core_param_t` union with
`[FieldOffset]` and the callback is a `RowCallback` delegate.

```bash
# from bindings/csharp (targets net8.0; see csharp.csproj)
dotnet build -c Release
cp ../../bin/x64-Windows/shared/Release/libsqlite3.dll bin/Release/net6.0/
dotnet run --no-build -c Release
```

Notes:

- `Lib.PtrToStringAnsi` reads static strings (versions, error messages);
  `Lib.PtrToStringFree` also releases the SQLite-allocated ones
  (`string_format`, `string_concat`, `*_to_ansi/utf8`,
  `execute_reader_to_json`). Never free with `Marshal.FreeHGlobal`.
- `Lib.Text` keeps the parameter text in unmanaged memory
  (`StringToHGlobalAnsi`) — release the handles after the call
  (`Marshal.FreeHGlobal`) as the example does.
- Verified against the x64 build: 19 checks, 0 failures.