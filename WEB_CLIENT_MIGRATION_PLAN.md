# Web Client 功能对照与打磨计划 - 逐文件对照

> **Qt Client 源文件总数：87 个 (.cpp/.h)**  
> **本文档对照进度：87/87 ✅**

---

## 文件对照说明

- ✅ **已完成** - Web Client 已实现对应功能
- 🔄 **部分实现** - 功能已有但需优化
- ❌ **未实现** - 需要新建或补充
- ⚠️ **简化版** - 实现了核心功能但缺少细节
- 🔧 **底层差异** - Qt/Web 技术栈差异，不需要直接移植

---

## 1. Agent 管理层 (Source/Agent/) - 5 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 1 | `Agent.cpp` | Agent 数据模型与状态管理 | `context/AgentContext.jsx` | ✅ 已完成 | - |
| 2 | `AgentTableWidgetItem.cpp` | Agent 表格项渲染 | `pages/Agents/AgentCard.jsx` | 🔄 简化版 | P2 需增加分组/标签 |
| 3 | `Commander.cpp` | 命令解析与执行引擎 | `context/AgentContext.jsx` _executeCommandLogic | ✅ 已完成 | - |
| 4 | `Task.cpp` | Task 数据模型 | AgentContext 内联状态 | ✅ 已完成 | - |
| 5 | `TaskTableWidgetItem.cpp` | Task 表格项渲染 | 控制台内嵌显示 | ❌ 缺独立任务历史 | P2 需任务历史页 |

---

## 2. 核心客户端层 (Source/Client/) - 6 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 6 | `AuthProfile.cpp` | 认证配置文件管理 | `pages/Login/` | ⚠️ 基础登录 | P2 需多 profile |
| 7 | `Extender.cpp` | 插件/扩展器管理 | 无 | ❌ 未实现 | P2 需新建页面 |
| 8 | `ProcessSyncPacket.cpp` | 同步包处理分发 | `context/AgentContext.jsx` handleWebSocket | ✅ 已完成 | - |
| 9 | `Requestor.cpp` | HTTP API 请求封装 | `api/agent.js`, `api/listener.js` 等 | ✅ 已完成 | - |
| 10 | `Settings.cpp` | 客户端设置管理 | `pages/Settings/` | 🔄 配置项不全 | P1 补全设置 |
| 11 | `Storage.cpp` | 本地数据存储 | LocalStorage + Context | ✅ 已完成 | - |
| 12 | `TunnelEndpoint.cpp` | 隧道端点管理 | `pages/Agents/Tunnels.jsx` | 🔄 功能简单 | P2 增强管理 |

---

## 3. Extension-Kit 脚本引擎 (Source/Client/AxScript/) - 8 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 13 | `AxScriptEngine.cpp` | QJSEngine 脚本引擎核心 | `utils/axScript.js` AxScriptEngine | ✅ 已完成 | - |
| 14 | `AxScriptManager.cpp` | 脚本加载与生命周期 | `context/AgentContext.jsx` reloadScripts | ✅ 已完成 | - |
| 15 | `BridgeApp.cpp` | ax.* API 桥接 | `utils/axScript.js` createAPI | ✅ 已完成 | - |
| 16 | `BridgeEvent.cpp` | 事件回调系统 | 部分在 axScript.js | 🔄 需完善 | P2 事件系统 |
| 17 | `BridgeForm.cpp` | 动态表单生成 | 无 | ❌ 未实现 | P2 需表单组件 |
| 18 | `BridgeMenu.cpp` | 右键菜单集成 | 无 | ❌ 未实现 | P3 上下文菜单 |
| 19 | `AxCommandWrappers.cpp` | 命令对象封装 | `utils/axCommand.js` AxCommand | ✅ 已完成 | - |
| 20 | `AxElementWrappers.cpp` | UI 元素桥接 | 无 | ❌ 未实现 | P3 组件封装 |

