package playbook

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"
)

// ToolInvoker MCP 工具调用接口
type ToolInvoker func(toolName string, args map[string]interface{}) (interface{}, error)

// Step playbook 步骤定义
type Step struct {
	ID             string                 `yaml:"id" json:"id"`
	Name           string                 `yaml:"name" json:"name"`
	When           string                 `yaml:"when" json:"when"`
	Foreach        *ForeachSpec           `yaml:"foreach" json:"foreach"`
	Action         string                 `yaml:"action" json:"action"`
	Args           map[string]interface{} `yaml:"args" json:"args"`
	TimeoutSeconds int                    `yaml:"timeoutSeconds" json:"timeoutSeconds"`
	Retry          *RetrySpec             `yaml:"retry" json:"retry"`
	OnError        string                 `yaml:"onError" json:"onError"` // "continue", "fail"
	SaveAs         string                 `yaml:"saveAs" json:"saveAs"`
	Extractors     []Extractor            `yaml:"extractors" json:"extractors"`
}

// ForeachSpec foreach 循环定义
type ForeachSpec struct {
	In       string `yaml:"in" json:"in"`             // 表达式，返回数组
	ItemVar  string `yaml:"itemVar" json:"itemVar"`   // 当前项变量名，默认 "item"
	IndexVar string `yaml:"indexVar" json:"indexVar"` // 索引变量名，默认 "index"
}

// RetrySpec 重试配置
type RetrySpec struct {
	MaxAttempts int `yaml:"maxAttempts" json:"maxAttempts"`
	DelayMs     int `yaml:"delayMs" json:"delayMs"`
}

// RunState 运行状态
type RunState struct {
	RunID      string                 `json:"run_id"`
	PlaybookID string                 `json:"playbook_id"`
	Status     string                 `json:"status"` // "planned", "running", "completed", "failed"
	Inputs     map[string]interface{} `json:"inputs"`
	Steps      map[string]*StepResult `json:"steps"`
	Outputs    map[string]interface{} `json:"outputs,omitempty"`
	StartTime  time.Time              `json:"start_time"`
	EndTime    time.Time              `json:"end_time"`
	Error      string                 `json:"error,omitempty"`
}

// Engine playbook 执行引擎
type Engine struct {
	catalog     *ActionCatalog
	toolInvoker ToolInvoker
}

// NewEngine 创建新的执行引擎
func NewEngine(catalog *ActionCatalog, invoker ToolInvoker) *Engine {
	return &Engine{
		catalog:     catalog,
		toolInvoker: invoker,
	}
}

// Execute 执行 playbook
func (e *Engine) Execute(doc *PlaybookDoc, inputs map[string]interface{}) (*RunState, error) {
	// 初始化运行状态
	state := &RunState{
		RunID:      fmt.Sprintf("run_%s", time.Now().Format("20060102_150405")),
		PlaybookID: doc.Metadata.ID,
		Status:     "running",
		Inputs:     inputs,
		Steps:      make(map[string]*StepResult),
		StartTime:  time.Now(),
	}

	// 初始化表达式上下文
	ctx := NewExprContext()
	ctx.Inputs = inputs
	ctx.Metadata = map[string]interface{}{
		"id":          doc.Metadata.ID,
		"name":        doc.Metadata.Name,
		"version":     doc.Metadata.Version,
		"description": doc.Metadata.Description,
	}

	// 解析并设置变量 (vars)
	if varsRaw, ok := doc.Spec["vars"].(map[string]interface{}); ok {
		for k, v := range varsRaw {
			// 展开变量值中的表达式
			expanded, err := e.expandValue(v, ctx)
			if err != nil {
				state.Status = "failed"
				state.Error = fmt.Sprintf("failed to expand var %s: %v", k, err)
				state.EndTime = time.Now()
				return state, err
			}
			ctx.Vars[k] = expanded
		}
	}

	// 获取步骤列表
	steps, err := e.parseSteps(doc.Spec)
	if err != nil {
		state.Status = "failed"
		state.Error = fmt.Sprintf("failed to parse steps: %v", err)
		state.EndTime = time.Now()
		return state, err
	}

	// 执行每个步骤
	for _, step := range steps {
		// 检查是否有 foreach
		if step.Foreach != nil {
			results, err := e.executeForeach(step, ctx, state)
			if err != nil {
				if step.OnError == "continue" {
					continue
				}
				state.Status = "failed"
				state.Error = fmt.Sprintf("step %s foreach failed: %v", step.ID, err)
				state.EndTime = time.Now()
				return state, nil
			}
			// 合并 foreach 结果
			for id, result := range results {
				state.Steps[id] = result
				ctx.Steps[id] = result
			}
		} else {
			stepResult, err := e.executeStepWithRetry(step, ctx)
			state.Steps[step.ID] = stepResult
			ctx.Steps[step.ID] = stepResult

			if err != nil {
				// 检查 onError 策略
				if step.OnError == "continue" {
					continue
				}
				state.Status = "failed"
				state.Error = fmt.Sprintf("step %s failed: %v", step.ID, err)
				state.EndTime = time.Now()
				return state, nil // 返回状态但不返回错误，让调用方可以看到详细结果
			}
		}
	}

	// 处理 outputs
	state.Outputs = e.processOutputs(doc.Spec, ctx)

	state.Status = "completed"
	state.EndTime = time.Now()
	return state, nil
}

