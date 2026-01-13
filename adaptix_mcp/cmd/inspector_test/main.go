package main

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
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

func main() {
	// Adjust this path to the actual Extension-Kit location
	rootPath := "/Users/blackman/netattack/c2/adaptixC2_1.0/Extension-Kit"
	filter := "cookie-monster" // Filter to focus on relevant tools

	extensions := []ExtensionInfo{}

	err := filepath.Walk(rootPath, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if !info.IsDir() && strings.HasSuffix(info.Name(), ".axs") {
			ext, err := parseAxsFile(path)
			if err == nil {
				// Apply filter
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
		fmt.Printf("Error: %v\n", err)
		return
	}

	data, _ := json.MarshalIndent(extensions, "", "  ")
	fmt.Println(string(data))
}

func parseAxsFile(path string) (ExtensionInfo, error) {
	content, err := ioutil.ReadFile(path)
	if err != nil {
		return ExtensionInfo{}, err
	}
	text := string(content)

	ext := ExtensionInfo{
		Path: path,
		Name: filepath.Base(path), // Default name
	}

	// Regex for metadata
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
