package playbook

import (
	"embed"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
)

//go:embed defaults/**
var defaultsFS embed.FS

const (
	workspaceDirName = ".adaptix/playbooks"
)

func WorkspaceDir() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(home, workspaceDirName), nil
}
func EnsureWorkspace() error {
	root, err := WorkspaceDir()
	if err != nil {
		return err
	}

	dirs := []string{
		root,
		filepath.Join(root, "catalog"),
		filepath.Join(root, "playbooks"),
		filepath.Join(root, "packs"),
		filepath.Join(root, "runs"),
	}
	for _, d := range dirs {
		if err := os.MkdirAll(d, 0o755); err != nil {
			return fmt.Errorf("mkdir %s: %w", d, err)
		}
	}

	sub, err := fs.Sub(defaultsFS, "defaults")
	if err != nil {
		return fmt.Errorf("sub defaultsFS: %w", err)
	}

	return fs.WalkDir(sub, ".", func(path string, d fs.DirEntry, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		dstPath := filepath.Join(root, path)
		if d.IsDir() {
			return os.MkdirAll(dstPath, 0o755)
		}

		if _, err := os.Stat(dstPath); err == nil {
			return nil
		}

		src, err := sub.Open(path)
		if err != nil {
			return err
		}
		defer src.Close()

		if err := os.MkdirAll(filepath.Dir(dstPath), 0o755); err != nil {
			return err
		}
		out, err := os.OpenFile(dstPath, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0o644)
		if err != nil {
			return err
		}
		defer out.Close()

		if _, err := io.Copy(out, src); err != nil {
			return err
		}
		return nil
	})
}