// parseSteps 从 spec 中解析步骤列表
func (e *Engine) parseSteps(spec map[string]interface{}) ([]Step, error) {
	stepsRaw, ok := spec["steps"]
	if !ok {
		return nil, fmt.Errorf("spec.steps is required")
	}

	stepsArr, ok := stepsRaw.([]interface{})
	if !ok {
		return nil, fmt.Errorf("spec.steps must be an array")
	}

	steps := make([]Step, 0, len(stepsArr))
	for i, stepRaw := range stepsArr {
		stepMap, ok := stepRaw.(map[string]interface{})
		if !ok {
			return nil, fmt.Errorf("step %d is not an object", i)
		}

		step, err := e.parseStep(stepMap)
		if err != nil {
			return nil, fmt.Errorf("step %d: %w", i, err)
		}
		steps = append(steps, step)
	}

	return steps, nil
}

// parseStep 解析单个步骤
func (e *Engine) parseStep(m map[string]interface{}) (Step, error) {
	step := Step{
		OnError: "fail", // 默认值
	}

	if id, ok := m["id"].(string); ok {
		step.ID = id
	} else {
		return step, fmt.Errorf("step.id is required")
	}

	if name, ok := m["name"].(string); ok {
		step.Name = name
	}

	if when, ok := m["when"].(string); ok {
		step.When = when
	}

	if action, ok := m["action"].(string); ok {
		step.Action = action
	} else {
		return step, fmt.Errorf("step.action is required")
	}

	if args, ok := m["args"].(map[string]interface{}); ok {
		step.Args = args
	} else {
		step.Args = make(map[string]interface{})
	}

	if saveAs, ok := m["saveAs"].(string); ok {
		step.SaveAs = saveAs
	}

	if onError, ok := m["onError"].(string); ok {
		step.OnError = onError
	}

	if timeout, ok := m["timeoutSeconds"].(int); ok {
		step.TimeoutSeconds = timeout
	} else if timeout, ok := m["timeoutSeconds"].(float64); ok {
		step.TimeoutSeconds = int(timeout)
	}

	// 解析 extractors
	if extractorsRaw, ok := m["extractors"].([]interface{}); ok {
		for _, extRaw := range extractorsRaw {
			if extMap, ok := extRaw.(map[string]interface{}); ok {
				ext := Extractor{}
				if id, ok := extMap["id"].(string); ok {
					ext.ID = id
				}
				if key, ok := extMap["key"].(string); ok {
					ext.Key = key
				}
				if from, ok := extMap["from"].(string); ok {
					ext.From = from
				}
				if t, ok := extMap["type"].(string); ok {
					ext.Type = t
				}
				if opts, ok := extMap["options"].(map[string]interface{}); ok {
					ext.Options = opts
				}
				if req, ok := extMap["required"].(bool); ok {
					ext.Required = req
				}
				step.Extractors = append(step.Extractors, ext)
			}
		}
	}

	return step, nil
}

