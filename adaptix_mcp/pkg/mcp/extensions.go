package mcp

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

// ExtensionInfo represents a parsed C2 extension
type ExtensionInfo struct {
	Name        string        `json:"name"`
	Description string        `json:"description"`
	Path        string        `json:"path"`
	Commands    []CommandInfo `json:"commands"`
}

// CommandInfo represents a command within an extension
type CommandInfo struct {
	Name        string `json:"name"`
	Description string `json:"description"`
	Usage       string `json:"usage"`
}

// handleInspectExtensions scans the Extension-Kit directory and parses .axs files
func (s *MCPServer) handleInspectExtensions(params map[string]interface{}) (interface{}, error) {
	// Root path to search. Defaults to relative path from workspace root if not provided
	rootPath, _ := params["root_path"].(string)
	if rootPath == "" {
		// Try to find Extension-Kit relative to common workspace locations
		// First try current working directory
		cwd, err := os.Getwd()
		if err == nil {
			// Check if we're in the workspace root or a subdirectory
			possiblePaths := []string{
				filepath.Join(cwd, "Extension-Kit"),
				filepath.Join(cwd, "..", "Extension-Kit"),
				filepath.Join(cwd, "../..", "Extension-Kit"),
			}
			for _, path := range possiblePaths {
				if info, err := os.Stat(path); err == nil && info.IsDir() {
					rootPath = path
					break
				}
			}
		}
		// If still not found, try environment variable
		if rootPath == "" {
			if envPath := os.Getenv("ADAPTIX_EXTENSION_KIT_PATH"); envPath != "" {
				rootPath = envPath
			}
		}
		// Last resort: return error if not found
		if rootPath == "" {
			return nil, fmt.Errorf("Extension-Kit path not found. Please provide root_path parameter or set ADAPTIX_EXTENSION_KIT_PATH environment variable")
		}
	}

	filter, _ := params["filter"].(string)
	filter = strings.ToLower(filter)

	extensions := []ExtensionInfo{}

	err := filepath.Walk(rootPath, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if !info.IsDir() && strings.HasSuffix(info.Name(), ".axs") {
			ext, err := parseAxsFile(path)
			if err == nil {
				// Apply filter if present
				if filter != "" {
					matched := strings.Contains(strings.ToLower(ext.Name), filter) ||
						strings.Contains(strings.ToLower(ext.Description), filter)
					for _, cmd := range ext.Commands {
						if strings.Contains(strings.ToLower(cmd.Name), filter) ||
							strings.Contains(strings.ToLower(cmd.Description), filter) {
							matched = true
							break
						}
					}
					if !matched {
						return nil
					}
				}
				extensions = append(extensions, ext)
			}
		}
		return nil
	})

	if err != nil {
		return nil, fmt.Errorf("failed to scan extensions: %v", err)
	}

	return map[string]interface{}{
		"extensions": extensions,
		"count":      len(extensions),
		"root_path":  rootPath,
	}, nil
}

func parseAxsFile(path string) (ExtensionInfo, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return ExtensionInfo{}, err
	}
	text := string(content)

	ext := ExtensionInfo{
		Path: path,
		Name: filepath.Base(path), // Default name
	}

	// Regex for metadata
	// var metadata = { name: "...", description: "..." };
	metaNameRe := regexp.MustCompile(`name:\s*"([^"]+)"`)
	metaDescRe := regexp.MustCompile(`description:\s*"([^"]+)"`)

	if match := metaNameRe.FindStringSubmatch(text); len(match) > 1 {
		ext.Name = match[1]
	}
	if match := metaDescRe.FindStringSubmatch(text); len(match) > 1 {
		ext.Description = match[1]
	}

	// Regex for commands
	// ax.create_command("name", "desc", "usage")
	// Note: usage is optional
	cmdRe := regexp.MustCompile(`ax\.create_command\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"(?:\s*,\s*"([^"]*)")?`)

	matches := cmdRe.FindAllStringSubmatch(text, -1)
	for _, match := range matches {
		cmd := CommandInfo{
			Name:        match[1],
			Description: match[2],
		}
		if len(match) > 3 {
			cmd.Usage = match[3]
		}
		ext.Commands = append(ext.Commands, cmd)
	}

	return ext, nil
}
