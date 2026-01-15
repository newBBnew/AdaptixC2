package database

import (
	"database/sql"

	_ "github.com/mattn/go-sqlite3"
)

type DBMS struct {
	database *sql.DB
	exists   bool
}

func NewDatabase(dbPath string) (*DBMS, error) {
	var err error

	dbms := &DBMS{
		exists: true,
	}

	dbms.database, err = sql.Open("sqlite3", dbPath)
	if err != nil {
		dbms.exists = false
	}

	if dbms.exists {
		err = dbms.DatabaseInit()
		if err != nil {
			dbms.exists = false
		}
	}
	return dbms, err
}

func (dbms *DBMS) DatabaseInit() error {
	var (
		err              error
		createTableQuery string
	)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Listeners" (
    	"ListenerName" TEXT NOT NULL UNIQUE, 
    	"ListenerRegName" TEXT NOT NULL,
    	"ListenerConfig" TEXT NOT NULL,
    	"CreateTime" BIGINT,
    	"Watermark" TEXT NOT NULL,
    	"CustomData" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Chat" (
    	"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"Username" TEXT NOT NULL,
    	"Message" TEXT NOT NULL,
    	"Date" BIGINT,
		"SessionId" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	// Migration: Check if SessionId column exists in Chat, if not add it
	// This is a simple migration check for SQLite
	var sessionIdExists string
	err = dbms.database.QueryRow("SELECT SessionId FROM Chat LIMIT 1").Scan(&sessionIdExists)
	if err != nil && err != sql.ErrNoRows {
		// Column likely doesn't exist
		_, _ = dbms.database.Exec(`ALTER TABLE Chat ADD COLUMN "SessionId" TEXT;`)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "ChatSessions" (
		"SessionId" TEXT NOT NULL UNIQUE,
		"Name" TEXT,
		"StartTime" BIGINT,
		"EndTime" BIGINT,
		"Status" TEXT
	);`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Downloads" (
    	"FileId" TEXT NOT NULL UNIQUE, 
    	"AgentId" TEXT NOT NULL,
    	"AgentName" TEXT NOT NULL,
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"RemotePath" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"TotalSize" INTEGER,
    	"RecvSize" INTEGER,
    	"Date" BIGINT,
    	"State" INTEGER
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Screenshots" (
    	"ScreenId" TEXT NOT NULL UNIQUE, 
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"Note" TEXT,
    	"Date" BIGINT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Agents" (
    	"Id" TEXT NOT NULL UNIQUE, 
    	"Crc" TEXT NOT NULL,
    	"Name" TEXT NOT NULL,
    	"SessionKey" BLOB NOT NULL,
    	"Listener" TEXT NOT NULL,
    	"Async" INTEGER,
    	"ExternalIP" TEXT,
    	"InternalIP" TEXT,
    	"GmtOffset" INTEGER,
    	"Sleep" INTEGER,
    	"Jitter" INTEGER,
    	"Pid" TEXT,
    	"Tid" TEXT,
    	"Arch" TEXT,
    	"Elevated" INTEGER,
    	"Process" TEXT,
    	"Os" INTEGER,
    	"OsDesc" TEXT,
    	"Domain" TEXT,
    	"Computer" TEXT,
    	"Username" TEXT,
    	"Impersonated" TEXT,
    	"OemCP" INTEGER,
    	"ACP" INTEGER,
    	"CreateTime" BIGINT,
    	"LastTick" INTEGER,
    	"WorkingTime" INTEGER,	
    	"KillDate" INTEGER,
    	"Tags" TEXT,
    	"Mark" TEXT,
    	"Color" TEXT,
    	"TargetId" TEXT,
    	"CustomData" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Tasks" (
    	"TaskId" TEXT NOT NULL UNIQUE, 
    	"AgentId" TEXT NOT NULL,
    	"TaskType" INTEGER,
    	"Client" TEXT,
    	"User" TEXT,
    	"Computer" TEXT,
    	"StartDate" BIGINT,
    	"FinishDate" BIGINT,
    	"CommandLine" TEXT NOT NULL,
    	"MessageType" INTEGER,
    	"Message" TEXT,
    	"ClearText" TEXT,
    	"Completed" INTEGER
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Consoles" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"AgentId" TEXT NOT NULL,
    	"Packet" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Pivots" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"PivotId" TEXT NOT NULL,
    	"PivotName" TEXT NOT NULL,
    	"ParentAgentId" TEXT NOT NULL,
    	"ChildAgentId" TEXT NOT NULL
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Credentials" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"CredId" TEXT NOT NULL,
    	"Username" TEXT,
    	"Password" TEXT,
    	"Realm" TEXT,
    	"Type" TEXT,
    	"Tag" TEXT,
    	"Date" BIGINT,
    	"Storage" TEXT,
		"AgentId" TEXT,
		"Host" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Targets" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"TargetId" TEXT NOT NULL UNIQUE, 
    	"Computer" TEXT,
    	"Domain" TEXT,
    	"Address" TEXT,
    	"Os" INTEGER,
    	"OsDesk" TEXT,
    	"Tag" TEXT,
    	"Info" TEXT,
    	"Date" BIGINT,
		"Alive" BOOLEAN,
		"Agents" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "FileDeliveryFiles" (
		"ID" TEXT NOT NULL UNIQUE,
		"FileName" TEXT NOT NULL,
		"Sha256" TEXT NOT NULL,
		"Size" INTEGER,
		"Owner" TEXT,
		"CreatedAt" BIGINT,
		"StoredPath" TEXT NOT NULL
	);`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "FileDeliveryLinks" (
		"Token" TEXT NOT NULL UNIQUE,
		"FileID" TEXT NOT NULL,
		"CreatedAt" BIGINT,
		"ExpiresAt" BIGINT,
		"MaxUses" INTEGER,
		"Uses" INTEGER,
		"AllowedIP" TEXT
	);`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "FileDeliveryDownloads" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
		"Token" TEXT NOT NULL,
		"FileId" TEXT NOT NULL,
		"Ts" BIGINT,
		"IP" TEXT,
		"UserAgent" TEXT,
		"Result" TEXT
	);`
	_, err = dbms.database.Exec(createTableQuery)

	// 添加MSF相关的表（只保存adaptix需要的元数据）
	// MSF控制台映射表
	createTableQuery = `CREATE TABLE IF NOT EXISTS "MSFConsoleMapping" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
		"ConsoleId" TEXT NOT NULL,
		"UserId" TEXT NOT NULL,
		"Busy" INTEGER DEFAULT 0,
		"Prompt" TEXT,
		"CreatedAt" BIGINT,
		"LastActive" BIGINT
	);`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return err
	}

	return err
}

func (dbms *DBMS) DatabaseExists() bool {
	return dbms.exists
}