// executeStep 执行单个步骤
func (e *Engine) executeStep(step Step, ctx *ExprContext) (*StepResult, error) {
	result := &StepResult{
		StepID:    step.ID,
		Status:    "running",
		StartTime: time.Now(),
		Result:    make(map[string]interface{}),
		Extracted: make(map[string]interface{}),
	}

	// 检查 when 条件
	if step.When != "" {
		shouldRun, err := e.evaluateCondition(step.When, ctx)
		if err != nil {
			result.Status = "failed"
			result.Error = fmt.Sprintf("failed to evaluate when condition: %v", err)
			result.EndTime = time.Now()
			return result, err
		}
		if !shouldRun {
			result.Status = "skipped"
			result.EndTime = time.Now()
			return result, nil
		}
	}

	// 展开参数中的表达式
	expandedArgs, err := e.expandArgs(step.Args, ctx)
	if err != nil {
		result.Status = "failed"
		result.Error = fmt.Sprintf("failed to expand args: %v", err)
		result.EndTime = time.Now()
		return result, err
	}

	var toolResult interface{}

	// 检查是否为内部 action
	if e.catalog.IsInternalAction(step.Action) {
		handler, err := e.catalog.GetHandler(step.Action)
		if err != nil {
			result.Status = "failed"
			result.Error = fmt.Sprintf("failed to get handler: %v", err)
			result.EndTime = time.Now()
			return result, err
		}

		toolResult, err = e.executeInternalAction(handler, expandedArgs, ctx)
		if err != nil {
			result.Status = "failed"
			result.Error = err.Error()
			result.EndTime = time.Now()
			return result, err
		}
	} else {
		// 解析 action 到 tool
		toolName, err := e.catalog.GetToolName(step.Action)
		if err != nil {
			result.Status = "failed"
			result.Error = fmt.Sprintf("failed to resolve action: %v", err)
			result.EndTime = time.Now()
			return result, err
		}

		// 调用工具
		toolResult, err = e.toolInvoker(toolName, expandedArgs)
		if err != nil {
			result.Status = "failed"
			result.Error = err.Error()
			result.EndTime = time.Now()
			return result, err
		}
	}

	// 保存原始结果
	if resultMap, ok := toolResult.(map[string]interface{}); ok {
		result.Result = resultMap
	} else {
		result.Result["value"] = toolResult
	}

	// 运行 extractors
	if len(step.Extractors) > 0 {
		// 获取输出文本
		output := ""
		if o, ok := result.Result["output"].(string); ok {
			output = o
		} else {
			// 尝试 JSON 序列化
			if b, err := json.Marshal(result.Result); err == nil {
				output = string(b)
			}
		}

		extracted, err := RunExtractors(output, step.Extractors)
		if err != nil {
			result.Status = "failed"
			result.Error = fmt.Sprintf("extractors failed: %v", err)
			result.EndTime = time.Now()
			return result, err
		}
		result.Extracted = extracted
	}

	result.Status = "completed"
	result.EndTime = time.Now()
	return result, nil
}

// executeInternalAction 执行内部 action
func (e *Engine) executeInternalAction(handler string, args map[string]interface{}, ctx *ExprContext) (interface{}, error) {
	switch handler {
	case "log":
		// core.log: 记录日志消息
		message := ""
		if msg, ok := args["message"].(string); ok {
			message = msg
		}
		level := "info"
		if lvl, ok := args["level"].(string); ok {
			level = lvl
		}
		// 返回日志信息（实际应用中可以写入日志系统）
		return map[string]interface{}{
			"logged":  true,
			"level":   level,
			"message": message,
		}, nil

	case "sleep":
		// core.sleep: 等待指定时间
		durationMs := 0
		if d, ok := args["duration_ms"].(float64); ok {
			durationMs = int(d)
		} else if d, ok := args["duration_ms"].(int); ok {
			durationMs = d
		} else if d, ok := args["duration_ms"].(int64); ok {
			durationMs = int(d)
		}
		if durationMs > 0 {
			time.Sleep(time.Duration(durationMs) * time.Millisecond)
		}
		return map[string]interface{}{
			"slept_ms": durationMs,
		}, nil

	case "set_var":
		// core.set_var: 设置变量
		varName := ""
		if name, ok := args["name"].(string); ok {
			varName = name
		}
		varValue := args["value"]
		if varName != "" {
			ctx.Vars[varName] = varValue
		}
		return map[string]interface{}{
			"name":  varName,
			"value": varValue,
		}, nil

	case "fail":
		// core.fail: 主动失败
		message := "Playbook failed"
		if msg, ok := args["message"].(string); ok {
			message = msg
		}
		return nil, fmt.Errorf("%s", message)

	default:
		return nil, fmt.Errorf("unknown internal handler: %s", handler)
	}
}

