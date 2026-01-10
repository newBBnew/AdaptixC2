# Adaptix C2 战术工作流模块 (Tactical Workflow) 开发文档

## 1. 模块概览与设计理念

本模块旨在为 AdaptixClient (Qt) 提供高度灵活、可视化且具备 AI 协作能力的积木式战术编排系统。通过 4 栏式布局，实现从命令积累到复杂工作流执行的全生命周期管理。

---

## 2. 核心 4 栏布局设计

### 第一栏：命令知识库 (Command Library)
*   **展现形式**：树形结构，支持按战术意图多级分类（如：侦察类、数据收集、凭据获取、提权、持久化等）。
*   **管理能力**：
    *   **全量管理**：支持对基础命令、系统注册命令（Extenders/Scripts）的增删改查。
    *   **命令变体 (Variants)**：单条意图命令下可展开显示多种变体（针对不同 OS 版本、不同 EDR 环境的实现）。
    *   **详细备注**：每条命令支持配置详细的执行说明、风险等级与 OPSEC 建议。
*   **交互逻辑**：支持将单条命令或变体拖拽至第二栏进行编排。

### 第二栏：工作流编排器 (Workflow Composer)
*   **展现形式**：树形结构，按任务名称或战术目标对工作流进行分类管理。
*   **核心功能**：
    *   **组合与排序**：对从第一栏拖入的积木进行顺序调整、嵌套组合。
    *   **精细执行控制**：支持勾选执行工作流中的单条、一组或组内某几条特定命令。
    *   **多目标下发 (1-to-N)**：支持选择一个或多个 Agent 作为执行目标。
*   **结果分发**：执行动作发起后，其状态与实时输出将自动定向至第三栏。

### 第三栏：执行结果监控 (Execution Results)
*   **视图逻辑**：
    *   **分组展示**：默认按 Agent 或目标计算机 ID 进行分组，清晰展示不同目标的执行进度。
    *   **联动回溯**：点击结果可自动定位并高亮第二栏中对应的命令节点。
    *   **流式输出**：实时呈现命令的原始回显与解析后的结构化数据。

### 第四栏：AI/MCP 协作空间 (AI/MCP Collaboration)
*   **核心定位**：AI 副驾驶 (Co-Pilot) 的深度集成环境。
*   **可见性 (Read)**：AI 拥有对系统状态的深度感知，包括：
    *   读取命令库内容及其注释说明。
    *   感知实时 Agent 元数据与环境分析结果。
    *   监控当前的命令编排组与实时执行反馈。
*   **操作性 (Write)**：AI 通过 MCP (Model Context Protocol) 参与实战：
    *   **动态编排**：支持 AI 直接编辑命令库或优化第二栏的工作流序列。
    *   **决策执行**：AI 基于执行结果自动调整后续战术，并能直接发起执行请求。
    *   **注释增强**：AI 自动根据实战结果为命令库补充风险备注。

---

## 3. 技术实现细节

### 3.1 同步包与数据格式 (SyncPacket)

为了在 Teamserver 与 Qt 客户端之间传输战术数据，引入以下同步包：

#### TYPE_TACTICAL_CATALOG_SYNC (0xA1)
用于将服务端的 YAML 命令库同步到第一栏。
```json
{
  "type": 161,
  "action": "sync_all",
  "categories": [
    {
      "name": "Reconnaissance",
      "blocks": [
        {
          "id": "win_edr_detect",
          "name": "EDR Detection",
          "variants": [
            {
              "id": "v1_tasklist",
              "name": "Tasklist Method",
              "cmd": "shell tasklist /v",
              "os": 1, // OS_WINDOWS
              "risk": 1
            }
          ]
        }
      ]
    }
  ]
}
```

#### TYPE_TACTICAL_WORKFLOW_SYNC (0xA2)
用于同步/保存第二栏的编排状态。
```json
{
  "type": 162,
  "workflow_id": "wf_initial_triage",
  "steps": [
    {"node_instance_id": "uuid_1", "block_id": "win_edr_detect", "variant_id": "v1_tasklist", "params": {}}
  ],
  "target_agents": ["agent_1", "agent_2"]
}
```

### 3.2 积木知识库 YAML 格式示例

```yaml
id: "recon_edr_detect"
name: "EDR 探测"
category: "Reconnaissance"
description: "识别目标机器上的防御软件。"
variants:
  - id: "win_tasklist_v"
    name: "标准进程枚举"
    os: "windows"
    template: "shell tasklist /v"
    risk: 1
    ai_guidance: "新 Agent 上线后的第一步建议，用于识别 Defender/CrowdStrike 等。"
```

### 3.3 AI/MCP 协作接口 (Column 4)

AI 通过以下 MCP Tool 操控战术系统：
- `tactical.get_library`: 获取全量命令库。
- `tactical.modify_workflow`: 修改编排序列。
- `tactical.execute_sequence`: 发起执行请求。
- `tactical.read_results`: 获取实时反馈流。

---

## 4. 后续演进 (Roadmap)

*   从列表式步骤演进为 Scratch 式的可视化连线图。
*   支持节点间的变量传递（如第一步获取的 PID 自动填入第二步的注入参数）。
*   引入更复杂的 AI 自动化触发器（如：发现特定进程时自动激活规避工作流）。
