package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/yaml.v3"
)

type TacticalYamlVariant struct {
	Id         string `yaml:"id"`
	Name       string `yaml:"name"`
	Cmd        string `yaml:"cmd"`
	Os         string `yaml:"os"`
	Risk       int    `yaml:"risk"`
	Opsec      string `yaml:"opsec"`
	AiGuidance string `yaml:"ai_guidance"`
}

type TacticalYamlBlock struct {
	Id          string                `yaml:"id"`
	Name        string                `yaml:"name"`
	Description string                `yaml:"description"`
	Variants    []TacticalYamlVariant `yaml:"variants"`
}

type TacticalYamlCategory struct {
	Name   string              `yaml:"category"`
	Blocks []TacticalYamlBlock `yaml:"blocks"`
}

func (ts *Teamserver) LoadTacticalLibrary() error {
	tacticalDir := filepath.Join("data", "tactical")
	if _, err := os.Stat(tacticalDir); os.IsNotExist(err) {
		err := os.MkdirAll(tacticalDir, 0755)
		if err != nil {
			return err
		}
		// Create a sample tactical file if none exists
		ts.createSampleTacticalFile(tacticalDir)
	}

	files, err := filepath.Glob(filepath.Join(tacticalDir, "*.yaml"))
	if err != nil {
		return err
	}

	ts.TacticalCatalog = safe.NewMap()

	for _, file := range files {
		data, err := os.ReadFile(file)
		if err != nil {
			logs.Error("TACTICAL", "Failed to read tactical file %s: %s", file, err.Error())
			continue
		}

		var category TacticalYamlCategory
		err = yaml.Unmarshal(data, &category)
		if err != nil {
			logs.Error("TACTICAL", "Failed to parse tactical file %s: %s", file, err.Error())
			continue
		}

		var blocks []connector.TacticalBlock
		for _, b := range category.Blocks {
			var variants []connector.TacticalVariant
			for _, v := range b.Variants {
				osInt := 0
				switch v.Os {
				case "windows":
					osInt = 1
				case "linux":
					osInt = 2
				case "mac":
					osInt = 3
				}

				variants = append(variants, connector.TacticalVariant{
					Id:              v.Id,
					Name:            v.Name,
					Cmd:             v.Cmd,
					Os:              osInt,
					Risk:            v.Risk,
					Opsec:           v.Opsec,
					AiGuidance:      v.AiGuidance,
					CommandTemplate: v.Cmd,
				})
			}
			blocks = append(blocks, connector.TacticalBlock{
				Id:          b.Id,
				Name:        b.Name,
				Description: b.Description,
				Variants:    variants,
			})
		}

		ts.TacticalCatalog.Put(category.Name, blocks)
		logs.Success("TACTICAL", "Loaded category '%s' with %d blocks from %s", category.Name, len(blocks), filepath.Base(file))
	}

	return nil
}

func (ts *Teamserver) createSampleTacticalFile(dir string) {
	sample := TacticalYamlCategory{
		Name: "Reconnaissance",
		Blocks: []TacticalYamlBlock{
			{
				Id:          "win_system_info",
				Name:        "System Information",
				Description: "Basic system and environment reconnaissance",
				Variants: []TacticalYamlVariant{
					{
						Id:         "sysinfo_all",
						Name:       "Full System Info",
						Cmd:        "shell systeminfo",
						Os:         "windows",
						Risk:       1,
						Opsec:      "Medium - Spawns shell and systeminfo.exe",
						AiGuidance: "Run this first to understand the target OS and hotfixes.",
					},
					{
						Id:         "whoami",
						Name:       "Who Am I",
						Cmd:        "whoami /all",
						Os:         "windows",
						Risk:       1,
						Opsec:      "Low",
						AiGuidance: "Check current user privileges and group memberships.",
					},
				},
			},
		},
	}

	data, _ := yaml.Marshal(sample)
	os.WriteFile(filepath.Join(dir, "recon.yaml"), data, 0644)
}

func (ts *Teamserver) TsPresyncTacticalCatalog() []interface{} {
	var categories []connector.TacticalCategory
	ts.TacticalCatalog.ForEach(func(key string, value interface{}) bool {
		blocks := value.([]connector.TacticalBlock)
		categories = append(categories, connector.TacticalCategory{
			Name:   key,
			Blocks: blocks,
		})
		return true
	})

	if len(categories) == 0 {
		return nil
	}

	packet := CreateSpTacticalCatalogSync(categories)
	return []interface{}{packet}
}

