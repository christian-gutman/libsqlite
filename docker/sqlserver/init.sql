IF NOT EXISTS (SELECT * FROM sys.databases WHERE name = 'dblibsqlite')
BEGIN
    CREATE DATABASE dblibsqlite;
END
GO

USE dblibsqlite;
GO

IF NOT EXISTS (SELECT * FROM sys.server_principals WHERE name = 'libsqliteuser')
BEGIN
    CREATE LOGIN libsqliteuser WITH PASSWORD = 'Libsqlite#User2026!';
END
GO

IF NOT EXISTS (SELECT * FROM sys.database_principals WHERE name = 'libsqliteuser')
BEGIN
    CREATE USER libsqliteuser FOR LOGIN libsqliteuser;
    ALTER ROLE db_owner ADD MEMBER libsqliteuser;
END
GO

-- Same shape as the C example in src/sqlite/examples/basic.c, so the
-- SQL Server side can be compared with the libsqlite3 contract.
IF OBJECT_ID('dbo.person', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.person (
        id   INT IDENTITY(1,1) PRIMARY KEY,
        name NVARCHAR(100) NOT NULL,
        data VARBINARY(MAX) NULL
    );
END
GO

INSERT INTO dbo.person (name, data) VALUES (N'Ada',  0x00);
INSERT INTO dbo.person (name, data) VALUES (N'Grace',0x01);
GO

-- Batch table used to compare sqlite_core_insert_json (JSON -> rows).
IF OBJECT_ID('dbo.person_json', 'U') IS NULL
BEGIN
    CREATE TABLE dbo.person_json (
        id   INT IDENTITY(1,1) PRIMARY KEY,
        name NVARCHAR(100) NOT NULL
    );
END
GO

INSERT INTO dbo.person_json (name) VALUES (N'Line1'), (N'Line2');
GO