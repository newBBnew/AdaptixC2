package playbook

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"
)

// ExprContext 表达式求值上下文
type ExprContext struct {
	Inputs   map[string]interface{}            // 用户输入 inputs.*
	Vars     map[string]interface{}            // 变量 vars.*
	Steps    map[string]*StepResult            // 步骤结果 steps.<id>.result.* / steps.<id>.extracted.*
	Metadata map[string]interface{}            // playbook 元数据 metadata.*
}

// StepResult 步骤结果
type StepResult struct {
	StepID    string
	Status    string // "pending", "running", "completed", "failed", "skipped"
	Result    map[string]interface{}
	Extracted map[string]interface{}
	Error     string
	StartTime time.Time
	EndTime   time.Time
}

// NewExprContext 创建新的表达式上下文
func NewExprContext() *ExprContext {
	return &ExprContext{
		Inputs:   make(map[string]interface{}),
		Vars:     make(map[string]interface{}),
		Steps:    make(map[string]*StepResult),
		Metadata: make(map[string]interface{}),
	}
}

// exprPattern 匹配 ${{ ... }} 表达式
var exprPattern = regexp.MustCompile(`\$\{\{\s*(.+?)\s*\}\}`)

// ExpandString 展开字符串中的所有表达式
// 例如: "Hello ${{ inputs.name }}" -> "Hello World"
func ExpandString(s string, ctx *ExprContext) (string, error) {
	var lastErr error
	result := exprPattern.ReplaceAllStringFunc(s, func(match string) string {
		// 提取表达式内容
		submatch := exprPattern.FindStringSubmatch(match)
		if len(submatch) < 2 {
			return match
		}
		expr := submatch[1]

		// 求值
		val, err := EvaluateExpr(expr, ctx)
		if err != nil {
			lastErr = err
			return match
		}

		// 转换为字符串
		return toString(val)
	})

	return result, lastErr
}

// EvaluateExpr 求值单个表达式
// 支持: inputs.*, vars.*, steps.<id>.result.*, steps.<id>.extracted.*, metadata.*
// 支持函数调用: len(), join(), split(), default(), lower(), upper(), trim()
func EvaluateExpr(expr string, ctx *ExprContext) (interface{}, error) {
	expr = strings.TrimSpace(expr)

	// 检查是否是函数调用
	if idx := strings.Index(expr, "("); idx > 0 && strings.HasSuffix(expr, ")") {
		funcName := expr[:idx]
		argsStr := expr[idx+1 : len(expr)-1]
		return evaluateFunction(funcName, argsStr, ctx)
	}

	// 否则作为路径求值
	return evaluatePath(expr, ctx)
}

// evaluatePath 求值路径表达式
func evaluatePath(path string, ctx *ExprContext) (interface{}, error) {
	parts := strings.Split(path, ".")
	if len(parts) == 0 {
		return nil, fmt.Errorf("empty path")
	}

	var root interface{}
	var remaining []string

	switch parts[0] {
	case "inputs":
		root = ctx.Inputs
		remaining = parts[1:]
	case "vars":
		root = ctx.Vars
		remaining = parts[1:]
	case "metadata":
		root = ctx.Metadata
		remaining = parts[1:]
	case "steps":
		if len(parts) < 2 {
			return nil, fmt.Errorf("invalid steps path: %s", path)
		}
		stepID := parts[1]
		stepResult, ok := ctx.Steps[stepID]
		if !ok {
			return nil, fmt.Errorf("step not found: %s", stepID)
		}
		if len(parts) < 3 {
			return stepResult, nil
		}
		switch parts[2] {
		case "result":
			root = stepResult.Result
			remaining = parts[3:]
		case "extracted":
			root = stepResult.Extracted
			remaining = parts[3:]
		case "status":
			return stepResult.Status, nil
		case "error":
			return stepResult.Error, nil
		default:
			return nil, fmt.Errorf("invalid step property: %s", parts[2])
		}
	default:
		// 尝试作为字面量解析
		return parseLiteral(path)
	}

	// 递归获取嵌套值
	return getNestedValue(root, remaining)
}

