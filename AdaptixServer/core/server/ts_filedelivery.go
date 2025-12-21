package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

type FileDeliveryFile struct {
	ID        string    `json:"id"`
	FileName  string    `json:"file_name"`
	Sha256    string    `json:"sha256"`
	Size      int64     `json:"size"`
	Owner     string    `json:"owner"`
	CreatedAt time.Time `json:"created_at"`
	URL       string    `json:"url"`
	Downloads int       `json:"downloads"`
}

type FileDeliveryLink struct {
	Token     string    `json:"token"`
	FileID    string    `json:"file_id"`
	CreatedAt time.Time `json:"created_at"`
	ExpiresAt time.Time `json:"expires_at"`
	MaxUses   int       `json:"max_uses"`
	Uses      int       `json:"uses"`
	AllowedIP string    `json:"allowed_ip"` // optional single IP or empty
}

func (ts *Teamserver) TsFileDeliveryUpload(owner string, fileName string, fileData []byte) (interface{}, error) {
	if fileName == "" {
		return nil, errors.New("file_name is required")
	}
	if len(fileData) == 0 {
		return nil, errors.New("file is empty")
	}

	dirPath := filepath.Join(logs.RepoLogsInstance.DataPath, "filedelivery", "files")
	if err := os.MkdirAll(dirPath, 0755); err != nil {
		return nil, err
	}

	fileID := krypt.MD5(append([]byte(fmt.Sprintf("%d", time.Now().UnixNano())), fileData...))
	baseName := filepath.Base(filepath.Clean(strings.ReplaceAll(fileName, `\`, `/`)))
	storedName := fmt.Sprintf("%s_%s", fileID, baseName)
	storedPath := filepath.Join(dirPath, storedName)

	sum := sha256.Sum256(fileData)
	sha := hex.EncodeToString(sum[:])

	if err := os.WriteFile(storedPath, fileData, 0644); err != nil {
		return nil, err
	}

	fd := &FileDeliveryFile{
		ID:        fileID,
		FileName:  baseName,
		Sha256:    sha,
		Size:      int64(len(fileData)),
		Owner:     owner,
		CreatedAt: time.Now(),
	}

	if err := ts.DBMS.DbFileDeliveryFileUpsert(
		fd.ID,
		fd.FileName,
		fd.Sha256,
		fd.Size,
		fd.Owner,
		fd.CreatedAt.Unix(),
		storedPath,
	); err != nil {
		return nil, err
	}

	logs.Info("", "FileDelivery uploaded: %s (%s)", fd.FileName, fd.ID)

	// Create (or reuse) a default stable download link so UI can always show a URL.
	// 5 years default expiry, unlimited uses, no IP restriction.
	if token, url, err := ts.TsFileDeliveryCreateLink(owner, fd.ID, 24*365*5, 0, ""); err == nil {
		_ = token
		fd.URL = url
		fd.Downloads = 0
	}

	return fd, nil
}

func (ts *Teamserver) TsFileDeliveryList(owner string) (interface{}, error) {
	rows, err := ts.DBMS.DbFileDeliveryFilesByOwner(owner)
	if err != nil {
		return nil, err
	}

	now := time.Now().Unix()
	baseURL := ts.TsFileDeliveryPublicBaseURL()
	defaultMaxUses := 0
	defaultAllowedIP := ""

	files := make([]FileDeliveryFile, 0, len(rows))
	for _, r := range rows {
		url := ""
		downloads := 0
		if link, ok, err := ts.DBMS.DbFileDeliveryLinkFindActive(r.ID, defaultMaxUses, defaultAllowedIP, now); err == nil && ok {
			url = fmt.Sprintf("%s/download/%s", baseURL, link.Token)
			downloads = link.Uses
		}

		files = append(files, FileDeliveryFile{
			ID:        r.ID,
			FileName:  r.FileName,
			Sha256:    r.Sha256,
			Size:      r.Size,
			Owner:     r.Owner,
			CreatedAt: time.Unix(r.CreatedAt, 0),
			URL:       url,
			Downloads: downloads,
		})
	}
	return files, nil
}

func (ts *Teamserver) TsFileDeliveryDelete(owner string, fileID string) error {
	row, err := ts.DBMS.DbFileDeliveryFileGet(fileID)
	if err != nil {
		return err
	}
	if row.Owner != owner {
		return errors.New("permission denied")
	}

	_ = os.Remove(row.StoredPath)
	_ = ts.DBMS.DbFileDeliveryLinksDeleteByFile(fileID)
	return ts.DBMS.DbFileDeliveryFileDelete(fileID)
}

func (ts *Teamserver) TsFileDeliveryCreateLink(owner string, fileID string, expireHours int, maxUses int, allowedIP string) (string, string, error) {
	row, err := ts.DBMS.DbFileDeliveryFileGet(fileID)
	if err != nil {
		return "", "", err
	}
	if row.Owner != owner {
		return "", "", errors.New("permission denied")
	}

	if expireHours <= 0 {
		expireHours = 24
	}
	if maxUses < 0 {
		maxUses = 0
	}
	if allowedIP != "" {
		if net.ParseIP(strings.TrimSpace(allowedIP)) == nil {
			return "", "", errors.New("allowed_ip is invalid")
		}
	}

	allowedIP = strings.TrimSpace(allowedIP)
	now := time.Now().Unix()
	if existing, ok, err := ts.DBMS.DbFileDeliveryLinkFindActive(fileID, maxUses, allowedIP, now); err != nil {
		return "", "", err
	} else if ok {
		baseURL := ts.TsFileDeliveryPublicBaseURL()
		url := fmt.Sprintf("%s/download/%s", baseURL, existing.Token)
		return existing.Token, url, nil
	}

	token := ts.generateFileDeliveryToken(10)
	createdAt := time.Unix(now, 0)
	expiresAt := createdAt.Add(time.Duration(expireHours) * time.Hour)

	link := FileDeliveryLink{
		Token:     token,
		FileID:    fileID,
		CreatedAt: createdAt,
		ExpiresAt: expiresAt,
		MaxUses:   maxUses,
		Uses:      0,
		AllowedIP: allowedIP,
	}
	if err := ts.DBMS.DbFileDeliveryLinkInsert(
		link.Token,
		link.FileID,
		link.CreatedAt.Unix(),
		link.ExpiresAt.Unix(),
		link.MaxUses,
		link.Uses,
		link.AllowedIP,
	); err != nil {
		return "", "", err
	}

	baseURL := ts.TsFileDeliveryPublicBaseURL()
	url := fmt.Sprintf("%s/download/%s", baseURL, token)
	return token, url, nil
}

func (ts *Teamserver) TsFileDeliveryPublicBaseURL() string {
	host := "127.0.0.1"
	port := ts.Profile.Server.Port
	scheme := "https"

	// Pick first http/https listener and take its AgentAddr (callback_addresses)
	var picked *adaptix.ListenerData
	ts.listeners.ForEach(func(_ string, value interface{}) bool {
		ld := value.(adaptix.ListenerData)
		if ld.Protocol == "http" || ld.Protocol == "https" {
			picked = &ld
			return false
		}
		return true
	})

	if picked != nil {
		if picked.Protocol != "" {
			scheme = picked.Protocol
		}
		addr := strings.TrimSpace(picked.AgentAddr)
		if addr != "" {
			first := strings.Split(addr, ",")[0]
			first = strings.TrimSpace(first)
			if h, p, err := net.SplitHostPort(first); err == nil {
				if h != "" {
					host = h
				}
				if parsed, err := parsePort(p); err == nil {
					port = parsed
				}
			}
		}
	}

	return fmt.Sprintf("%s://%s:%d", scheme, host, port)
}

func parsePort(s string) (int, error) {
	port, err := strconv.Atoi(strings.TrimSpace(s))
	if err != nil {
		return 0, err
	}
	if port < 1 || port > 65535 {
		return 0, fmt.Errorf("invalid port")
	}
	return port, nil
}

func (ts *Teamserver) generateFileDeliveryToken(length int) string {
	b := make([]byte, length)
	if _, err := rand.Read(b); err != nil {
		return fmt.Sprintf("%x", time.Now().UnixNano())[:length]
	}
	return hex.EncodeToString(b)[:length]
}

func (ts *Teamserver) TsFileDeliveryResolveToken(token string, clientIP string) (string, string, string, error) {
	link, err := ts.DBMS.DbFileDeliveryLinkGet(token)
	if err != nil {
		return "", "", "", err
	}
	if time.Now().After(time.Unix(link.ExpiresAt, 0)) {
		_ = ts.DBMS.DbFileDeliveryDownloadAdd(token, link.FileID, clientIP, "", "expired")
		return "", "", "", errors.New("link expired")
	}
	if link.AllowedIP != "" && link.AllowedIP != clientIP {
		_ = ts.DBMS.DbFileDeliveryDownloadAdd(token, link.FileID, clientIP, "", "forbidden")
		return "", "", "", errors.New("forbidden")
	}
	if link.MaxUses > 0 && link.Uses >= link.MaxUses {
		_ = ts.DBMS.DbFileDeliveryDownloadAdd(token, link.FileID, clientIP, "", "limit")
		return "", "", "", errors.New("link usage limit")
	}

	fileRow, err := ts.DBMS.DbFileDeliveryFileGet(link.FileID)
	if err != nil {
		return "", "", "", err
	}

	if err := ts.DBMS.DbFileDeliveryLinkIncUses(token); err != nil {
		return "", "", "", err
	}
	_ = ts.DBMS.DbFileDeliveryDownloadAdd(token, link.FileID, clientIP, "", "ok")

	return fileRow.StoredPath, fileRow.FileName, fileRow.Sha256, nil
}