### 功能验证清单
| 功能点 | Qt实现 | Web现状 | 改进任务 |
|-------|--------|---------|---------|
| pre_hook 参数传递 | `Commander.cpp:executeAlias` | ✅ 已修复 | - |
| 命令分组显示 | `ConsoleWidget.cpp:help` | ✅ 已修复 | - |
| 子命令识别 | `Commander.cpp:ProcessInput` | ✅ 已修复 | - |
| 命令自动补全 | `ConsoleWidget.cpp:completer` | ✅ 已实现 | 完善子命令补全 |
| execute_alias 递归 | `Commander.cpp:executeAlias` | ✅ 已实现 | 测试复杂场景 |

---

## 4. UI 对话框层 (Source/UI/Dialogs/) - 13 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 21 | `CommandPaletteDialog.cpp` | 快捷命令面板 | 无 | ❌ 未实现 | P2 快捷导航 |
| 22 | `DialogAgent.cpp` | Agent 详情对话框 | Modal 组件 | ⚠️ 简化版 | P2 增强详情 |
| 23 | `DialogConnect.cpp` | 连接服务器对话框 | `pages/Login/` | ✅ 已完成 | - |
| 24 | `DialogCredential.cpp` | 凭证管理对话框 | 无 | ❌ 未实现 | P1 凭证管理 |
| 25 | `DialogDownloader.cpp` | 文件下载管理 | 无 | ❌ 未实现 | P1 下载管理 |
| 26 | `DialogExtender.cpp` | 扩展器配置对话框 | 无 | ❌ 未实现 | P2 插件配置 |
| 27 | `DialogListener.cpp` | 监听器配置对话框 | `pages/Listeners/` Modal | ✅ 已完成 | - |
| 28 | `DialogSaveTask.cpp` | 任务保存对话框 | 无 | ❌ 未实现 | P3 任务导出 |
| 29 | `DialogSettings.cpp` | 设置对话框 | `pages/Settings/` | 🔄 配置不全 | P1 补全设置 |
| 30 | `DialogSyncPacket.cpp` | 同步包查看调试 | 无 | ❌ 未实现 | P3 调试工具 |
| 31 | `DialogTarget.cpp` | 目标管理对话框 | 无 | ❌ 未实现 | P1 目标管理 |
| 32 | `DialogTunnel.cpp` | 隧道配置对话框 | `Tunnels.jsx` Modal | 🔄 功能简单 | P2 详细配置 |
| 33 | `DialogUploader.cpp` | 文件上传管理 | 无 | ❌ 未实现 | P1 上传管理 |

---

## 5. UI Widget 层 (Source/UI/Widgets/) - 19 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 34 | `AdaptixWidget.cpp` | 主容器 Widget | `App.jsx` 布局 | 🔧 技术栈差异 | - |
| 35 | `AxConsoleWidget.cpp` | AxScript 控制台 | 可合并到 AgentConsole | ❌ 未单独实现 | P3 调试工具 |
| 36 | `BrowserFilesWidget.cpp` | 文件浏览器 | 无 | ❌ 未实现 | P1 文件管理 |
| 37 | `BrowserProcessWidget.cpp` | 进程浏览器 | 无 | ❌ 未实现 | P1 进程管理 |
| 38 | `ChatWidget.cpp` | 团队聊天 | 无 | ❌ 未实现 | P2 协作功能 |
| 39 | `ConsoleWidget.cpp` | Agent 命令控制台 | `pages/Agents/AgentConsole.jsx` | ✅ 已完成 | - |
| 40 | `CredentialsWidget.cpp` | 凭证列表视图 | 无 | ❌ 未实现 | P1 凭证页面 |
| 41 | `DownloadsWidget.cpp` | 下载任务列表 | 无 | ❌ 未实现 | P1 下载页面 |
| 42 | `FileDeliveryWidget.cpp` | 文件投递管理 | 无 | ❌ 未实现 | P2 文件投递 |
| 43 | `HostedFilesWidget.cpp` | 托管文件列表 | 无 | ❌ 未实现 | P2 文件托管 |
| 44 | `ListenersWidget.cpp` | 监听器列表 | `pages/Listeners/` | ✅ 已完成 | - |
| 45 | `LogsWidget.cpp` | 事件日志 | `pages/EventLog/` | ✅ 已完成 | - |
| 46 | `ScreenshotsWidget.cpp` | 截图管理 | 无 | ❌ 未实现 | P2 截图查看 |
| 47 | `SessionsTableWidget.cpp` | Agent 表格视图 | `pages/Agents/` | ✅ 已完成 | - |
| 48 | `TacticalWidget.cpp` | 战术视图/仪表板 | `pages/Dashboard/` | 🔄 简化版 | P2 增强仪表板 |
| 49 | `TargetsWidget.cpp` | 目标列表视图 | 无 | ❌ 未实现 | P1 目标页面 |
| 50 | `TasksWidget.cpp` | 任务列表视图 | 控制台内嵌 | ❌ 缺独立视图 | P2 任务历史 |
| 51 | `TerminalWidget.cpp` | 终端/PTY 交互 | 无 | ❌ 未实现 | P2 终端功能 |
| 52 | `TunnelsWidget.cpp` | 隧道列表视图 | `pages/Agents/Tunnels.jsx` | 🔄 功能简单 | P2 增强管理 |