// evaluateCondition 评估条件表达式（支持比较和逻辑运算符）
func (e *Engine) evaluateCondition(when string, ctx *ExprContext) (bool, error) {
	when = strings.TrimSpace(when)

	// 检查逻辑运算符 AND
	if strings.Contains(when, " && ") {
		parts := strings.SplitN(when, " && ", 2)
		left, err := e.evaluateCondition(parts[0], ctx)
		if err != nil {
			return false, err
		}
		if !left {
			return false, nil // 短路求值
		}
		return e.evaluateCondition(parts[1], ctx)
	}

	// 检查逻辑运算符 OR
	if strings.Contains(when, " || ") {
		parts := strings.SplitN(when, " || ", 2)
		left, err := e.evaluateCondition(parts[0], ctx)
		if err != nil {
			return false, err
		}
		if left {
			return true, nil // 短路求值
		}
		return e.evaluateCondition(parts[1], ctx)
	}

	// 检查 NOT 运算符
	if strings.HasPrefix(when, "!") {
		result, err := e.evaluateCondition(strings.TrimSpace(when[1:]), ctx)
		if err != nil {
			return false, err
		}
		return !result, nil
	}

	// 检查比较运算符
	operators := []string{"==", "!=", ">=", "<=", ">", "<"}
	for _, op := range operators {
		if idx := strings.Index(when, op); idx > 0 {
			leftExpr := strings.TrimSpace(when[:idx])
			rightExpr := strings.TrimSpace(when[idx+len(op):])

			leftVal, err := EvaluateExpr(leftExpr, ctx)
			if err != nil {
				return false, err
			}
			rightVal, err := EvaluateExpr(rightExpr, ctx)
			if err != nil {
				return false, err
			}

			return e.compareValues(leftVal, rightVal, op)
		}
	}

	// 展开表达式并判断 truthy
	expanded, err := ExpandString(when, ctx)
	if err != nil {
		return false, err
	}

	// 简单判断：非空且非 "false" 即为真
	if expanded == "" || expanded == "false" || expanded == "0" || expanded == "null" || expanded == "nil" {
		return false, nil
	}
	return true, nil
}

// compareValues 比较两个值
func (e *Engine) compareValues(left, right interface{}, op string) (bool, error) {
	// 尝试数值比较
	leftNum, leftIsNum := e.toNumber(left)
	rightNum, rightIsNum := e.toNumber(right)

	if leftIsNum && rightIsNum {
		switch op {
		case "==":
			return leftNum == rightNum, nil
		case "!=":
			return leftNum != rightNum, nil
		case ">":
			return leftNum > rightNum, nil
		case "<":
			return leftNum < rightNum, nil
		case ">=":
			return leftNum >= rightNum, nil
		case "<=":
			return leftNum <= rightNum, nil
		}
	}

	// 字符串比较
	leftStr := toString(left)
	rightStr := toString(right)

	switch op {
	case "==":
		return leftStr == rightStr, nil
	case "!=":
		return leftStr != rightStr, nil
	case ">":
		return leftStr > rightStr, nil
	case "<":
		return leftStr < rightStr, nil
	case ">=":
		return leftStr >= rightStr, nil
	case "<=":
		return leftStr <= rightStr, nil
	}

	return false, fmt.Errorf("unsupported operator: %s", op)
}

