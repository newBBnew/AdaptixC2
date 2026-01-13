package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbChatInsert(chatData adaptix.ChatData, sessionId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	insertQuery := `INSERT INTO Chat (Username, Message, Date, SessionId) values(?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, chatData.Username, chatData.Message, chatData.Date, sessionId)
	return err
}

func (dbms *DBMS) DbChatAll() []adaptix.ChatData {
	var messages []adaptix.ChatData

	ok := dbms.DatabaseExists()
	if ok {
		// Only select messages that belong to the current active session (where SessionId is NULL or empty)
		selectQuery := `SELECT Username, Message, Date FROM Chat WHERE SessionId IS NULL OR SessionId = '' ORDER BY Id;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				chatData := adaptix.ChatData{}
				// We don't need to scan SessionId here because we are only fetching active messages
				// and adaptix.ChatData doesn't have the field anyway.
				err = query.Scan(&chatData.Username, &chatData.Message, &chatData.Date)
				if err != nil {
					continue
				}
				messages = append(messages, chatData)
			}
		} else {
			logs.Debug("", err.Error()+" --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return messages
}

// Session Management

func (dbms *DBMS) DbSessionCreate(sessionId, name string, startTime int64) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	insertQuery := `INSERT INTO ChatSessions (SessionId, Name, StartTime, Status) values(?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, sessionId, name, startTime, "archived")
	return err
}

func (dbms *DBMS) DbSessionList() []map[string]interface{} {
	var sessions []map[string]interface{}
	ok := dbms.DatabaseExists()
	if !ok {
		return sessions
	}

	selectQuery := `SELECT SessionId, Name, StartTime, EndTime FROM ChatSessions ORDER BY StartTime DESC;`
	query, err := dbms.database.Query(selectQuery)
	if err != nil {
		return sessions
	}
	defer query.Close()

	for query.Next() {
		var sId, name string
		var start, end sql.NullInt64
		if err := query.Scan(&sId, &name, &start, &end); err == nil {
			sess := map[string]interface{}{
				"id":    sId,
				"name":  name,
				"start": 0,
				"end":   0,
			}
			if start.Valid {
				sess["start"] = start.Int64
			}
			if end.Valid {
				sess["end"] = end.Int64
			}
			sessions = append(sessions, sess)
		}
	}
	return sessions
}

func (dbms *DBMS) DbChatGetBySession(sessionId string) []adaptix.ChatData {
	var messages []adaptix.ChatData
	ok := dbms.DatabaseExists()
	if !ok {
		return messages
	}

	selectQuery := `SELECT Username, Message, Date FROM Chat WHERE SessionId = ? ORDER BY Id;`
	query, err := dbms.database.Query(selectQuery, sessionId)
	if err != nil {
		return messages
	}
	defer query.Close()

	for query.Next() {
		chatData := adaptix.ChatData{}
		if err := query.Scan(&chatData.Username, &chatData.Message, &chatData.Date); err == nil {
			// Cannot set SessionId on chatData as it doesn't exist on the struct
			messages = append(messages, chatData)
		}
	}
	return messages
}

func (dbms *DBMS) DbChatArchiveCurrent(sessionId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	// Update all messages with empty SessionId to have the new SessionId
	updateQuery := `UPDATE Chat SET SessionId = ? WHERE SessionId IS NULL OR SessionId = '';`
	_, err := dbms.database.Exec(updateQuery, sessionId)
	return err
}

func (dbms *DBMS) DbSessionDelete(sessionId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	// Delete the session record
	deleteSessionQuery := `DELETE FROM ChatSessions WHERE SessionId = ?;`
	_, err := dbms.database.Exec(deleteSessionQuery, sessionId)
	if err != nil {
		return err
	}

	// Delete associated messages
	deleteChatQuery := `DELETE FROM Chat WHERE SessionId = ?;`
	_, err = dbms.database.Exec(deleteChatQuery, sessionId)
	return err
}