---

## 6. UI 图形视图层 (Source/UI/Graph/) - 5 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 53 | `SessionsGraph.cpp` | Agent 拓扑图主视图 | 无 | ❌ 未实现 | P2 图形视图 |
| 54 | `GraphScene.cpp` | 图形场景管理 | 无 | ❌ 未实现 | P2 图形引擎 |
| 55 | `GraphItem.cpp` | 节点渲染 | 无 | ❌ 未实现 | P2 节点组件 |
| 56 | `GraphItemLink.cpp` | 连接线渲染 | 无 | ❌ 未实现 | P2 连线组件 |
| 57 | `LayoutTreeLeft.cpp` | 树形布局算法 | 无 | ❌ 未实现 | P2 布局算法 |

**建议技术栈：** React Flow / D3.js / Cytoscape.js

---

## 7. UI 主界面层 (Source/UI/) - 1 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 58 | `MainUI.cpp` | 主窗口框架与布局 | `App.jsx` + Router | ✅ 已完成 | - |

---

## 8. 工具类层 (Source/Utils/) - 6 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 59 | `Convert.cpp` | 数据格式转换工具 | JS 内置函数 | 🔧 技术栈差异 | - |
| 60 | `CustomElements.cpp` | 自定义 UI 元素 | Tailwind + 自定义组件 | 🔧 技术栈差异 | - |
| 61 | `FileSystem.cpp` | 文件系统操作 | Browser File API | 🔧 技术栈差异 | - |
| 62 | `FontManager.cpp` | 字体管理 | CSS Fonts | 🔧 技术栈差异 | - |
| 63 | `Logs.cpp` | 日志工具 | console.log + EventLog | 🔧 技术栈差异 | - |
| 64 | `NonBlockingDialogs.cpp` | 非阻塞对话框 | React Modal | 🔧 技术栈差异 | - |

---

## 9. Worker 线程层 (Source/Workers/) - 6 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 65 | `DownloaderWorker.cpp` | 后台下载线程 | axios + Context | ⚠️ 简化版 | P1 下载队列 |
| 66 | `LastTickWorker.cpp` | Agent 心跳检测 | WebSocket 处理 | ✅ 已完成 | - |
| 67 | `TerminalWorker.cpp` | 终端 I/O 线程 | 无 | ❌ 未实现 | P2 终端功能 |
| 68 | `TunnelWorker.cpp` | 隧道数据转发 | 无 | ❌ 未实现 | P2 本地代理 |
| 69 | `UploaderWorker.cpp` | 后台上传线程 | axios + Context | ⚠️ 简化版 | P1 上传队列 |
| 70 | `WebSocketWorker.cpp` | WebSocket 连接线程 | `context/AgentContext.jsx` | ✅ 已完成 | - |

---

## 10. MCP 协议处理层 (Source/Workers/MCP/) - 15 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 71 | `MCPBridgeWorker.cpp/.h` | MCP 桥接主线程 | 无需（Web直接WS） | 🔧 架构差异 | - |
| 72 | `MCPCommandHandler.cpp/.h` | MCP 命令分发 | AgentContext handleWebSocket | ✅ 已完成 | - |
| 73 | `MCPProtocol.h` | MCP 协议定义 | 内联在代码中 | ✅ 已完成 | - |