// toNumber 尝试将值转换为数字
func (e *Engine) toNumber(v interface{}) (float64, bool) {
	switch val := v.(type) {
	case int:
		return float64(val), true
	case int64:
		return float64(val), true
	case float64:
		return val, true
	case string:
		if f, err := strconv.ParseFloat(val, 64); err == nil {
			return f, true
		}
	}
	return 0, false
}

// expandArgs 展开参数中的表达式
func (e *Engine) expandArgs(args map[string]interface{}, ctx *ExprContext) (map[string]interface{}, error) {
	result := make(map[string]interface{})

	for key, value := range args {
		expanded, err := e.expandValue(value, ctx)
		if err != nil {
			return nil, fmt.Errorf("failed to expand %s: %w", key, err)
		}
		result[key] = expanded
	}

	return result, nil
}

// expandValue 递归展开值中的表达式
func (e *Engine) expandValue(value interface{}, ctx *ExprContext) (interface{}, error) {
	switch v := value.(type) {
	case string:
		return ExpandString(v, ctx)
	case map[string]interface{}:
		result := make(map[string]interface{})
		for k, val := range v {
			expanded, err := e.expandValue(val, ctx)
			if err != nil {
				return nil, err
			}
			result[k] = expanded
		}
		return result, nil
	case []interface{}:
		result := make([]interface{}, len(v))
		for i, val := range v {
			expanded, err := e.expandValue(val, ctx)
			if err != nil {
				return nil, err
			}
			result[i] = expanded
		}
		return result, nil
	default:
		return value, nil
	}
}

// executeStepWithRetry 执行步骤，支持重试机制
func (e *Engine) executeStepWithRetry(step Step, ctx *ExprContext) (*StepResult, error) {
	maxAttempts := 1
	delayMs := 0

	if step.Retry != nil {
		if step.Retry.MaxAttempts > 0 {
			maxAttempts = step.Retry.MaxAttempts
		}
		if step.Retry.DelayMs > 0 {
			delayMs = step.Retry.DelayMs
		}
	}

	var lastResult *StepResult
	var lastErr error

	for attempt := 1; attempt <= maxAttempts; attempt++ {
		// 超时控制
		if step.TimeoutSeconds > 0 {
			result, err := e.executeStepWithTimeout(step, ctx, step.TimeoutSeconds)
			lastResult = result
			lastErr = err
		} else {
			result, err := e.executeStep(step, ctx)
			lastResult = result
			lastErr = err
		}

		// 成功则返回
		if lastErr == nil && lastResult.Status == "completed" {
			return lastResult, nil
		}

		// 最后一次尝试不需要延迟
		if attempt < maxAttempts && delayMs > 0 {
			time.Sleep(time.Duration(delayMs) * time.Millisecond)
		}
	}

	return lastResult, lastErr
}

// executeStepWithTimeout 带超时的步骤执行
func (e *Engine) executeStepWithTimeout(step Step, ctx *ExprContext, timeoutSec int) (*StepResult, error) {
	resultChan := make(chan *StepResult, 1)
	errChan := make(chan error, 1)

	go func() {
		result, err := e.executeStep(step, ctx)
		if err != nil {
			errChan <- err
			return
		}
		resultChan <- result
	}()

	select {
	case result := <-resultChan:
		return result, nil
	case err := <-errChan:
		return &StepResult{
			StepID:  step.ID,
			Status:  "failed",
			Error:   err.Error(),
			EndTime: time.Now(),
		}, err
	case <-time.After(time.Duration(timeoutSec) * time.Second):
		return &StepResult{
			StepID:  step.ID,
			Status:  "failed",
			Error:   fmt.Sprintf("step timed out after %d seconds", timeoutSec),
			EndTime: time.Now(),
		}, fmt.Errorf("timeout after %d seconds", timeoutSec)
	}
}

