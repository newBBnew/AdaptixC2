# AdaptixC2 AI Resident Mode Demo Guide

This guide describes how to demonstrate the "AI Resident in Team Chat" functionality using the AdaptixC2 MCP integration.

## Architecture Overview

1.  **AdaptixServer**: The C2 Teamserver handling agents and data.
2.  **AdaptixClient**: The Qt GUI Client used by operators. It contains an `MCPBridgeWorker` that exposes a WebSocket interface.
3.  **adaptix_mcp**: The MCP Server that connects to the Client's Bridge. It acts as the brain/interface for the AI.
4.  **AI Agent (You)**: The LLM connected to `adaptix_mcp` that runs the "Mind-Stone Loop".

## Setup Instructions

1.  **Start AdaptixServer**:
    ```bash
    cd AdaptixServer
    ./adaptix-server
    ```

2.  **Start AdaptixClient**:
    - Launch the Client.
    - Connect to the Teamserver.
    - Ensure the "MCP Bridge" is active (usually starts automatically on port 9999 after sync).

3.  **Start adaptix_mcp**:
    ```bash
    cd adaptix_mcp
    go run main.go -url "ws://127.0.0.1:9999"
    ```
    *Verify connection: You should see "✅ Connected to Client MCP Bridge" in the logs.*

## Demonstration Flow

### Step 1: Activate the AI Resident
As the AI, you should begin your "Listen Loop".
- **Tool Call**: `listen_intelligence`
- **Params**: `{"type": "chat", "timeout": 60, "exclude_user": "Tactical AI"}`

### Step 2: User Interaction (Simulation)
In the AdaptixClient "Chat" tab, an operator types:
> `@AI what listeners are currently running?`

### Step 3: AI Perception & Action
1.  The `listen_intelligence` tool returns the message:
    ```json
    {
      "status": "received",
      "messages": [
        {"username": "operator", "content": "@AI what listeners are currently running?", "timestamp": ...}
      ]
    }
    ```
2.  You analyze the intent: "List Listeners".
3.  You call `look_assets`:
    - **Tool Call**: `look_assets`
    - **Params**: `{"type": "listeners"}`
4.  You receive the listener data.
5.  You respond to the team:
    - **Tool Call**: `speak_interaction`
    - **Params**: `{"action": "team_chat", "content": "Currently active listeners:\n1. **HTTP_1** (Port 80)\n2. **SMB_Internal** (Pipe: msvc)"}`

### Step 4: Interactive Suggestion (Tactical Flash)
In the Chat, operator types:
> `We need to check if the file server is accessible.`

You decide to propose a ping test.
1.  **Tool Call**: `tactical_flash`
2.  **Params**:
    ```json
    {
      "summary": "I can attempt to ping the file server (10.10.10.5) from the current beacon to verify connectivity. Shall I proceed?",
      "timeout_seconds": 60
    }
    ```
3.  This automatically:
    - Broadcasts the question to the chat.
    - Waits for a reply.

Operator replies:
> `Yes`

4.  The tool returns the "Yes" message.
5.  You proceed to execute the ping command.

## Troubleshooting

- **Loop Detection**: If you see your own messages appearing in `listen_intelligence`, ensure `exclude_user` parameter is supported and set to "Tactical AI".
- **Timeout**: The listen tool defaults to 60s timeout. If it times out, simply loop and call it again.
- **Connection**: If `adaptix_mcp` fails to connect, check if AdaptixClient is fully synced (the bridge only starts after sync).