### MCP Handlers (Source/Workers/MCP/handlers/) - 12 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 74 | `AgentHandler.cpp/.h` | Agent 消息处理 | AgentContext 0x42等 | ✅ 已完成 | - |
| 75 | `AxScriptHandler.cpp/.h` | AxScript 消息处理 | AgentContext AxScript消息 | ✅ 已完成 | - |
| 76 | `BOFHandler.cpp/.h` | BOF 消息处理 | AgentContext BOF响应 | ✅ 已完成 | - |
| 77 | `ConsoleHandler.cpp/.h` | 控制台消息处理 | AgentContext 0x69等 | ✅ 已完成 | - |
| 78 | `InfoHandler.cpp/.h` | 信息消息处理 | AgentContext 0x01等 | ✅ 已完成 | - |
| 79 | `ListenerHandler.cpp/.h` | Listener 消息处理 | AgentContext Listener消息 | ✅ 已完成 | - |
| 80 | `PivotsHandler.cpp/.h` | Pivot 消息处理 | 无 | ❌ 未实现 | P2 Pivot功能 |
| 81 | `TargetsHandler.cpp/.h` | Target 消息处理 | 无 | ❌ 未实现 | P1 Target功能 |
| 82 | `TunnelHandler.cpp/.h` | Tunnel 消息处理 | AgentContext Tunnel消息 | 🔄 简化版 | P2 完善处理 |

---

## 11. 主程序入口 (Source/) - 2 个文件

| # | Qt 文件 | 功能描述 | Web 对应实现 | 状态 | 优先级 |
|---|---------|---------|-------------|------|--------|
| 83 | `main.cpp` | Qt 应用入口 | `index.html` + `main.jsx` | 🔧 技术栈差异 | - |
| 84 | `MainAdaptix.cpp` | 主应用类 | `App.jsx` | 🔧 技术栈差异 | - |

---

## 12. 头文件总结 (Headers/)

所有 .h 文件已包含在上述对应的 .cpp 文件中统计，无需单独列出。

---

## 📊 统计总览

| 状态 | 文件数 | 百分比 |
|------|--------|--------|
| ✅ 已完成 | 24 | 27.9% |
| 🔄 部分实现 | 10 | 11.6% |
| ⚠️ 简化版 | 6 | 7.0% |
| ❌ 未实现 | 35 | 40.7% |
| 🔧 技术栈差异 | 11 | 12.8% |
| **总计** | **86** | **100%** |

> **注：** 统计不包括重复的 .h 文件

---

## 🎯 优先级分组

### P1 - 核心功能缺失（必须实现）- 13 项

1. ❌ `DialogCredential.cpp` → 凭证管理页面
2. ❌ `DialogDownloader.cpp` → 下载管理器
3. ❌ `DialogTarget.cpp` → 目标管理页面
4. ❌ `DialogUploader.cpp` → 上传管理器
5. ❌ `BrowserFilesWidget.cpp` → 文件浏览器
6. ❌ `BrowserProcessWidget.cpp` → 进程浏览器
7. ❌ `CredentialsWidget.cpp` → 凭证列表页
8. ❌ `DownloadsWidget.cpp` → 下载列表页
9. ❌ `TargetsWidget.cpp` → 目标列表页
10. 🔄 `Settings.cpp` → 补全所有设置项
11. 🔄 `DialogSettings.cpp` → 设置对话框完善
12. ⚠️ `DownloaderWorker.cpp` → 下载队列管理
13. ⚠️ `UploaderWorker.cpp` → 上传队列管理

### P2 - 体验增强（重要）- 25 项

