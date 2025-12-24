package playbook

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"gopkg.in/yaml.v3"
)

type PlaybookMetadata struct {
	ID          string            `yaml:"id" json:"id"`
	Name        string            `yaml:"name" json:"name"`
	Version     interface{}       `yaml:"version" json:"version"`
	Description string            `yaml:"description" json:"description"`
	Labels      map[string]string `yaml:"labels" json:"labels"`
	Annotations map[string]string `yaml:"annotations" json:"annotations"`
}

type PlaybookDoc struct {
	APIVersion string                 `yaml:"apiVersion" json:"apiVersion"`
	Kind       string                 `yaml:"kind" json:"kind"`
	Metadata   PlaybookMetadata       `yaml:"metadata" json:"metadata"`
	Spec       map[string]interface{} `yaml:"spec" json:"spec"`
}

type PlaybookEntry struct {
	ID      string `json:"id"`
	Name    string `json:"name"`
	Version string `json:"version"`
	Path    string `json:"path"`
}

type RunRecord struct {
	RunID      string                 `json:"run_id"`
	PlaybookID string                 `json:"playbook_id"`
	CreatedAt  string                 `json:"created_at"`
	Inputs     map[string]interface{} `json:"inputs"`
	Plan       map[string]interface{} `json:"plan"`
	Status     string                 `json:"status"`
}

func PlaybooksDir() (string, error) {
	root, err := WorkspaceDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(root, "playbooks"), nil
}

func RunsDirForDate(t time.Time) (string, error) {
	root, err := WorkspaceDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(root, "runs", t.Format("2006-01-02")), nil
}

func ListPlaybooks() ([]PlaybookEntry, error) {
	dir, err := PlaybooksDir()
	if err != nil {
		return nil, err
	}

	entries := make([]PlaybookEntry, 0)
	_ = filepath.WalkDir(dir, func(path string, d os.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if d.IsDir() {
			return nil
		}
		lower := strings.ToLower(d.Name())
		if !(strings.HasSuffix(lower, ".yaml") || strings.HasSuffix(lower, ".yml")) {
			return nil
		}

		data, err := os.ReadFile(path)
		if err != nil {
			return nil
		}
		var doc PlaybookDoc
		if err := yaml.Unmarshal(data, &doc); err != nil {
			return nil
		}
		if doc.Metadata.ID == "" {
			return nil
		}

		ver := ""
		switch v := doc.Metadata.Version.(type) {
		case string:
			ver = v
		case int:
			ver = fmt.Sprintf("%d", v)
		case int64:
			ver = fmt.Sprintf("%d", v)
		case float64:
			ver = fmt.Sprintf("%v", v)
		default:
			if doc.Metadata.Version != nil {
				ver = fmt.Sprintf("%v", doc.Metadata.Version)
			}
		}

		entries = append(entries, PlaybookEntry{
			ID:      doc.Metadata.ID,
			Name:    doc.Metadata.Name,
			Version: ver,
			Path:    path,
		})
		return nil
	})

	sort.Slice(entries, func(i, j int) bool {
		return entries[i].ID < entries[j].ID
	})

	return entries, nil
}

func LoadPlaybookByID(playbookID string) (*PlaybookDoc, string, error) {
	list, err := ListPlaybooks()
	if err != nil {
		return nil, "", err
	}
	for _, e := range list {
		if e.ID == playbookID {
			data, err := os.ReadFile(e.Path)
			if err != nil {
				return nil, "", err
			}
			var doc PlaybookDoc
			if err := yaml.Unmarshal(data, &doc); err != nil {
				return nil, "", err
			}
			return &doc, e.Path, nil
		}
	}
	return nil, "", fmt.Errorf("playbook not found: %s", playbookID)
}

func CreateRun(playbookID string, inputs map[string]interface{}, plan map[string]interface{}) (*RunRecord, string, error) {
	now := time.Now()
	runsDir, err := RunsDirForDate(now)
	if err != nil {
		return nil, "", err
	}
	if err := os.MkdirAll(runsDir, 0o755); err != nil {
		return nil, "", err
	}

	safeID := strings.NewReplacer("/", "_", "\\", "_", ":", "_").Replace(playbookID)
	runID := fmt.Sprintf("run_%s_%s", now.Format("150405"), safeID)
	path := filepath.Join(runsDir, runID+".json")

	rec := &RunRecord{
		RunID:      runID,
		PlaybookID: playbookID,
		CreatedAt:  now.Format(time.RFC3339Nano),
		Inputs:     inputs,
		Plan:       plan,
		Status:     "planned",
	}

	b, err := json.MarshalIndent(rec, "", "  ")
	if err != nil {
		return nil, "", err
	}
	if err := os.WriteFile(path, b, 0o644); err != nil {
		return nil, "", err
	}
	return rec, path, nil
}

func ReadRun(runPath string) (*RunRecord, error) {
	b, err := os.ReadFile(runPath)
	if err != nil {
		return nil, err
	}
	var rec RunRecord
	if err := json.Unmarshal(b, &rec); err != nil {
		return nil, err
	}
	return &rec, nil
}
