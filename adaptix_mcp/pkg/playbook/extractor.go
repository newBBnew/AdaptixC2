package playbook

import (
	"encoding/json"
	"fmt"
	"regexp"
	"strings"
)

const (
	// 限制常量
	MaxOutputSize        = 256 * 1024 // 256KB
	MaxExtractedKeyLen   = 8 * 1024   // 8KB
	MaxExtractorsPerStep = 20
)

// Extractor 定义提取器
type Extractor struct {
	ID       string                 `yaml:"id" json:"id"`
	Key      string                 `yaml:"key" json:"key"`
	From     string                 `yaml:"from" json:"from"` // "output" 或字段路径
	Type     string                 `yaml:"type" json:"type"` // regex, json_path, split_lines, kv_pairs
	Options  map[string]interface{} `yaml:"options" json:"options"`
	Required bool                   `yaml:"required" json:"required"`
}

// RunExtractors 运行提取器列表，返回提取结果
func RunExtractors(output string, extractors []Extractor) (map[string]interface{}, error) {
	if len(extractors) > MaxExtractorsPerStep {
		return nil, fmt.Errorf("too many extractors: %d (max %d)", len(extractors), MaxExtractorsPerStep)
	}

	// 截断过长的输出
	if len(output) > MaxOutputSize {
		output = output[:MaxOutputSize]
	}

	result := make(map[string]interface{})

	for _, ext := range extractors {
		key := ext.Key
		if key == "" {
			key = ext.ID
		}

		val, err := runExtractor(output, ext)
		if err != nil {
			if ext.Required {
				return nil, fmt.Errorf("extractor %s failed: %w", ext.ID, err)
			}
			continue
		}

		// 截断过长的值
		if strVal, ok := val.(string); ok && len(strVal) > MaxExtractedKeyLen {
			val = strVal[:MaxExtractedKeyLen]
		}

		result[key] = val
	}

	return result, nil
}

// runExtractor 运行单个提取器
func runExtractor(output string, ext Extractor) (interface{}, error) {
	// 获取源数据
	source := output
	if ext.From != "" && ext.From != "output" {
		// TODO: 支持从 JSON 结构中获取特定字段
		source = output
	}

	switch ext.Type {
	case "regex":
		return extractRegex(source, ext.Options)
	case "json_path":
		return extractJSONPath(source, ext.Options)
	case "split_lines":
		return extractSplitLines(source, ext.Options)
	case "kv_pairs":
		return extractKVPairs(source, ext.Options)
	default:
		return nil, fmt.Errorf("unknown extractor type: %s", ext.Type)
	}
}

// extractRegex 正则表达式提取
func extractRegex(source string, options map[string]interface{}) (interface{}, error) {
	pattern, ok := options["pattern"].(string)
	if !ok || pattern == "" {
		return nil, fmt.Errorf("regex extractor requires 'pattern' option")
	}

	// 编译正则
	re, err := regexp.Compile(pattern)
	if err != nil {
		return nil, fmt.Errorf("invalid regex pattern: %w", err)
	}

	// 获取组号
	group := 0
	if g, ok := options["group"].(int); ok {
		group = g
	} else if g, ok := options["group"].(float64); ok {
		group = int(g)
	}

	// 检查是否需要匹配所有
	matchAll := false
	if all, ok := options["all"].(bool); ok {
		matchAll = all
	}

	if matchAll {
		// 返回所有匹配
		matches := re.FindAllStringSubmatch(source, -1)
		results := make([]string, 0, len(matches))
		for _, match := range matches {
			if len(match) > group {
				results = append(results, match[group])
			}
		}
		return results, nil
	}

	// 返回第一个匹配
	matches := re.FindStringSubmatch(source)
	if len(matches) <= group {
		return "", nil
	}
	return matches[group], nil
}

// extractJSONPath 从 JSON 中提取（简化版，仅支持简单路径）
func extractJSONPath(source string, options map[string]interface{}) (interface{}, error) {
	path, ok := options["path"].(string)
	if !ok || path == "" {
		return nil, fmt.Errorf("json_path extractor requires 'path' option")
	}

	// 尝试解析 JSON
	var data interface{}
	if err := json.Unmarshal([]byte(source), &data); err != nil {
		return nil, fmt.Errorf("failed to parse JSON: %w", err)
	}

	// 简单路径解析 (e.g., "user.name" or "items[0].id")
	parts := strings.Split(path, ".")
	current := data

	for _, part := range parts {
		if current == nil {
			return nil, nil
		}

		// 检查数组索引 (e.g., "items[0]")
		if idx := strings.Index(part, "["); idx >= 0 {
			key := part[:idx]
			indexStr := strings.TrimSuffix(part[idx+1:], "]")
			var index int
			fmt.Sscanf(indexStr, "%d", &index)

			if key != "" {
				obj, ok := current.(map[string]interface{})
				if !ok {
					return nil, nil
				}
				current = obj[key]
			}

			arr, ok := current.([]interface{})
			if !ok || index >= len(arr) {
				return nil, nil
			}
			current = arr[index]
		} else {
			obj, ok := current.(map[string]interface{})
			if !ok {
				return nil, nil
			}
			current = obj[part]
		}
	}

	return current, nil
}

// extractSplitLines 按行分割
func extractSplitLines(source string, options map[string]interface{}) (interface{}, error) {
	lines := strings.Split(source, "\n")

	// 过滤空行（可选）
	trimEmpty := true
	if te, ok := options["trim_empty"].(bool); ok {
		trimEmpty = te
	}

	if trimEmpty {
		filtered := make([]string, 0, len(lines))
		for _, line := range lines {
			trimmed := strings.TrimSpace(line)
			if trimmed != "" {
				filtered = append(filtered, trimmed)
			}
		}
		return filtered, nil
	}

	return lines, nil
}

// extractKVPairs 提取键值对
func extractKVPairs(source string, options map[string]interface{}) (interface{}, error) {
	// 分隔符
	separator := "="
	if sep, ok := options["separator"].(string); ok {
		separator = sep
	}

	// 行分隔符
	lineSep := "\n"
	if ls, ok := options["line_separator"].(string); ok {
		lineSep = ls
	}

	result := make(map[string]string)
	lines := strings.Split(source, lineSep)

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// 跳过注释行（可选）
		if skipComments, ok := options["skip_comments"].(bool); ok && skipComments {
			if strings.HasPrefix(line, "#") || strings.HasPrefix(line, "//") {
				continue
			}
		}

		idx := strings.Index(line, separator)
		if idx < 0 {
			continue
		}

		key := strings.TrimSpace(line[:idx])
		value := strings.TrimSpace(line[idx+len(separator):])

		if key != "" {
			result[key] = value
		}
	}

	return result, nil
}

// MergeExtractors 合并模板级和步骤级提取器
// 步骤级提取器覆盖模板级同 ID 的提取器
func MergeExtractors(templateExtractors, stepExtractors []Extractor) []Extractor {
	// 构建 ID 到提取器的映射
	merged := make(map[string]Extractor)

	for _, ext := range templateExtractors {
		merged[ext.ID] = ext
	}

	for _, ext := range stepExtractors {
		merged[ext.ID] = ext // 步骤级覆盖模板级
	}

	// 转换回切片
	result := make([]Extractor, 0, len(merged))
	for _, ext := range merged {
		result = append(result, ext)
	}

	return result
}