// getNestedValue 从嵌套结构中获取值
func getNestedValue(obj interface{}, path []string) (interface{}, error) {
	if len(path) == 0 {
		return obj, nil
	}

	if obj == nil {
		return nil, fmt.Errorf("cannot access property '%s' of nil", path[0])
	}

	switch v := obj.(type) {
	case map[string]interface{}:
		next, ok := v[path[0]]
		if !ok {
			return nil, nil // 返回 nil 而不是错误，允许使用 default() 函数
		}
		return getNestedValue(next, path[1:])
	case map[interface{}]interface{}:
		next, ok := v[path[0]]
		if !ok {
			return nil, nil
		}
		return getNestedValue(next, path[1:])
	default:
		return nil, fmt.Errorf("cannot access property '%s' of %T", path[0], obj)
	}
}

// parseLiteral 解析字面量值
func parseLiteral(s string) (interface{}, error) {
	s = strings.TrimSpace(s)

	// 布尔值
	if s == "true" {
		return true, nil
	}
	if s == "false" {
		return false, nil
	}

	// null/nil
	if s == "null" || s == "nil" {
		return nil, nil
	}

	// 字符串（带引号）
	if (strings.HasPrefix(s, "\"") && strings.HasSuffix(s, "\"")) ||
		(strings.HasPrefix(s, "'") && strings.HasSuffix(s, "'")) {
		return s[1 : len(s)-1], nil
	}

	// 数字
	if i, err := strconv.ParseInt(s, 10, 64); err == nil {
		return i, nil
	}
	if f, err := strconv.ParseFloat(s, 64); err == nil {
		return f, nil
	}

	// 作为字符串返回
	return s, nil
}

// evaluateFunction 求值函数调用
func evaluateFunction(funcName, argsStr string, ctx *ExprContext) (interface{}, error) {
	args, err := parseArgs(argsStr, ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to parse args for %s: %w", funcName, err)
	}

	switch funcName {
	case "now":
		return time.Now().Format(time.RFC3339), nil

	case "format_time":
		if len(args) < 2 {
			return nil, fmt.Errorf("format_time requires 2 arguments")
		}
		t, err := parseTime(args[0])
		if err != nil {
			return nil, err
		}
		layout := toString(args[1])
		return t.Format(layout), nil

	case "len":
		if len(args) < 1 {
			return nil, fmt.Errorf("len requires 1 argument")
		}
		return length(args[0]), nil

	case "join":
		if len(args) < 2 {
			return nil, fmt.Errorf("join requires 2 arguments")
		}
		return joinFunc(args[0], toString(args[1])), nil

	case "split":
		if len(args) < 2 {
			return nil, fmt.Errorf("split requires 2 arguments")
		}
		return strings.Split(toString(args[0]), toString(args[1])), nil

	case "lower":
		if len(args) < 1 {
			return nil, fmt.Errorf("lower requires 1 argument")
		}
		return strings.ToLower(toString(args[0])), nil

	case "upper":
		if len(args) < 1 {
			return nil, fmt.Errorf("upper requires 1 argument")
		}
		return strings.ToUpper(toString(args[0])), nil

	case "trim":
		if len(args) < 1 {
			return nil, fmt.Errorf("trim requires 1 argument")
		}
		return strings.TrimSpace(toString(args[0])), nil

	case "default":
		if len(args) < 2 {
			return nil, fmt.Errorf("default requires 2 arguments")
		}
		if args[0] == nil || toString(args[0]) == "" {
			return args[1], nil
		}
		return args[0], nil

	case "regex_match":
		if len(args) < 2 {
			return nil, fmt.Errorf("regex_match requires 2 arguments")
		}
		re, err := regexp.Compile(toString(args[1]))
		if err != nil {
			return nil, fmt.Errorf("invalid regex: %w", err)
		}
		return re.MatchString(toString(args[0])), nil

	case "regex_find":
		if len(args) < 2 {
			return nil, fmt.Errorf("regex_find requires at least 2 arguments")
		}
		re, err := regexp.Compile(toString(args[1]))
		if err != nil {
			return nil, fmt.Errorf("invalid regex: %w", err)
		}
		group := 0
		if len(args) >= 3 {
			if g, ok := args[2].(int64); ok {
				group = int(g)
			} else if g, ok := args[2].(float64); ok {
				group = int(g)
			}
		}
		matches := re.FindStringSubmatch(toString(args[0]))
		if len(matches) > group {
			return matches[group], nil
		}
		return "", nil

	default:
		return nil, fmt.Errorf("unknown function: %s", funcName)
	}
}