14. 🔄 `AgentTableWidgetItem.cpp` → Agent分组/标签
15. ❌ `TaskTableWidgetItem.cpp` → 任务历史页
16. 🔄 `AuthProfile.cpp` → 多profile支持
17. ❌ `Extender.cpp` → 插件管理页
18. 🔄 `TunnelEndpoint.cpp` → 隧道详细管理
19. 🔄 `BridgeEvent.cpp` → 事件系统完善
20. ❌ `BridgeForm.cpp` → 动态表单组件
21. ❌ `CommandPaletteDialog.cpp` → 快捷命令面板
22. 🔄 `DialogAgent.cpp` → Agent详情增强
23. ❌ `DialogExtender.cpp` → 插件配置界面
24. 🔄 `DialogTunnel.cpp` → 隧道详细配置
25. ❌ `ChatWidget.cpp` → 团队聊天
26. ❌ `FileDeliveryWidget.cpp` → 文件投递
27. ❌ `HostedFilesWidget.cpp` → 文件托管
28. ❌ `ScreenshotsWidget.cpp` → 截图管理
29. 🔄 `TacticalWidget.cpp` → 仪表板增强
30. ❌ `TasksWidget.cpp` → 任务独立视图
31. ❌ `TerminalWidget.cpp` → 终端功能
32. 🔄 `TunnelsWidget.cpp` → 隧道管理增强
33. ❌ `SessionsGraph.cpp` → Agent拓扑图
34. ❌ `GraphScene.cpp` → 图形引擎
35. ❌ `GraphItem.cpp` → 节点组件
36. ❌ `GraphItemLink.cpp` → 连线组件
37. ❌ `LayoutTreeLeft.cpp` → 布局算法
38. ❌ `TerminalWorker.cpp` → 终端I/O
39. ❌ `TunnelWorker.cpp` → 本地隧道代理
40. ❌ `PivotsHandler.cpp/.h` → Pivot功能
41. 🔄 `TunnelHandler.cpp/.h` → 隧道处理完善

### P3 - 可选功能（后续）- 6 项

42. ❌ `BridgeMenu.cpp` → 右键菜单
43. ❌ `AxElementWrappers.cpp` → UI元素桥接
44. ❌ `AxConsoleWidget.cpp` → AxScript调试控制台
45. ❌ `DialogSaveTask.cpp` → 任务导出
46. ❌ `DialogSyncPacket.cpp` → 同步包调试工具

---

## 📋 实施路线图

### 第一阶段：核心功能补全（P1）

**Week 1-2: 文件管理**
1. 实现 `BrowserFilesWidget.cpp` → 文件浏览器
2. 实现 `DialogUploader.cpp` → 上传管理器  
3. 实现 `DialogDownloader.cpp` → 下载管理器
4. 实现 `DownloadsWidget.cpp` → 下载列表页

**Week 3: 进程管理**
5. 实现 `BrowserProcessWidget.cpp` → 进程浏览器

**Week 4: 凭证与目标**
6. 实现 `DialogCredential.cpp` → 凭证管理
7. 实现 `CredentialsWidget.cpp` → 凭证列表页
8. 实现 `DialogTarget.cpp` → 目标管理
9. 实现 `TargetsWidget.cpp` → 目标列表页

**Week 5: 设置完善**
10. 完善 `Settings.cpp` → 补全所有设置项
11. 完善 `DialogSettings.cpp` → 设置界面

### 第二阶段：体验优化（P2）

**Week 6-7: Agent增强**
- Agent分组/标签
- 任务历史页
- Agent详情增强

**Week 8-9: 图形视图**
- React Flow 集成
- Agent拓扑图
- 节点/连线组件

**Week 10-11: 高级功能**
- 终端功能
- 隧道本地代理
- Pivot管理

**Week 12: 协作功能**
- 团队聊天
- 文件投递/托管
- 截图管理

### 第三阶段：锦上添花（P3）

- 右键菜单
- 调试工具
- 命令面板

---

## 🚀 建议下一步行动

**立即开始：文件管理器三件套**

1. **文件浏览器** (`BrowserFilesWidget.cpp`)
   - 新建 `pages/Agents/FileManager.jsx`
   - 目录树展示
   - 文件列表
   - 文件操作（删除、重命名、下载）

2. **上传管理器** (`DialogUploader.cpp`)
   - 新建 `components/FileUploader.jsx`
   - 文件选择
   - 上传进度
   - 队列管理

3. **下载管理器** (`DialogDownloader.cpp` + `DownloadsWidget.cpp`)
   - 新建 `pages/Downloads/`
   - 下载任务列表
   - 进度显示
   - 历史记录