// executeForeach 执行 foreach 循环
func (e *Engine) executeForeach(step Step, ctx *ExprContext, state *RunState) (map[string]*StepResult, error) {
	results := make(map[string]*StepResult)

	if step.Foreach == nil {
		return results, nil
	}

	// 展开 foreach.in 表达式获取数组
	itemsRaw, err := EvaluateExpr(step.Foreach.In, ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to evaluate foreach.in: %w", err)
	}

	// 转换为数组
	var items []interface{}
	switch v := itemsRaw.(type) {
	case []interface{}:
		items = v
	case []string:
		for _, s := range v {
			items = append(items, s)
		}
	default:
		return nil, fmt.Errorf("foreach.in must evaluate to an array, got %T", itemsRaw)
	}

	// 确定变量名
	itemVar := "item"
	indexVar := "index"
	if step.Foreach.ItemVar != "" {
		itemVar = step.Foreach.ItemVar
	}
	if step.Foreach.IndexVar != "" {
		indexVar = step.Foreach.IndexVar
	}

	// 遍历执行
	for i, item := range items {
		// 创建临时上下文，注入循环变量
		loopCtx := &ExprContext{
			Inputs:   ctx.Inputs,
			Vars:     make(map[string]interface{}),
			Steps:    ctx.Steps,
			Metadata: ctx.Metadata,
		}
		// 复制原有变量
		for k, v := range ctx.Vars {
			loopCtx.Vars[k] = v
		}
		// 注入循环变量
		loopCtx.Vars[itemVar] = item
		loopCtx.Vars[indexVar] = i

		// 生成唯一步骤 ID
		iterStepID := fmt.Sprintf("%s_%d", step.ID, i)
		iterStep := step
		iterStep.ID = iterStepID

		// 执行步骤
		result, err := e.executeStepWithRetry(iterStep, loopCtx)
		results[iterStepID] = result

		if err != nil && step.OnError != "continue" {
			return results, err
		}
	}

	return results, nil
}

// processOutputs 处理 outputs 定义
func (e *Engine) processOutputs(spec map[string]interface{}, ctx *ExprContext) map[string]interface{} {
	outputs := make(map[string]interface{})

	outputsRaw, ok := spec["outputs"].(map[string]interface{})
	if !ok {
		return outputs
	}

	for key, def := range outputsRaw {
		defMap, ok := def.(map[string]interface{})
		if !ok {
			continue
		}

		// 处理 template 类型
		if template, ok := defMap["template"].(string); ok {
			expanded, err := ExpandString(template, ctx)
			if err == nil {
				outputs[key] = expanded
			} else {
				outputs[key] = fmt.Sprintf("Error expanding template: %v", err)
			}
			continue
		}

		// 处理 value 类型（直接引用表达式）
		if value, ok := defMap["value"].(string); ok {
			expanded, err := ExpandString(value, ctx)
			if err == nil {
				outputs[key] = expanded
			} else {
				outputs[key] = fmt.Sprintf("Error expanding value: %v", err)
			}
		}
	}

	return outputs
}

// SaveRun 保存运行记录到文件
func SaveRun(state *RunState) (*RunRecord, string, error) {
	runsDir, err := RunsDirForDate(state.StartTime)
	if err != nil {
		return nil, "", err
	}

	if err := os.MkdirAll(runsDir, 0o755); err != nil {
		return nil, "", err
	}

	path := fmt.Sprintf("%s/%s.json", runsDir, state.RunID)

	rec := &RunRecord{
		RunID:      state.RunID,
		PlaybookID: state.PlaybookID,
		CreatedAt:  state.StartTime.Format(time.RFC3339Nano),
		Inputs:     state.Inputs,
		Status:     state.Status,
	}

	// 将步骤结果转换为 plan
	stepsData := make(map[string]interface{})
	for id, step := range state.Steps {
		stepsData[id] = map[string]interface{}{
			"status":     step.Status,
			"result":     step.Result,
			"extracted":  step.Extracted,
			"error":      step.Error,
			"start_time": step.StartTime.Format(time.RFC3339Nano),
			"end_time":   step.EndTime.Format(time.RFC3339Nano),
		}
	}
	rec.Plan = map[string]interface{}{
		"steps":      stepsData,
		"start_time": state.StartTime.Format(time.RFC3339Nano),
		"end_time":   state.EndTime.Format(time.RFC3339Nano),
		"error":      state.Error,
	}

	data, err := json.MarshalIndent(rec, "", "  ")
	if err != nil {
		return nil, "", err
	}

	if err := os.WriteFile(path, data, 0o644); err != nil {
		return nil, "", err
	}

	return rec, path, nil
}
