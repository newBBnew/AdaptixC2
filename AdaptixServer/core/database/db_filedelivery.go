package database

import (
	"database/sql"
	"errors"
	"time"
)

type FileDeliveryFileRow struct {
	ID         string
	FileName   string
	Sha256     string
	Size       int64
	Owner      string
	CreatedAt  int64
	StoredPath string
}

type FileDeliveryLinkRow struct {
	Token     string
	FileID    string
	CreatedAt int64
	ExpiresAt int64
	MaxUses   int
	Uses      int
	AllowedIP string
}

func (dbms *DBMS) DbFileDeliveryFileUpsert(fileID string, fileName string, sha256 string, size int64, owner string, createdAt int64, storedPath string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	query := `INSERT OR REPLACE INTO FileDeliveryFiles (ID, FileName, Sha256, Size, Owner, CreatedAt, StoredPath) VALUES(?,?,?,?,?,?,?);`
	_, err := dbms.database.Exec(query, fileID, fileName, sha256, size, owner, createdAt, storedPath)
	return err
}

func (dbms *DBMS) DbFileDeliveryFileGet(fileID string) (FileDeliveryFileRow, error) {
	row := FileDeliveryFileRow{}
	ok := dbms.DatabaseExists()
	if !ok {
		return row, errors.New("database does not exist")
	}
	q := `SELECT ID, FileName, Sha256, Size, Owner, CreatedAt, StoredPath FROM FileDeliveryFiles WHERE ID = ?;`
	r := dbms.database.QueryRow(q, fileID)
	err := r.Scan(&row.ID, &row.FileName, &row.Sha256, &row.Size, &row.Owner, &row.CreatedAt, &row.StoredPath)
	if err != nil {
		return row, err
	}
	return row, nil
}

func (dbms *DBMS) DbFileDeliveryLinkFindActive(fileID string, maxUses int, allowedIP string, nowUnix int64) (FileDeliveryLinkRow, bool, error) {
	row := FileDeliveryLinkRow{}
	ok := dbms.DatabaseExists()
	if !ok {
		return row, false, errors.New("database does not exist")
	}

	q := `SELECT Token, FileId, CreatedAt, ExpiresAt, MaxUses, Uses, AllowedIP
		FROM FileDeliveryLinks
		WHERE FileId = ? AND MaxUses = ? AND IFNULL(AllowedIP, '') = IFNULL(?, '')
		AND ExpiresAt > ?
		AND (MaxUses = 0 OR Uses < MaxUses)
		ORDER BY CreatedAt DESC
		LIMIT 1;`

	r := dbms.database.QueryRow(q, fileID, maxUses, allowedIP, nowUnix)
	err := r.Scan(&row.Token, &row.FileID, &row.CreatedAt, &row.ExpiresAt, &row.MaxUses, &row.Uses, &row.AllowedIP)
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return row, false, nil
		}
		return row, false, err
	}
	return row, true, nil
}

func (dbms *DBMS) DbFileDeliveryFileDelete(fileID string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`DELETE FROM FileDeliveryFiles WHERE ID = ?;`, fileID)
	return err
}

func (dbms *DBMS) DbFileDeliveryFilesByOwner(owner string) ([]FileDeliveryFileRow, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	rows, err := dbms.database.Query(`SELECT ID, FileName, Sha256, Size, Owner, CreatedAt, StoredPath FROM FileDeliveryFiles WHERE Owner = ? ORDER BY CreatedAt DESC;`, owner)
	if err != nil {
		return nil, err
	}
	defer func(rows *sql.Rows) { _ = rows.Close() }(rows)

	out := make([]FileDeliveryFileRow, 0)
	for rows.Next() {
		row := FileDeliveryFileRow{}
		if err := rows.Scan(&row.ID, &row.FileName, &row.Sha256, &row.Size, &row.Owner, &row.CreatedAt, &row.StoredPath); err != nil {
			continue
		}
		out = append(out, row)
	}
	return out, nil
}

func (dbms *DBMS) DbFileDeliveryFilesAll() ([]FileDeliveryFileRow, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	rows, err := dbms.database.Query(`SELECT ID, FileName, Sha256, Size, Owner, CreatedAt, StoredPath FROM FileDeliveryFiles ORDER BY CreatedAt DESC;`)
	if err != nil {
		return nil, err
	}
	defer func(rows *sql.Rows) { _ = rows.Close() }(rows)

	out := make([]FileDeliveryFileRow, 0)
	for rows.Next() {
		row := FileDeliveryFileRow{}
		if err := rows.Scan(&row.ID, &row.FileName, &row.Sha256, &row.Size, &row.Owner, &row.CreatedAt, &row.StoredPath); err != nil {
			continue
		}
		out = append(out, row)
	}
	return out, nil
}

func (dbms *DBMS) DbFileDeliveryLinkInsert(token string, fileID string, createdAt int64, expiresAt int64, maxUses int, uses int, allowedIP string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}
	query := `INSERT INTO FileDeliveryLinks (Token, FileId, CreatedAt, ExpiresAt, MaxUses, Uses, AllowedIP) VALUES(?,?,?,?,?,?,?);`
	_, err := dbms.database.Exec(query, token, fileID, createdAt, expiresAt, maxUses, uses, allowedIP)
	return err
}

func (dbms *DBMS) DbFileDeliveryLinkGet(token string) (FileDeliveryLinkRow, error) {
	row := FileDeliveryLinkRow{}
	ok := dbms.DatabaseExists()
	if !ok {
		return row, errors.New("database does not exist")
	}
	q := `SELECT Token, FileId, CreatedAt, ExpiresAt, MaxUses, Uses, AllowedIP FROM FileDeliveryLinks WHERE Token = ?;`
	r := dbms.database.QueryRow(q, token)
	err := r.Scan(&row.Token, &row.FileID, &row.CreatedAt, &row.ExpiresAt, &row.MaxUses, &row.Uses, &row.AllowedIP)
	if err != nil {
		return row, err
	}
	return row, nil
}

func (dbms *DBMS) DbFileDeliveryLinkIncUses(token string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`UPDATE FileDeliveryLinks SET Uses = Uses + 1 WHERE Token = ?;`, token)
	return err
}

func (dbms *DBMS) DbFileDeliveryLinksDeleteByFile(fileID string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`DELETE FROM FileDeliveryLinks WHERE FileId = ?;`, fileID)
	return err
}

func (dbms *DBMS) DbFileDeliveryDownloadAdd(token string, fileID string, ip string, ua string, result string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`INSERT INTO FileDeliveryDownloads (Token, FileId, Ts, IP, UserAgent, Result) VALUES(?,?,?,?,?,?);`, token, fileID, time.Now().Unix(), ip, ua, result)
	return err
}