// parseArgs 解析函数参数
func parseArgs(argsStr string, ctx *ExprContext) ([]interface{}, error) {
	if strings.TrimSpace(argsStr) == "" {
		return []interface{}{}, nil
	}

	// 简单的参数分割（不处理嵌套括号和引号内的逗号）
	// TODO: 实现更完善的参数解析
	var args []interface{}
	var current strings.Builder
	depth := 0
	inQuote := false
	quoteChar := rune(0)

	for _, ch := range argsStr {
		switch {
		case ch == '"' || ch == '\'':
			if !inQuote {
				inQuote = true
				quoteChar = ch
			} else if ch == quoteChar {
				inQuote = false
			}
			current.WriteRune(ch)
		case ch == '(' && !inQuote:
			depth++
			current.WriteRune(ch)
		case ch == ')' && !inQuote:
			depth--
			current.WriteRune(ch)
		case ch == ',' && depth == 0 && !inQuote:
			arg := strings.TrimSpace(current.String())
			if arg != "" {
				val, err := EvaluateExpr(arg, ctx)
				if err != nil {
					return nil, err
				}
				args = append(args, val)
			}
			current.Reset()
		default:
			current.WriteRune(ch)
		}
	}

	// 处理最后一个参数
	arg := strings.TrimSpace(current.String())
	if arg != "" {
		val, err := EvaluateExpr(arg, ctx)
		if err != nil {
			return nil, err
		}
		args = append(args, val)
	}

	return args, nil
}

// 辅助函数

func toString(v interface{}) string {
	if v == nil {
		return ""
	}
	switch val := v.(type) {
	case string:
		return val
	case int:
		return strconv.Itoa(val)
	case int64:
		return strconv.FormatInt(val, 10)
	case float64:
		return strconv.FormatFloat(val, 'f', -1, 64)
	case bool:
		return strconv.FormatBool(val)
	default:
		return fmt.Sprintf("%v", v)
	}
}

func length(v interface{}) int {
	if v == nil {
		return 0
	}
	switch val := v.(type) {
	case string:
		return len(val)
	case []interface{}:
		return len(val)
	case []string:
		return len(val)
	case map[string]interface{}:
		return len(val)
	default:
		return 0
	}
}

func joinFunc(v interface{}, sep string) string {
	switch val := v.(type) {
	case []interface{}:
		strs := make([]string, len(val))
		for i, item := range val {
			strs[i] = toString(item)
		}
		return strings.Join(strs, sep)
	case []string:
		return strings.Join(val, sep)
	default:
		return toString(v)
	}
}

func parseTime(v interface{}) (time.Time, error) {
	switch val := v.(type) {
	case time.Time:
		return val, nil
	case string:
		// 尝试多种格式
		formats := []string{
			time.RFC3339,
			time.RFC3339Nano,
			"2006-01-02 15:04:05",
			"2006-01-02",
		}
		for _, f := range formats {
			if t, err := time.Parse(f, val); err == nil {
				return t, nil
			}
		}
		return time.Time{}, fmt.Errorf("cannot parse time: %s", val)
	default:
		return time.Time{}, fmt.Errorf("cannot convert %T to time", v)
	}
}