func (ts *Teamserver) TsPresyncTacticalWorkflow() []interface{} {
	ts.WorkflowMutex.Lock()
	defer ts.WorkflowMutex.Unlock()

	if len(ts.CurrentWorkflow) == 0 && ts.WorkflowTargetAgents == "" {
		return nil
	}

	packet := CreateSpTacticalWorkflowSync("update", ts.CurrentWorkflow, ts.WorkflowTargetAgents)
	return []interface{}{packet}
}

func (ts *Teamserver) TsTacticalWorkflowUpdate(steps []connector.TacticalWorkflowStep, targets string) {
	ts.WorkflowMutex.Lock()
	ts.CurrentWorkflow = steps
	ts.WorkflowTargetAgents = targets
	ts.WorkflowMutex.Unlock()

	packet := CreateSpTacticalWorkflowSync("update", steps, targets)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsTacticalWorkflowClear() {
	ts.WorkflowMutex.Lock()
	ts.CurrentWorkflow = nil
	ts.WorkflowTargetAgents = ""
	ts.WorkflowMutex.Unlock()

	packet := CreateSpTacticalWorkflowSync("clear", nil, "")
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsTacticalAiSuggestion(content string) {
	packet := CreateSpTacticalAiSuggestion(content)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsTacticalLibraryUpdate(category string, block connector.TacticalBlock) error {
	// 1. Update in-memory catalog
	var blocks []connector.TacticalBlock
	if val, ok := ts.TacticalCatalog.Get(category); ok {
		blocks = val.([]connector.TacticalBlock)
	}

	found := false
	for i, b := range blocks {
		if b.Id == block.Id {
			blocks[i] = block
			found = true
			break
		}
	}
	if !found {
		blocks = append(blocks, block)
	}
	ts.TacticalCatalog.Put(category, blocks)

	// 2. Persist to YAML
	ts.SaveTacticalLibrary()

	// 3. Notify all clients
	packet := CreateSpTacticalCatalogSync([]connector.TacticalCategory{
		{Name: category, Blocks: blocks},
	})
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsTacticalLibraryDelete(blockId string) error {
	var targetCategory string
	var updatedBlocks []connector.TacticalBlock

	ts.TacticalCatalog.ForEach(func(key string, value interface{}) bool {
		blocks := value.([]connector.TacticalBlock)
		for i, b := range blocks {
			if b.Id == blockId {
				targetCategory = key
				updatedBlocks = append(blocks[:i], blocks[i+1:]...)
				return false
			}
		}
		return true
	})

	if targetCategory != "" {
		if len(updatedBlocks) == 0 {
			ts.TacticalCatalog.Delete(targetCategory)
		} else {
			ts.TacticalCatalog.Put(targetCategory, updatedBlocks)
		}

		// Persist to YAML
		ts.SaveTacticalLibrary()

		// Notify clients
		// For delete, we might need a specific action in 0xA1 or a full sync
		// For now, trigger a full re-sync
		ts.TsSyncAllClients(ts.TsPresyncTacticalCatalog()[0])
	}

	return nil
}

func (ts *Teamserver) SaveTacticalLibrary() error {
	tacticalDir := filepath.Join("data", "tactical")

	// Create map category -> YAML structure
	ts.TacticalCatalog.ForEach(func(key string, value interface{}) bool {
		blocks := value.([]connector.TacticalBlock)

		yamlCategory := TacticalYamlCategory{
			Name: key,
		}

		for _, b := range blocks {
			var variants []TacticalYamlVariant
			for _, v := range b.Variants {
				osStr := "any"
				switch v.Os {
				case 1:
					osStr = "windows"
				case 2:
					osStr = "linux"
				case 3:
					osStr = "mac"
				}

				variants = append(variants, TacticalYamlVariant{
					Id:         v.Id,
					Name:       v.Name,
					Cmd:        v.Cmd,
					Os:         osStr,
					Risk:       v.Risk,
					Opsec:      v.Opsec,
					AiGuidance: v.AiGuidance,
				})
			}

			yamlCategory.Blocks = append(yamlCategory.Blocks, TacticalYamlBlock{
				Id:          b.Id,
				Name:        b.Name,
				Description: b.Description,
				Variants:    variants,
			})
		}

		data, err := yaml.Marshal(yamlCategory)
		if err != nil {
			logs.Error("TACTICAL", "Failed to marshal category %s: %s", key, err.Error())
			return true
		}

		fileName := fmt.Sprintf("%s.yaml", key) // Simple naming, might need sanitization
		filePath := filepath.Join(tacticalDir, fileName)

		err = os.WriteFile(filePath, data, 0644)
		if err != nil {
			logs.Error("TACTICAL", "Failed to write tactical file %s: %s", filePath, err.Error())
		}

		return true
	})

	return nil
}