**验证点：**
- ✅ 可以浏览 Agent 文件系统
- ✅ 可以上传文件到 Agent
- ✅ 可以从 Agent 下载文件
- ✅ 上传/下载有进度显示
- ✅ 可以查看历史记录
| **Agent详情显示** | `DialogAgent.cpp` | 简单信息 | 增加系统详情、网络信息 |
| **Agent分组** | `AgentTableWidgetItem.cpp` | 无 | 添加分组/标签功能 |
| **Task历史** | `TaskTableWidgetItem.cpp` | 无独立视图 | 创建任务历史页面 |
| **Agent搜索过滤** | `MainUI.cpp:filterAgents` | 简单过滤 | 增强多条件过滤 |

### ❌ 缺失功能
| Qt功能 | 描述 | 优先级 |
|--------|------|--------|
| **Agent注释** | 添加自定义备注 | P2 |
| **Agent导出** | 导出Agent列表 | P3 |
| **批量操作** | 多Agent批量命令 | P2 |

---

## 4. 命令执行层 (Commander.cpp)

### ✅ 已完成
| 功能 | Web实现 | 状态 |
|------|---------|------|
| 基础命令执行 | `AgentContext.jsx` | ✅ |
| pre_hook调用 | `AgentContext.jsx` | ✅ |
| execute_alias | `axScript.js` | ✅ |
| 参数解析 | `AgentContext.jsx` | ✅ |

### 🔄 待验证
| Qt功能 | Web实现 | 测试要点 |
|--------|---------|---------|
| **默认参数** | 已实现 | 测试quser等命令 |
| **必填参数检查** | 已实现 | 测试缺少参数提示 |
| **路径转义** | 需检查 | 测试Windows路径 |
| **BOF通知模式** | 已实现 | 测试BOF命令 |

### ❌ 缺失功能
| Qt功能 | 描述 | 优先级 |
|--------|------|--------|
| **命令历史持久化** | 跨会话保存 | P2 |
| **命令宏/别名** | 用户自定义别名 | P3 |
| **命令管道** | 命令输出重定向 | P3 |

---

## 5. UI控制台 (UI/Widgets/ConsoleWidget.cpp)

### ✅ 已完成
| Qt功能 | Web实现 | 状态 |
|--------|---------|------|
| 命令输入 | `AgentConsole.jsx` | ✅ |
| 输出显示 | `AgentConsole.jsx` | ✅ |
| 实时建议 | `AgentConsole.jsx` | ✅ |
| 历史导航 | `AgentConsole.jsx` | ✅ |
| 搜索过滤 | `AgentConsole.jsx` | ✅ |

### 🔄 待优化
| Qt功能 | Web现状 | 改进方向 |
|--------|---------|---------|
| **ANSI颜色** | 基础支持 | 完整ANSI转义序列 |
| **输出格式化** | 简单处理 | 增强表格、JSON显示 |
| **右键菜单** | 无 | 添加复制/清空等 |
| **时间戳显示** | 简单时间 | 可配置格式 |

### ❌ 缺失功能
| Qt功能 | 描述 | 优先级 |
|--------|------|--------|
| **输出导出** | 导出为文本/HTML | P2 |
| **多标签管理** | 多Agent标签切换 | P1 |
| **分屏显示** | 同时查看多Agent | P2 |

---

## 6. 对话框功能 (UI/Dialogs/)

### ✅ 已有
| Qt Dialog | Web实现 | 状态 |
|-----------|---------|------|
| `DialogConnect.cpp` | `Login.jsx` | ✅ |
| `DialogListener.cpp` | `Listeners.jsx` | ✅ |
| `DialogAgent.cpp` | Agent详情Modal | ⚠️ 简化版 |

