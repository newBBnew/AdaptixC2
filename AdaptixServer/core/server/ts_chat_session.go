package server

import (
	"AdaptixServer/core/utils/logs"
	"fmt"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/google/uuid"
)

func (ts *Teamserver) TsSessionCreate(name string) string {
	sessionId := uuid.New().String()
	startTime := time.Now().UTC().Unix()

	if name == "" {
		name = fmt.Sprintf("Session %s", time.Now().Format("2006-01-02 15:04:05"))
	}

	err := ts.DBMS.DbSessionCreate(sessionId, name, startTime)
	if err != nil {
		logs.Error("CHAT", "Failed to create session: %s", err.Error())
		return ""
	}
	return sessionId
}

func (ts *Teamserver) TsSessionArchiveCurrent() (string, error) {
	// 1. Create a new Session ID for the *current* content to be archived into
	sessionId := uuid.New().String()
	startTime := time.Now().UTC().Unix()
	name := fmt.Sprintf("Archived %s", time.Now().Format("2006-01-02 15:04:05"))

	// 2. Create the session record
	err := ts.DBMS.DbSessionCreate(sessionId, name, startTime)
	if err != nil {
		return "", err
	}

	// 3. Move all "active" messages (SessionId IS NULL) to this new SessionId
	err = ts.DBMS.DbChatArchiveCurrent(sessionId)
	if err != nil {
		return "", err
	}

	// 4. Clear in-memory messages since they are now archived
	// We need to filter out messages that were just archived from the live broadcast list?
	// Actually, for the clients, we probably want to send a "ClearChat" signal
	// or they will reload and see nothing.
	ts.messages.Clear()

	// 5. Notify clients to clear their chat windows
	// We need a new sync packet type for "ChatClear" or similar,
	// or we can just let them re-sync (but that's heavy).
	// For now, let's assume the client handles the "archive" action by clearing local UI.
	// But we should broadcast an event.
	ts.TsChatSendMessage("System", "Chat context has been archived to session: "+name)

	return sessionId, nil
}

func (ts *Teamserver) TsSessionList() []map[string]interface{} {
	return ts.DBMS.DbSessionList()
}

func (ts *Teamserver) TsSessionGetContent(sessionId string) []adaptix.ChatData {
	return ts.DBMS.DbChatGetBySession(sessionId)
}

func (ts *Teamserver) TsSessionDelete(sessionId string) error {
	return ts.DBMS.DbSessionDelete(sessionId)
}
