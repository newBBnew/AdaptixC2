# AdaptixC2 MCP Server

MCP (Model Context Protocol) 服务器，为 AI 助手提供 AdaptixC2 操作能力。

## 构建

```bash
cd adaptix_mcp
go build -o adaptix_mcp .
```

## IDE 配置

### Windsurf / Cursor

在 `~/.codeium/windsurf/mcp_config.json` 或对应 IDE 配置文件中添加：

```json
{
  "mcpServers": {
    "adaptix": {
      "command": "/path/to/adaptixC2_1.0/adaptix_mcp/adaptix_mcp",
      "args": ["-url", "ws://127.0.0.1:9999"]
    }
  }
}
```

## 工具列表

| 工具 | 描述 |
|------|------|
| `list_agents` | 列出所有在线 Agent |
| `get_agent_info` | 获取 Agent 详细信息 |
| `execute_command` | 在 Agent 上执行命令 |
| `get_console_output` | 获取控制台输出 |
| `clear_console` | 清空控制台 |
| `list_tasks` | 列出任务 |
| `get_task_output` | 获取任务输出 |
| `delete_tasks` | 删除任务 |
| `list_listeners` | 列出监听器 |
| `list_tunnels` | 列出隧道 |
| `list_targets` | 列出目标 |
| `list_pivots` | 列出 Pivot |
| `list_collected_data` | 列出凭据/下载/截图 |
| `update_agent_config` | 更新 sleep/jitter |
| `update_agent_metadata` | 更新 tag/mark |

## 架构

```
AI (Claude/GPT)
    ↓ stdio (JSON-RPC)
adaptix_mcp (Go)
    ↓ WebSocket (ws://127.0.0.1:9999)
AdaptixClient (Qt/C++)
    ↓ WebSocket
AdaptixServer
```

## 使用流程

1. 启动 AdaptixServer
2. 启动 AdaptixClient 并登录同步
3. Client 自动在 9999 端口启动 MCP Bridge
4. IDE 通过 MCP 协议调用 adaptix_mcp
5. AI 即可操作 AdaptixC2
