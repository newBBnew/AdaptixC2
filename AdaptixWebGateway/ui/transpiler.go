package ui

import (
	"fmt"

	"github.com/dop251/goja"
)

// UIComponent represents a generic UI element in the schema
type UIComponent struct {
	Type       string                 `json:"type"`
	ID         string                 `json:"id"`
	Label      string                 `json:"label,omitempty"`
	Default    interface{}            `json:"default,omitempty"`
	Items      []string               `json:"items,omitempty"` // For combos
	Properties map[string]interface{} `json:"properties,omitempty"`
	Children   []*UIComponent         `json:"children,omitempty"`
}

// UISchema represents the final transpiled output
type UISchema struct {
	Root   *UIComponent            `json:"root"`
	Fields map[string]*UIComponent `json:"fields"` // Map key -> Component (for data binding)
	Width  int                     `json:"width"`
	Height int                     `json:"height"`
}

// TranspileScript executes an AxScript and extracts the UI definition
func TranspileScript(scriptContent string, entryFunc string, args ...interface{}) (*UISchema, error) {
	vm := goja.New()
	schema := &UISchema{
		Fields: make(map[string]*UIComponent),
	}

	// Mock Object Registry to keep track of created elements
	// registry := make(map[string]*UIComponent) // Unused for now

	// Helper to create mock objects
	createMock := func(call goja.FunctionCall, typeName string) goja.Value {
		comp := &UIComponent{
			Type:       typeName,
			Properties: make(map[string]interface{}),
			Children:   make([]*UIComponent, 0),
		}

		obj := vm.NewObject()

		// Common methods
		obj.Set("setEnabled", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
		obj.Set("setVisible", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
		obj.Set("setCurrentIndex", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Properties["current_index"] = call.Argument(0).ToInteger()
			}
			return goja.Undefined()
		})
		obj.Set("setPlaceholder", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Properties["placeholder"] = call.Argument(0).String()
			}
			return goja.Undefined()
		})
		obj.Set("setText", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Default = call.Argument(0).String()
			}
			return goja.Undefined()
		})

		// Capture basic args based on type
		if len(call.Arguments) > 0 {
			comp.Label = call.Argument(0).String()
		}
		if len(call.Arguments) > 1 {
			comp.Default = call.Argument(1).Export()
		}

		// Store reference for layouts/containers
		obj.Set("__internal", comp)

		return obj
	}

	// --- Mock 'ax' object (Utilities) ---
	ax := vm.NewObject()

	ax.Set("interfaces", func(goja.FunctionCall) goja.Value {
		return vm.ToValue([]string{"0.0.0.0", "127.0.0.1"})
	})

	ax.Set("random_string", func(call goja.FunctionCall) goja.Value {
		// Mock random string
		return vm.ToValue("a1b2c3d4e5f6")
	})

	ax.Set("script_dir", func(goja.FunctionCall) goja.Value { return vm.ToValue("/") })
	ax.Set("log", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	vm.Set("ax", ax)

	// --- Mock 'menu' object (for scripts that build UI menus during transpile) ---
	menu := vm.NewObject()
	menu.Set("create_action", func(call goja.FunctionCall) goja.Value {
		obj := vm.NewObject()
		if len(call.Arguments) > 0 {
			obj.Set("title", call.Argument(0))
		}
		if len(call.Arguments) > 1 {
			obj.Set("handler", call.Argument(1))
		}
		return obj
	})
	menu.Set("create_menu", func(call goja.FunctionCall) goja.Value {
		obj := vm.NewObject()
		if len(call.Arguments) > 0 {
			obj.Set("title", call.Argument(0))
		}
		obj.Set("addItem", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
		return obj
	})
	menu.Set("create_separator", func(goja.FunctionCall) goja.Value {
		obj := vm.NewObject()
		obj.Set("type", "separator")
		return obj
	})

	// Registration functions (no-op for transpile)
	for _, fnName := range []string{
		"add_session_access",
		"add_session_agent",
		"add_session_browser",
		"add_session_main",
		"add_session_command",
		"add_session_connection",
		"add_session_credential",
		"add_session_file",
		"add_session_listener",
		"add_session_note",
		"add_session_process",
		"add_session_route",
		"add_session_screenshot",
		"add_session_target",
		"add_session_task",
		"add_session_tunnel",
		"add_session_user",
		"add_filebrowser",
		"add_processbrowser",
		"add_downloads_running",
		"add_downloads_finished",
		"add_tasks",
		"add_tasks_job",
		"add_targets",
		"add_credentials",
	} {
		localName := fnName
		menu.Set(localName, func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	}
	vm.Set("menu", menu)

	// --- Mock 'event' object (Qt BridgeEvent compatibility) ---
	// Used by Agent GenerateUI scripts to register handlers. During transpile we only need the API surface.
	event := vm.NewObject()
	for _, fnName := range []string{
		"on_filebrowser_disks",
		"on_filebrowser_list",
		"on_filebrowser_upload",
		"on_processbrowser_list",
		"on_new_agent",
		"on_disconnect",
		"on_ready",
	} {
		localName := fnName
		event.Set(localName, func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	}
	// Timer-style helpers return an event_id in Qt; for transpile any stable string is fine.
	event.Set("on_interval", func(goja.FunctionCall) goja.Value { return vm.ToValue("interval") })
	event.Set("on_timeout", func(goja.FunctionCall) goja.Value { return vm.ToValue("timeout") })
	event.Set("list", func(goja.FunctionCall) goja.Value { return vm.ToValue([]interface{}{}) })
	event.Set("remove", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
	vm.Set("event", event)

	// --- Mock 'form' object (UI Factory) ---
	form := vm.NewObject()

	// Helper for common widget methods
	addCommonMethods := func(obj *goja.Object, comp *UIComponent) {
		obj.Set("setEnabled", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
		obj.Set("setVisible", func(goja.FunctionCall) goja.Value { return goja.Undefined() })
		obj.Set("setPlaceholder", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Properties["placeholder"] = call.Argument(0).String()
			}
			return goja.Undefined()
		})
		obj.Set("setText", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Default = call.Argument(0).String()
			}
			return goja.Undefined()
		})
	}

	// form.create_panel
	form.Set("create_panel", func(call goja.FunctionCall) goja.Value {
		comp := &UIComponent{Type: "panel", Children: make([]*UIComponent, 0)}
		obj := vm.NewObject()
		addCommonMethods(obj, comp)

		obj.Set("setLayout", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				layoutObj := call.Argument(0).ToObject(vm)
				internal := layoutObj.Get("__internal")
				if internal != nil {
					layoutComp := internal.Export().(*UIComponent)
					// In our schema, a panel just wraps the layout.
					// Or the layout IS the panel's content.
					// Let's add layout as a child of panel.
					comp.Children = append(comp.Children, layoutComp)
				}
			}
			return goja.Undefined()
		})

		obj.Set("__internal", comp)
		return obj
	})

	// form.create_input / create_textline
	createText := func(call goja.FunctionCall) goja.Value {
		obj := createMock(call, "input")
		return obj
	}
	form.Set("create_input", createText)
	form.Set("create_textline", createText)

	// form.create_textmulti
	form.Set("create_textmulti", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "textarea")
	})

	// form.create_button
	form.Set("create_button", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "button")
	})

	// form.create_list
	form.Set("create_list", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "list")
	})

	// form.create_table
	form.Set("create_table", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "table")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			if len(call.Arguments) > 0 {
				comp.Properties["headers"] = call.Argument(0).Export()
			}
		}
		return val
	})

	// form.create_spin
	form.Set("create_spin", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "spin")
		obj := val.ToObject(vm)
		internal := obj.Get("__internal")
		if internal != nil {
			comp := internal.Export().(*UIComponent)
			obj.Set("setRange", func(call goja.FunctionCall) goja.Value {
				if len(call.Arguments) > 1 {
					comp.Properties["min"] = call.Argument(0).ToInteger()
					comp.Properties["max"] = call.Argument(1).ToInteger()
				}
				return goja.Undefined()
			})
			obj.Set("setValue", func(call goja.FunctionCall) goja.Value {
				if len(call.Arguments) > 0 {
					comp.Default = call.Argument(0).ToInteger()
				}
				return goja.Undefined()
			})
		}
		return val
	})

	// form.create_scrollarea
	form.Set("create_scrollarea", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "scrollarea")
	})

	// form.create_stack
	form.Set("create_stack", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "stack")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			obj.Set("setCurrentIndex", func(call goja.FunctionCall) goja.Value {
				if len(call.Arguments) > 0 {
					comp.Properties["current_index"] = call.Argument(0).ToInteger()
				}
				return goja.Undefined()
			})
		}
		return val
	})

	// form.create_hsplitter / create_vsplitter
	form.Set("create_hsplitter", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "splitter")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			comp.Properties["orientation"] = "h"
		}
		return val
	})
	form.Set("create_vsplitter", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "splitter")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			comp.Properties["orientation"] = "v"
		}
		return val
	})

	// form.create_dialog
	form.Set("create_dialog", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "dialog")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			if len(call.Arguments) > 0 {
				comp.Properties["title"] = call.Argument(0).String()
			}
		}
		return val
	})

	// form.create_combo
	form.Set("create_combo", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "select")
		obj := val.ToObject(vm)
		internal := obj.Get("__internal")
		if internal == nil {
			return val
		}
		comp := internal.Export().(*UIComponent)

		obj.Set("addItem", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Items = append(comp.Items, call.Argument(0).String())
			}
			return goja.Undefined()
		})
		obj.Set("addItems", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				itemsVal := call.Argument(0).Export()
				if items, ok := itemsVal.([]interface{}); ok {
					for _, item := range items {
						comp.Items = append(comp.Items, fmt.Sprint(item))
					}
				}
			}
			return goja.Undefined()
		})
		obj.Set("clear", func(goja.FunctionCall) goja.Value {
			comp.Items = []string{}
			return goja.Undefined()
		})

		return val
	})

	// form.create_check
	form.Set("create_check", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "checkbox")
	})

	// form.create_label
	form.Set("create_label", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "label")
	})

	// form.create_spacer
	form.Set("create_spacer", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "spacer")
	})

	// Qt-compat: form.create_vspacer / create_hspacer
	form.Set("create_vspacer", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "spacer")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			comp.Properties["orientation"] = "v"
		}
		return val
	})
	form.Set("create_hspacer", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "spacer")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			comp.Properties["orientation"] = "h"
		}
		return val
	})

	// Qt-compat: form.create_vline / create_hline
	form.Set("create_vline", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "line")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			comp.Properties["orientation"] = "v"
		}
		return val
	})
	form.Set("create_hline", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "line")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			comp.Properties["orientation"] = "h"
		}
		return val
	})

	// form.create_selector_file
	form.Set("create_selector_file", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "file_selector")
	})

	// form.create_selector_credentials
	form.Set("create_selector_credentials", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "credentials_selector")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			if len(call.Arguments) > 0 {
				comp.Properties["headers"] = call.Argument(0).Export()
			}
		}
		return val
	})

	// form.create_selector_agents
	form.Set("create_selector_agents", func(call goja.FunctionCall) goja.Value {
		val := createMock(call, "agents_selector")
		obj := val.ToObject(vm)
		if internal := obj.Get("__internal"); internal != nil {
			comp := internal.Export().(*UIComponent)
			if len(call.Arguments) > 0 {
				comp.Properties["headers"] = call.Argument(0).Export()
			}
		}
		return val
	})

	// form.create_dateline
	form.Set("create_dateline", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "date")
	})

	// form.create_timeline
	form.Set("create_timeline", func(call goja.FunctionCall) goja.Value {
		return createMock(call, "time")
	})

	// form.create_groupbox
	form.Set("create_groupbox", func(call goja.FunctionCall) goja.Value {
		// Arguments: title, checkable
		val := createMock(call, "groupbox")
		obj := val.ToObject(vm)
		internal := obj.Get("__internal")
		if internal != nil {
			comp := internal.Export().(*UIComponent)
			if len(call.Arguments) > 1 {
				comp.Properties["checkable"] = call.Argument(1).ToBoolean()
			}
			obj.Set("setChecked", func(call goja.FunctionCall) goja.Value {
				if len(call.Arguments) > 0 {
					comp.Properties["checked"] = call.Argument(0).ToBoolean()
				}
				return goja.Undefined()
			})
			obj.Set("setPanel", func(call goja.FunctionCall) goja.Value {
				if len(call.Arguments) > 0 {
					panelObj := call.Argument(0).ToObject(vm)
					internalP := panelObj.Get("__internal")
					if internalP != nil {
						childComp := internalP.Export().(*UIComponent)
						comp.Children = append(comp.Children, childComp)
					}
				}
				return goja.Undefined()
			})
		}
		return val
	})

	// form.create_tabs
	form.Set("create_tabs", func(call goja.FunctionCall) goja.Value {
		comp := &UIComponent{Type: "tabs", Children: make([]*UIComponent, 0)}
		obj := vm.NewObject()

		obj.Set("setCurrentIndex", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 0 {
				comp.Properties["current_index"] = call.Argument(0).ToInteger()
			}
			return goja.Undefined()
		})

		obj.Set("addTab", func(call goja.FunctionCall) goja.Value {
			if len(call.Arguments) > 1 {
				widgetObj := call.Argument(0).ToObject(vm)
				title := call.Argument(1).String()

				internal := widgetObj.Get("__internal")
				if internal != nil {
					childComp := internal.Export().(*UIComponent)
					// Wrap child in a tab item to store title
					tabItem := &UIComponent{
						Type:     "tab_item",
						Label:    title,
						Children: []*UIComponent{childComp},
					}
					comp.Children = append(comp.Children, tabItem)
				}
			}
			return goja.Undefined()
		})

		obj.Set("__internal", comp)
		return obj
	})

	// Layouts
	createLayout := func(typeName string) func(goja.FunctionCall) goja.Value {
		return func(call goja.FunctionCall) goja.Value {
			comp := &UIComponent{Type: typeName, Children: make([]*UIComponent, 0)}
			obj := vm.NewObject()

			addWidget := func(call goja.FunctionCall) goja.Value {
				if len(call.Arguments) == 0 {
					return goja.Undefined()
				}
				childObj := call.Argument(0).ToObject(vm)
				internal := childObj.Get("__internal")
				if internal != nil {
					childComp := internal.Export().(*UIComponent)

					// Handle Grid layout params (row, col, rowspan, colspan)
					if typeName == "grid_layout" && len(call.Arguments) > 2 {
						if childComp.Properties == nil {
							childComp.Properties = make(map[string]interface{})
						}
						childComp.Properties["grid_row"] = call.Argument(1).ToInteger()
						childComp.Properties["grid_col"] = call.Argument(2).ToInteger()
						if len(call.Arguments) > 3 {
							childComp.Properties["grid_rowspan"] = call.Argument(3).ToInteger()
						}
						if len(call.Arguments) > 4 {
							childComp.Properties["grid_colspan"] = call.Argument(4).ToInteger()
						}
					}

					comp.Children = append(comp.Children, childComp)
				}
				return goja.Undefined()
			}

			obj.Set("add", addWidget)
			obj.Set("addWidget", addWidget)
			obj.Set("addLayout", addWidget) // Nested layouts treated as widgets
			obj.Set("addStretch", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

			obj.Set("__internal", comp)
			return obj
		}
	}

	form.Set("create_vlayout", createLayout("v_layout"))
	form.Set("create_hlayout", createLayout("h_layout"))
	form.Set("create_gridlayout", createLayout("grid_layout"))

	// form.create_container()
	form.Set("create_container", func(call goja.FunctionCall) goja.Value {
		obj := vm.NewObject()

		// container.put(id, widget)
		obj.Set("put", func(call goja.FunctionCall) goja.Value {
			id := call.Argument(0).String()
			widgetObj := call.Argument(1).ToObject(vm)
			internal := widgetObj.Get("__internal")
			if internal != nil {
				comp := internal.Export().(*UIComponent)
				comp.ID = id
				schema.Fields[id] = comp
			}
			return goja.Undefined()
		})

		// form.connect(sender, signal, slot)
		form.Set("connect", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

		obj.Set("__is_container", true)
		return obj
	})

	form.Set("connect", func(goja.FunctionCall) goja.Value { return goja.Undefined() })

	vm.Set("form", form)

	// Execute Script
	_, err := vm.RunString(scriptContent)
	if err != nil {
		return nil, fmt.Errorf("script execution failed: %v", err)
	}

	// Call Entry Function (ListenerUI / GenerateUI)
	entryFn, ok := goja.AssertFunction(vm.Get(entryFunc))
	if !ok {
		return nil, fmt.Errorf("function %s not found", entryFunc)
	}

	// Convert args to goja values
	var jsArgs []goja.Value
	for _, arg := range args {
		jsArgs = append(jsArgs, vm.ToValue(arg))
	}

	// Execute
	resVal, err := entryFn(goja.Undefined(), jsArgs...)
	if err != nil {
		return nil, fmt.Errorf("entry function execution failed: %v", err)
	}

	// Extract result
	resObj := resVal.ToObject(vm)

	// Check ui_panel (The visual tree)
	uiPanel := resObj.Get("ui_panel")
	if uiPanel != nil && !goja.IsUndefined(uiPanel) {
		internal := uiPanel.ToObject(vm).Get("__internal")
		if internal != nil {
			schema.Root = internal.Export().(*UIComponent)
		}
	}

	// Check dimensions
	if h := resObj.Get("ui_height"); h != nil {
		schema.Height = int(h.ToInteger())
	}
	if w := resObj.Get("ui_width"); w != nil {
		schema.Width = int(w.ToInteger())
	}

	return schema, nil
}