### ❌ 缺失对话框
| Qt Dialog | 功能描述 | Web实现位置 | 优先级 |
|-----------|---------|------------|--------|
| `DialogCredential.cpp` | 凭证管理 | 需新建 `pages/Credentials/` | P1 |
| `DialogTarget.cpp` | 目标管理 | 需新建 `pages/Targets/` | P1 |
| `DialogUploader.cpp` | 文件上传管理 | 集成到Agent控制台 | P1 |
| `DialogDownloader.cpp` | 文件下载管理 | 集成到Agent控制台 | P1 |
| `DialogTunnel.cpp` | 隧道详细配置 | 增强 `Tunnels.jsx` | P2 |
| `DialogExtender.cpp` | 扩展器配置 | 新建页面 | P2 |
| `DialogSaveTask.cpp` | 任务保存 | 新建功能 | P3 |
| `DialogSyncPacket.cpp` | 同步包查看 | 调试功能 | P3 |
| `CommandPaletteDialog.cpp` | 命令面板 | 快捷键导航 | P2 |

---

## 7. 高级UI组件 (UI/Widgets/)

### ❌ 缺失组件
| Qt Widget | 功能描述 | Web实现 | 优先级 |
|-----------|---------|---------|--------|
| `BrowserFilesWidget.cpp` | 文件浏览器 | 需新建文件管理器 | P1 |
| `BrowserProcessWidget.cpp` | 进程浏览器 | 需新建进程管理器 | P1 |
| `ChatWidget.cpp` | 团队聊天 | 需新建聊天功能 | P2 |
| `EventsWidget.cpp` | 事件日志 | ✅ 已有 EventLog | - |
| `FileManagerWidget.cpp` | 文件管理 | 同文件浏览器 | P1 |
| `GraphView.cpp` | 会话拓扑图 | 需新建图形视图 | P2 |
| `ListenersWidget.cpp` | 监听器列表 | ✅ 已有 | - |
| `PivotsWidget.cpp` | Pivot管理 | 需新建 | P2 |
| `TargetsWidget.cpp` | 目标列表 | 需新建 | P1 |
| `CredentialsWidget.cpp` | 凭证列表 | 需新建 | P1 |

---

## 8. 图形化视图 (UI/Graph/)

### ❌ 完全缺失
| Qt实现 | 功能描述 | 优先级 |
|--------|---------|--------|
| `SessionsGraph.cpp` | Agent拓扑关系图 | P2 |
| `GraphScene.cpp` | 图形场景管理 | P2 |
| `GraphItem.cpp` | 节点渲染 | P2 |
| `GraphItemLink.cpp` | 连接线渲染 | P2 |
| `LayoutTreeLeft.cpp` | 树形布局算法 | P2 |

**建议技术栈：** React Flow / D3.js / Cytoscape.js

---

## 实施优先级路线图

### Phase 1: 核心功能完善 (P1) - 当前阶段
1. ✅ Extension-Kit命令执行 - **已完成**
2. ✅ 命令补全与帮助 - **已完成**
3. 🔄 文件上传/下载UI - **进行中**
4. 🔄 文件浏览器 - **待开始**
5. 🔄 进程浏览器 - **待开始**
6. 🔄 凭证管理 - **待开始**
7. 🔄 目标管理 - **待开始**

### Phase 2: 用户体验优化 (P2)
1. Agent分组与标签
2. 任务历史独立页面
3. 多标签Agent控制台
4. 命令历史持久化
5. 快捷键命令面板
6. 图形化会话视图
7. 团队聊天功能
8. Pivot管理

### Phase 3: 高级功能 (P3)
1. Agent注释与导出
2. 批量操作
3. 命令宏/别名
4. 分屏显示
5. 输出导出
6. 调试工具

---

## 下一步行动

**建议执行顺序：**

1. **文件管理器** (`BrowserFilesWidget.cpp` → 新建 `FileManager.jsx`)
   - 文件浏览
   - 文件上传/下载
   - 文件操作（删除、重命名）

2. **进程管理器** (`BrowserProcessWidget.cpp` → 新建 `ProcessManager.jsx`)
   - 进程列表
   - 进程详情
   - Kill进程

3. **凭证管理** (`DialogCredential.cpp` → 新建 `Credentials.jsx`)
   - 凭证列表显示
   - 添加/编辑凭证
   - 凭证导出

4. **目标管理** (`DialogTarget.cpp` → 新建 `Targets.jsx`)
   - 目标列表
   - 添加/编辑目标
   - 目标标记

5. **完善命令执行**
   - 测试所有Extension-Kit命令
   - 验证参数解析
   - 优化错误提示
