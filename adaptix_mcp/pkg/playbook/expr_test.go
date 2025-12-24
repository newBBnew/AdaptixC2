package playbook

import (
	"testing"
)

func TestExpandString(t *testing.T) {
	ctx := NewExprContext()
	ctx.Inputs["name"] = "Alice"
	ctx.Inputs["age"] = 25
	ctx.Inputs["items"] = []interface{}{"a", "b", "c"}

	tests := []struct {
		name     string
		input    string
		expected string
	}{
		{
			name:     "simple variable",
			input:    "Hello ${{ inputs.name }}",
			expected: "Hello Alice",
		},
		{
			name:     "multiple variables",
			input:    "${{ inputs.name }} is ${{ inputs.age }} years old",
			expected: "Alice is 25 years old",
		},
		{
			name:     "no expression",
			input:    "Hello World",
			expected: "Hello World",
		},
		{
			name:     "empty string",
			input:    "",
			expected: "",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := ExpandString(tt.input, ctx)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if result != tt.expected {
				t.Errorf("expected %q, got %q", tt.expected, result)
			}
		})
	}
}

func TestEvaluateExpr(t *testing.T) {
	ctx := NewExprContext()
	ctx.Inputs["name"] = "Alice"
	ctx.Inputs["count"] = int64(5)
	ctx.Vars["timeout"] = int64(30)
	ctx.Steps["step1"] = &StepResult{
		StepID: "step1",
		Status: "completed",
		Result: map[string]interface{}{
			"output": "user123",
		},
		Extracted: map[string]interface{}{
			"username": "user123",
		},
	}

	tests := []struct {
		name     string
		expr     string
		expected interface{}
	}{
		{
			name:     "inputs path",
			expr:     "inputs.name",
			expected: "Alice",
		},
		{
			name:     "vars path",
			expr:     "vars.timeout",
			expected: int64(30),
		},
		{
			name:     "steps extracted",
			expr:     "steps.step1.extracted.username",
			expected: "user123",
		},
		{
			name:     "steps status",
			expr:     "steps.step1.status",
			expected: "completed",
		},
		{
			name:     "literal string",
			expr:     `"hello"`,
			expected: "hello",
		},
		{
			name:     "literal number",
			expr:     "42",
			expected: int64(42),
		},
		{
			name:     "literal bool",
			expr:     "true",
			expected: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := EvaluateExpr(tt.expr, ctx)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if result != tt.expected {
				t.Errorf("expected %v (%T), got %v (%T)", tt.expected, tt.expected, result, result)
			}
		})
	}
}

func TestEvaluateFunctions(t *testing.T) {
	ctx := NewExprContext()
	ctx.Inputs["items"] = []interface{}{"a", "b", "c"}
	ctx.Inputs["text"] = "  Hello World  "
	ctx.Inputs["empty"] = ""

	tests := []struct {
		name     string
		expr     string
		expected interface{}
	}{
		{
			name:     "len array",
			expr:     "len(inputs.items)",
			expected: 3,
		},
		{
			name:     "len string",
			expr:     `len("hello")`,
			expected: 5,
		},
		{
			name:     "join",
			expr:     `join(inputs.items, ",")`,
			expected: "a,b,c",
		},
		{
			name:     "split",
			expr:     `split("a,b,c", ",")`,
			expected: []string{"a", "b", "c"},
		},
		{
			name:     "lower",
			expr:     `lower("HELLO")`,
			expected: "hello",
		},
		{
			name:     "upper",
			expr:     `upper("hello")`,
			expected: "HELLO",
		},
		{
			name:     "trim",
			expr:     "trim(inputs.text)",
			expected: "Hello World",
		},
		{
			name:     "default with nil",
			expr:     `default(inputs.missing, "fallback")`,
			expected: "fallback",
		},
		{
			name:     "default with empty",
			expr:     `default(inputs.empty, "fallback")`,
			expected: "fallback",
		},
		{
			name:     "regex_match true",
			expr:     `regex_match("hello123", "[0-9]+")`,
			expected: true,
		},
		{
			name:     "regex_match false",
			expr:     `regex_match("hello", "[0-9]+")`,
			expected: false,
		},
		{
			name:     "regex_find",
			expr:     `regex_find("user=alice", "user=(.+)", 1)`,
			expected: "alice",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result, err := EvaluateExpr(tt.expr, ctx)
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			// 特殊处理切片比较
			switch expected := tt.expected.(type) {
			case []string:
				resultSlice, ok := result.([]string)
				if !ok {
					t.Errorf("expected []string, got %T", result)
					return
				}
				if len(resultSlice) != len(expected) {
					t.Errorf("expected %v, got %v", expected, resultSlice)
					return
				}
				for i, v := range expected {
					if resultSlice[i] != v {
						t.Errorf("expected %v, got %v", expected, resultSlice)
						return
					}
				}
			default:
				if result != tt.expected {
					t.Errorf("expected %v (%T), got %v (%T)", tt.expected, tt.expected, result, result)
				}
			}
		})
	}
}
