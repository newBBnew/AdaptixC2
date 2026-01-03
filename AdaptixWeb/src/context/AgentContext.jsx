import React, { createContext, useContext, useState, useEffect, useCallback, useRef } from 'react';
import { useSocket } from './SocketContext';
import { agentApi } from '../api/agent';
import { listenerApi, taskApi, tunnelApi, deliveryApi, dataApi } from '../api/control';
import { PacketType } from '../constants/packetTypes';
import axEngine from '../utils/axScript';

const AgentContext = createContext();

export const useAgents = () => useContext(AgentContext);

export const AgentProvider = ({ children }) => {
  const [agents, setAgents] = useState([]);
  const [listeners, setListeners] = useState([]);
  const [tasks, setTasks] = useState({}); // { taskId: taskData }
  const [logs, setLogs] = useState([]);
  const [credentials, setCredentials] = useState([]);
  const [targets, setTargets] = useState([]);
  const [downloads, setDownloads] = useState([]);
  const [screenshots, setScreenshots] = useState([]);
  const [fileDeliveries, setFileDeliveries] = useState({}); // { fileId: data }
  const [tunnels, setTunnels] = useState([]);
  const [pivots, setPivots] = useState({}); // { pivotId: data }
  const [chatMessages, setChatMessages] = useState([]);
  const [browserData, setBrowserData] = useState({}); // { agentId: { disks: [], files: [], procs: [] } }
  
  const [openTabs, setOpenTabs] = useState(() => {
    try {
      const saved = localStorage.getItem('adaptix_openTabs');
      return saved ? JSON.parse(saved) : [{ id: 'logs', type: 'logs', title: 'Logs' }];
    } catch (e) {
      return [{ id: 'logs', type: 'logs', title: 'Logs' }];
    }
  });
  const [activeTabId, setActiveTabId] = useState(() => {
    return localStorage.getItem('adaptix_activeTabId') || 'logs';
  });

  // Persist openTabs and activeTabId
  useEffect(() => {
    localStorage.setItem('adaptix_openTabs', JSON.stringify(openTabs));
  }, [openTabs]);

  useEffect(() => {
    localStorage.setItem('adaptix_activeTabId', activeTabId);
  }, [activeTabId]);
  const [isDockExpanded, setIsDockExpanded] = useState(true);
  const [consoleHistory, setConsoleHistory] = useState({}); // { agentId: [lines] }
  
  // Buffer limits to prevent UI freezing on large outputs
  const CONSOLE_BUFFER_LIMIT = 1000;
  const LOGS_BUFFER_LIMIT = 500;
  const CHAT_BUFFER_LIMIT = 200;

  const [axCommands, setAxCommands] = useState([]); // Extension-Kit commands
  const [axPlugins, setAxPlugins] = useState([]); // Extension-Kit plugins
  const [axStats, setAxStats] = useState({ loadedScripts: 0, commandCount: 0, pluginCount: 0 });
  const [agentConfigs, setAgentConfigs] = useState({}); // Metadata for agent types
  const [listenerConfigs, setListenerConfigs] = useState({}); // Metadata for listener types
  
  const axInitialized = useRef(false);
  const consoleQueueRef = useRef({});
  const logQueueRef = useRef([]);
  const chatQueueRef = useRef([]);
  const { addListener } = useSocket();

  // --- Extension-Kit Engine Integration ---
  const reloadScripts = useCallback(async () => {
    try {
      axEngine.loadedScripts.clear();
      axEngine.commands.clear();
      axEngine.plugins = [];
      const success = await axEngine.init();
      if (success) {
        await axEngine.loadMainScript();
        console.log(`[AgentContext] Reloaded ${axEngine.commands.size} extension commands`);
        
        // Update stats
        setAxStats({
          loadedScripts: axEngine.loadedScripts.size,
          commandCount: axEngine.commands.size,
          pluginCount: axEngine.plugins.length
        });
        setAxCommands(Array.from(axEngine.commands.values()));
        setAxPlugins(axEngine.plugins);
      }
    } catch (err) {
      console.error('[AgentContext] Failed to reload scripts:', err);
    }
  }, []);

  // Update engine stats whenever commands are updated
  useEffect(() => {
    axEngine.setOnCommandsUpdated(() => {
      setAxStats({
        loadedScripts: axEngine.loadedScripts.size,
        commandCount: axEngine.commands.size,
        pluginCount: axEngine.plugins.length
      });
      setAxCommands(Array.from(axEngine.commands.values()));
      setAxPlugins(axEngine.plugins);
    });
  }, []);

  useEffect(() => {
    if (!axInitialized.current) {
      axInitialized.current = true;
      reloadScripts();
    }
  }, [reloadScripts]);

  // Update engine with current agents for script context
  useEffect(() => {
    axEngine.setAgents(agents.reduce((acc, a) => ({ ...acc, [a.a_id]: a }), {}));
  }, [agents]);

  // Parse command arguments based on command definition from axEngine
  const parseCommandArgs = (cmdDef, argParts, isSubcommand = false) => {
    const result = {};
    if (!cmdDef || !cmdDef.args) return result;
    
    let argIndex = 0;
    const args = cmdDef.args || [];
    
    for (let i = 0; i < argParts.length && argIndex < args.length; i++) {
      const part = argParts[i];
      const argDef = args[argIndex];
      
      // Check if it's a flag argument (starts with -)
      if (part.startsWith('-') && argDef.mark) {
        if (argDef.type.toUpperCase() === 'BOOL' && argDef.mark === part) {
          result[argDef.mark] = true;
          continue;
        }
        // Flag with value
        if (argDef.mark === part && i + 1 < argParts.length) {
          i++;
          result[argDef.name] = argDef.type.toUpperCase() === 'INT' ? parseInt(argParts[i]) : argParts[i];
          argIndex++;
          continue;
        }
      }
      
      // Positional argument
      if (argDef.type.toUpperCase() === 'INT') {
        result[argDef.name] = parseInt(part);
      } else if (argDef.type.toUpperCase() === 'FILE') {
        // In Web, we just pass the path, file content is handled if needed
        result[argDef.name] = part;
      } else {
        // For the last string argument, consume all remaining parts (wide args)
        if (argIndex === args.length - 1 && argDef.type.toUpperCase() === 'STRING') {
          result[argDef.name] = argParts.slice(i).join(' ');
          break;
        }
        result[argDef.name] = part;
      }
      argIndex++;
    }
    
    // Apply default values for missing optional args
    args.forEach(arg => {
      if (!(arg.name in result) && arg.defaultValue !== undefined) {
        result[arg.name] = arg.defaultValue;
      }
    });
    
    return result;
  };

  // Qt-compatible command line parser (unserializeParams)
  const unserializeParams = (cmdline) => {
    const tokens = [];
    let token = '';
    let inQuotes = false;
    const len = cmdline.length;

    for (let i = 0; i < len; ) {
      const c = cmdline[i];

      if (/\s/.test(c) && !inQuotes) {
        if (token.length > 0) {
          tokens.push(token);
          token = '';
        }
        i++;
        continue;
      }

      if (c === '"') {
        inQuotes = !inQuotes;
        i++;
        continue;
      }

      if (c === '\\') {
        let numBS = 0;
        while (i < len && cmdline[i] === '\\') {
          numBS++;
          i++;
        }
        if (i < len && cmdline[i] === '"') {
          token += '\\'.repeat(Math.floor(numBS / 2));
          if (numBS % 2 === 0) {
            inQuotes = !inQuotes;
          } else {
            token += '"';
          }
          i++;
        } else {
          token += '\\'.repeat(numBS);
        }
        continue;
      }

      token += c;
      i++;
    }

    if (token.length > 0) {
      tokens.push(token);
    }

    return tokens;
  };

  // Built-in beacon commands that Web client may not have definitions for
  // Format: { argName: argType } where argType is 'string' or 'int'
  const BUILTIN_COMMANDS = {
    help: { description: 'Show this help' },
    sleep: { args: [{ name: 'sleep', type: 'string' }, { name: 'jitter', type: 'int' }], description: 'Set agent sleep time and jitter' },
    cat: { args: [{ name: 'path', type: 'string' }], description: 'Read file content' },
    cd: { args: [{ name: 'path', type: 'string' }], description: 'Change current directory' },
    cp: { args: [{ name: 'src', type: 'string' }, { name: 'dst', type: 'string' }], description: 'Copy file' },
    mv: { args: [{ name: 'src', type: 'string' }, { name: 'dst', type: 'string' }], description: 'Move/Rename file' },
    rm: { args: [{ name: 'path', type: 'string' }], description: 'Remove file or directory' },
    mkdir: { args: [{ name: 'path', type: 'string' }], description: 'Create directory' },
    ls: { args: [{ name: 'directory', type: 'string', defaultValue: '.' }], description: 'List directory content' },
    download: { args: [{ name: 'file', type: 'string' }], description: 'Download file from agent' },
    upload: { args: [{ name: 'local_file', type: 'file' }, { name: 'remote_path', type: 'string' }], description: 'Upload file to agent' },
    unlink: { args: [{ name: 'id', type: 'string' }], description: 'Unlink a child agent' },
    exit: { description: 'Terminate agent session' },
    whoami: { description: 'Get current user identity' },
    pwd: { description: 'Get current working directory' },
    // Commands with subcommands
    ps: { 
      description: 'Process management',
      subcommands: [
        { name: 'list', description: 'List running processes' },
        { name: 'kill', args: [{ name: 'pid', type: 'int' }], description: 'Kill a process' },
        { name: 'run', args: [{ name: 'path', type: 'string' }], description: 'Run a process' }
      ] 
    },
    jobs: { 
      description: 'Manage background jobs',
      subcommands: [
        { name: 'list', description: 'List background jobs' },
        { name: 'kill', args: [{ name: 'jid', type: 'int' }], description: 'Kill a job' }
      ] 
    },
    socks: { 
      description: 'SOCKS proxy management',
      subcommands: [
        { name: 'start', args: [{ name: 'port', type: 'int' }], description: 'Start SOCKS5 proxy' },
        { name: 'stop', description: 'Stop SOCKS5 proxy' }
      ] 
    },
    lportfwd: { 
      description: 'Local port forward',
      subcommands: [
        { name: 'start', args: [{ name: 'lport', type: 'int' }, { name: 'thost', type: 'string' }, { name: 'tport', type: 'int' }], description: 'Start local port forward' },
        { name: 'stop', args: [{ name: 'lport', type: 'int' }], description: 'Stop local port forward' }
      ] 
    },
    rportfwd: { 
      description: 'Reverse port forward',
      subcommands: [
        { name: 'start', args: [{ name: 'rport', type: 'int' }, { name: 'thost', type: 'string' }, { name: 'tport', type: 'int' }], description: 'Start reverse port forward' },
        { name: 'stop', args: [{ name: 'rport', type: 'int' }], description: 'Stop reverse port forward' }
      ] 
    },
  };

  const processCommand = useCallback(async (agentId, cmdline) => {
    const agent = agents.find(a => a.a_id === agentId);
    if (!agent) return;

    const parts = unserializeParams(cmdline.trim());
    if (parts.length === 0) return;

    const commandName = parts[0].toLowerCase();
    
    // Handle 'reload' command
    if (commandName === 'reload') {
      addConsoleLine(agentId, { type: 'output', content: '[*] Reloading Extension-Kit scripts...', msgType: 0 });
      try {
        await reloadScripts();
        const allCmds = axEngine.getAllCommands(agent.a_name);
        addConsoleLine(agentId, { type: 'output', content: `[+] Successfully reloaded ${allCmds.size} commands`, msgType: 0 });
      } catch (err) {
        addConsoleLine(agentId, { type: 'output', content: `[-] Reload failed: ${err.message}`, msgType: 2 });
      }
      return;
    }
    
    // Handle 'help' command locally
    if (commandName === 'help') {
      const allCommands = axEngine.getAllCommands(agent.a_name);
      let helpText = '\n  Command                       Description\n  -------                       -----------\n';
      
      // 1. Extension/Engine Commands
      allCommands.forEach((cmd, name) => {
        const padding = ' '.repeat(Math.max(1, 30 - name.length));
        helpText += `  ${name}${cmd.subcommands?.length > 0 ? '*' : ''}${padding}${cmd.description || ''}\n`;
      });
      
      // Add a reload command description
      const reloadPadding = ' '.repeat(Math.max(1, 30 - 'reload'.length));
      helpText += `  reload${reloadPadding}Reload Extension-Kit scripts\n`;
      
      // 2. Built-in Commands (if not overridden)
      Object.entries(BUILTIN_COMMANDS).forEach(([name, def]) => {
        if (!allCommands.has(name)) {
          const padding = ' '.repeat(Math.max(1, 30 - name.length));
          helpText += `  ${name}${def.subcommands?.length > 0 ? '*' : ''}${padding}${def.description || '(built-in)'}\n`;
        }
      });
      
      addConsoleLine(agentId, { type: 'output', content: helpText, msgType: 0 });
      return;
    }
    
    // First, try to get command from agent-specific definitions (loaded from ax_config.axs)
    let engineCommand = axEngine.getAgentCommand(agent.a_name, commandName);
    
    // Fall back to extension commands
    if (!engineCommand) {
      engineCommand = axEngine.getCommand(commandName);
    }

    // Try multi-word command if not found (e.g. "token make")
    if (!engineCommand && parts.length > 1) {
      const fullCommandName = `${parts[0]} ${parts[1]}`;
      engineCommand = axEngine.getCommand(fullCommandName);
      if (engineCommand) {
        // Shift parts to account for multi-word command
        parts.splice(0, 1);
        parts[0] = fullCommandName;
      }
    }

    // If still no definition, check built-in commands as last resort
    const builtinDef = BUILTIN_COMMANDS[commandName];
    if (!engineCommand && builtinDef) {
      engineCommand = builtinDef;
    }

    // Build the data object that the agent plugin expects
    const commandData = { command: engineCommand?.name || commandName };

    if (engineCommand) {
      // 1. Process Pre-Hook if defined (Matches Qt's Commander::ProcessPreHook)
      if (engineCommand.is_pre_hook && typeof engineCommand.pre_hook === 'function') {
        try {
          // Prepare args for the hook: agentId, cmdline, data, ...rest_args
          const hookResult = engineCommand.pre_hook(agentId, cmdline, commandData, ...parts.slice(1));
          
          // If hook returns a string, it's an error message to display locally
          if (typeof hookResult === 'string' && hookResult !== "") {
            addConsoleLine(agentId, { type: 'output', content: `[-] Error: ${hookResult}`, msgType: 2 });
            return;
          }
          // If hook returns null/undefined or empty string, continue execution
        } catch (e) {
          console.error('[AgentContext] Pre-hook execution failed:', e);
          addConsoleLine(agentId, { type: 'output', content: `[-] Script Error: ${e.message}`, msgType: 2 });
          return;
        }
      }

      // Update command name if it was a multi-word or remapped command
      if (engineCommand.name && engineCommand.name !== commandName) {
        commandData.command = engineCommand.name;
      }
      
      // Check for subcommands
      const hasSubcommands = (engineCommand.subcommands && engineCommand.subcommands.length > 0) || 
                             (Array.isArray(builtinDef?.subcommands));
      
      if (hasSubcommands && parts.length > 1) {
        const subcommandName = parts[1];
        const subcommandList = engineCommand.subcommands || builtinDef?.subcommands || [];
        const subcommand = Array.isArray(subcommandList) && typeof subcommandList[0] === 'string'
          ? subcommandList.includes(subcommandName) ? { name: subcommandName } : null
          : subcommandList.find(sc => sc.name === subcommandName);
        
        if (subcommand) {
          commandData.subcommand = subcommandName;
          // Parse remaining args based on subcommand definition (if available)
          if (subcommand.args) {
            const parsedArgs = parseCommandArgs(subcommand, parts.slice(2));
            Object.assign(commandData, parsedArgs);
          } else if (parts.length > 2) {
            // Simple parsing for subcommand args
            commandData.args = parts.slice(2).join(' ');
            parts.slice(2).forEach((arg, idx) => {
              commandData[`arg${idx}`] = arg;
            });
          }
        } else {
          // Not a subcommand, might be an argument
          const parsedArgs = parseCommandArgs(engineCommand, parts.slice(1));
          Object.assign(commandData, parsedArgs);
        }
      } else if (engineCommand.args) {
        // Parse arguments based on command definition
        const parsedArgs = parseCommandArgs(engineCommand, parts.slice(1));
        Object.assign(commandData, parsedArgs);
      }
    } else {
      // No command definition found, use simple parsing
      if (parts.length > 1) {
        commandData.subcommand = parts[1];
        if (parts.length > 2) {
          commandData.args = parts.slice(2).join(' ');
        }
      }
    }

    try {
      const dataJson = JSON.stringify(commandData);
      
      // Determine if it's a UI-interactive command based on metadata or specific names
      const isUICommand = commandName === 'upload' || commandName === 'download';
      
      const response = await agentApi.executeCommand({
        name: agent.a_name,
        id: agent.a_id,
        ui: isUICommand,
        cmdline: cmdline,
        data: dataJson,
        ax_hook_id: "",
        ax_handler_id: ""
      });
      // Check if server returned an error in the response
      if (response.data && !response.data.ok) {
        addConsoleLine(agentId, {
          type: 'output',
          content: `[-] Error: ${response.data.message || 'Command rejected by server'}`,
          msgType: 2
        });
      }
    } catch (err) {
      console.error('[AgentContext] Command execution failed:', err);
      console.error('[AgentContext] Error details:', err.response?.status, err.response?.data);
      addConsoleLine(agentId, {
        type: 'output',
        content: `[-] Error: ${err.response?.data?.message || err.message || 'Command failed to send'}`,
        msgType: 2
      });
    }
  }, [agents]); // Note: addConsoleLine is stable and doesn't need to be in deps

  // Batch processing for high-frequency updates
  useEffect(() => {
    const interval = setInterval(() => {
      // Process Console Queue
      const queuedConsole = consoleQueueRef.current;
      if (Object.keys(queuedConsole).length > 0) {
        consoleQueueRef.current = {}; // Reset queue
        setConsoleHistory(prev => {
          const next = { ...prev };
          Object.entries(queuedConsole).forEach(([aid, newLines]) => {
            if (newLines.length > 0) {
              const current = next[aid] || [];
              next[aid] = [...current, ...newLines].slice(-CONSOLE_BUFFER_LIMIT);
            }
          });
          return next;
        });
      }

      // Process Logs Queue
      const queuedLogs = logQueueRef.current;
      if (queuedLogs.length > 0) {
        logQueueRef.current = []; // Reset queue
        setLogs(prev => {
          const next = [...prev, ...queuedLogs];
          return next.slice(-LOGS_BUFFER_LIMIT);
        });
      }

      // Process Chat Queue
      const queuedChat = chatQueueRef.current;
      if (queuedChat.length > 0) {
        chatQueueRef.current = []; // Reset queue
        setChatMessages(prev => {
          const next = [...prev, ...queuedChat];
          return next.slice(-CHAT_BUFFER_LIMIT);
        });
      }
    }, 200); // 200ms batch window

    return () => clearInterval(interval);
  }, []);

  // Initial Data Sync
  const syncAllData = useCallback(async () => {
    try {
      const [
        agentsRes,
        listenersRes,
        downloadsRes,
        targetsRes,
        credsRes,
        screenshotsRes,
        tunnelsRes,
        deliveriesRes
      ] = await Promise.all([
        agentApi.list(),
        listenerApi.list(),
        dataApi.downloads(),
        dataApi.targets(),
        dataApi.creds(),
        dataApi.screenshots(),
        tunnelApi.list(),
        deliveryApi.list()
      ]);

      setAgents(Array.isArray(agentsRes.data) ? agentsRes.data : []);
      setListeners(Array.isArray(listenersRes.data) ? listenersRes.data : []);
      setDownloads(Array.isArray(downloadsRes.data) ? downloadsRes.data : []);
      setTargets(Array.isArray(targetsRes.data) ? targetsRes.data : []);
      setCredentials(Array.isArray(credsRes.data) ? credsRes.data : []);
      setScreenshots(Array.isArray(screenshotsRes.data) ? screenshotsRes.data : []);
      setTunnels(Array.isArray(tunnelsRes.data) ? tunnelsRes.data : []);
      
      const deliveriesMap = {};
      if (Array.isArray(deliveriesRes.data)) {
        deliveriesRes.data.forEach(d => { deliveriesMap[d.f_file_id] = d; });
      }
      setFileDeliveries(deliveriesMap);

      // Global data sync completed
    } catch (err) {
      console.error('[AgentContext] Global data sync failed:', err);
    }
  }, []);

  useEffect(() => {
    syncAllData();
  }, [syncAllData]);

  const fetchAgents = useCallback(async () => {
    try {
      const response = await agentApi.list();
      setAgents(Array.isArray(response.data) ? response.data : []);
    } catch (err) {
      console.error('Failed to fetch agents:', err);
    }
  }, []);

  useEffect(() => {
    fetchAgents();
  }, [fetchAgents]);

  useEffect(() => {
    const removeListener = addListener((packet) => {
      const type = packet.type;
      

      switch (type) {
        // --- Agent Management ---
        case PacketType.AGENT_NEW:
          setAgents(prev => {
            if (prev.find(a => a.a_id === packet.a_id)) return prev;
            return [...prev, packet];
          });
          break;

        case PacketType.AGENT_TICK:
          // packet.a_id is an array of IDs that checked in
          if (Array.isArray(packet.a_id)) {
            setAgents(prev => prev.map(a => 
              packet.a_id.includes(a.a_id) ? { ...a, a_last_tick: Math.floor(Date.now() / 1000) } : a
            ));
          }
          break;

        case PacketType.AGENT_UPDATE:
          setAgents(prev => prev.map(a => 
            a.a_id === packet.a_id ? { ...a, ...packet } : a
          ));
          break;

        case PacketType.AGENT_REMOVE:
          setAgents(prev => prev.filter(a => a.a_id !== packet.a_id));
          setOpenTabs(prev => prev.filter(t => t.a_id !== packet.a_id));
          break;

        case PacketType.AGENT_REG:
          // Register agent configuration/capabilities (metadata from server)
          if (packet.agent) {
            setAgentConfigs(prev => ({
              ...prev,
              [packet.agent]: { ax: packet.ax, listeners: packet.listeners || [] }
            }));
            // Load and execute agent's ax_config.axs to register commands
            if (packet.ax && packet.listeners) {
              axEngine.loadAgentScript(packet.agent, packet.ax, packet.listeners);
            }
          }
          break;

        case PacketType.AGENT_LINK:
          // Handle agent peer linking (P2P/SMB beacons)
          break;

        // --- Listener Management ---
        case PacketType.LISTENER_REG:
          // Update available listener types metadata
          if (packet.l_name) {
            setListenerConfigs(prev => ({
              ...prev,
              [packet.l_name]: { protocol: packet.l_protocol, type: packet.l_type, ax: packet.ax }
            }));
          }
          break;

        case PacketType.LISTENER_START:
          setListeners(prev => {
            if (prev.find(l => l.l_name === packet.l_name)) return prev;
            return [...prev, packet];
          });
          break;

        case PacketType.LISTENER_EDIT:
          setListeners(prev => prev.map(l => 
            l.l_name === packet.l_name ? { ...l, ...packet } : l
          ));
          break;

        case PacketType.LISTENER_STOP:
          setListeners(prev => prev.filter(l => l.l_name !== packet.l_name));
          break;

    // --- Console Output ---
    case PacketType.AGENT_CONSOLE_OUT:
      {
        const agentId = packet.a_id;
        let content = packet.a_text || packet.a_message;
        
        // Handle Base64 encoded console output from Qt server
        if (packet.a_base64 && content) {
          try {
            content = atob(content);
          } catch (e) {
            console.error('[AgentContext] Failed to decode console Base64:', e);
          }
        }

        const item = {
          type: 'output',
          content: content,
          msgType: packet.a_msg_type,
          time: packet.time || Math.floor(Date.now() / 1000)
        };
        if (!consoleQueueRef.current[agentId]) consoleQueueRef.current[agentId] = [];
        consoleQueueRef.current[agentId].push(item);
      }
      break;

        // --- Task Management ---
        case PacketType.AGENT_TASK_SYNC:
          setTasks(prev => ({
            ...prev,
            [packet.a_task_id]: { 
              ...packet, 
              Status: packet.a_completed ? (packet.a_msg_type === 2 || packet.a_msg_type === 4 ? "Error" : "Success") : "Running",
              a_output: packet.a_text || packet.a_message // Ensure output is mapped
            }
          }));
          break;

        case PacketType.AGENT_TASK_UPDATE:
          setTasks(prev => {
            const taskId = packet.a_task_id;
            if (!prev[taskId]) return prev;
            const updatedTask = { ...prev[taskId], ...packet };
            if (packet.a_completed) {
              updatedTask.Status = (packet.a_msg_type === 2 || packet.a_msg_type === 4) ? "Error" : "Success";
            }
            updatedTask.a_output = packet.a_text || packet.a_message; // Update output
            return { ...prev, [taskId]: updatedTask };
          });
          break;

        case PacketType.AGENT_TASK_SEND:
          if (Array.isArray(packet.a_task_id)) {
            setTasks(prev => {
              const next = { ...prev };
              packet.a_task_id.forEach(id => {
                if (next[id]) next[id].Status = "Running";
              });
              return next;
            });
          }
          break;

        case PacketType.AGENT_TASK_REMOVE:
          setTasks(prev => {
            const next = { ...prev };
            delete next[packet.a_task_id];
            return next;
          });
          break;

        // --- Console Task Synchronization ---
        case PacketType.AGENT_CONSOLE_TASK_SYNC:
        case PacketType.AGENT_CONSOLE_TASK_UPD:
          // These are primarily for updating the console history/UI
          // Handled similarly to AGENT_CONSOLE_OUT but with more task-specific info
          {
            const agentId = packet.a_id;
            const taskId = packet.a_task_id;
            const item = {
              type: 'task',
              taskId,
              content: packet.a_text || packet.a_message,
              cmdline: packet.a_cmdline,
              completed: packet.a_completed,
              msgType: packet.a_msg_type,
              time: packet.a_finish_time || packet.a_start_time || Math.floor(Date.now() / 1000)
            };
            if (!consoleQueueRef.current[agentId]) consoleQueueRef.current[agentId] = [];
            consoleQueueRef.current[agentId].push(item);
          }
          break;

        // --- Chat ---
        case PacketType.CHAT_MESSAGE:
          chatQueueRef.current.push({
            time: packet.c_date || Math.floor(Date.now() / 1000),
            username: packet.c_username,
            message: packet.c_message
          });
          break;

        // --- Download Management ---
        case PacketType.DOWNLOAD_CREATE:
          setDownloads(prev => [...prev, packet.data || packet]);
          break;
        case PacketType.DOWNLOAD_UPDATE:
          setDownloads(prev => prev.map(d => 
            d.d_file_id === packet.d_file_id ? { ...d, d_recv_size: packet.d_recv_size, d_state: packet.d_state } : d
          ));
          break;
        case PacketType.DOWNLOAD_DELETE:
          if (Array.isArray(packet.d_files_id)) {
            setDownloads(prev => prev.filter(d => !packet.d_files_id.includes(d.d_file_id)));
          }
          break;

        // --- Screenshot Management ---
        case PacketType.SCREEN_CREATE:
          setScreenshots(prev => [...prev, packet.data || packet]);
          break;
        case PacketType.SCREEN_UPDATE:
          setScreenshots(prev => prev.map(s => 
            s.s_screen_id === packet.s_screen_id ? { ...s, s_note: packet.s_note } : s
          ));
          break;
        case PacketType.SCREEN_DELETE:
          setScreenshots(prev => prev.filter(s => s.s_screen_id !== packet.s_screen_id));
          break;

        // --- Credential Management ---
        case PacketType.CREDS_CREATE:
          if (Array.isArray(packet.c_creds)) {
            setCredentials(prev => {
              const next = [...prev];
              packet.c_creds.forEach(c => {
                if (!next.find(item => item.c_creds_id === c.c_creds_id)) {
                  next.push(c);
                }
              });
              return next;
            });
          }
          break;
        case PacketType.CREDS_EDIT:
          setCredentials(prev => prev.map(c => 
            c.c_creds_id === packet.c_creds_id ? { ...c, ...packet } : c
          ));
          break;
        case PacketType.CREDS_DELETE:
          if (Array.isArray(packet.c_creds_id)) {
            setCredentials(prev => prev.filter(c => !packet.c_creds_id.includes(c.c_creds_id)));
          }
          break;

        case PacketType.CREDS_SET_TAG:
          if (Array.isArray(packet.c_creds_id)) {
            setCredentials(prev => prev.map(c => 
              packet.c_creds_id.includes(c.c_creds_id) ? { ...c, c_tag: packet.c_tag } : c
            ));
          }
          break;

        // --- Target Management ---
        case PacketType.TARGETS_CREATE:
          if (Array.isArray(packet.t_targets)) {
            setTargets(prev => {
              const next = [...prev];
              packet.t_targets.forEach(t => {
                if (!next.find(item => item.t_target_id === t.t_target_id)) {
                  next.push(t);
                }
              });
              return next;
            });
          }
          break;
        case PacketType.TARGETS_EDIT:
          setTargets(prev => prev.map(t => 
            t.t_target_id === packet.t_target_id ? { ...t, ...packet } : t
          ));
          break;
        case PacketType.TARGETS_DELETE:
          if (Array.isArray(packet.t_target_id)) {
            setTargets(prev => prev.filter(t => !packet.t_target_id.includes(t.t_target_id)));
          }
          break;

        case PacketType.TARGETS_SET_TAG:
          if (Array.isArray(packet.t_targets_id)) {
            setTargets(prev => prev.map(t => 
              packet.t_targets_id.includes(t.t_target_id) ? { ...t, t_tag: packet.t_tag } : t
            ));
          }
          break;

        // --- Tunnel Management ---
        case PacketType.TUNNEL_CREATE:
          setTunnels(prev => [...prev, packet.data || packet]);
          break;
        case PacketType.TUNNEL_EDIT:
          setTunnels(prev => prev.map(t => 
            t.p_tunnel_id === packet.p_tunnel_id ? { ...t, p_info: packet.p_info } : t
          ));
          break;
        case PacketType.TUNNEL_DELETE:
          setTunnels(prev => prev.filter(t => t.p_tunnel_id !== packet.p_tunnel_id));
          break;

        // --- Pivot Management ---
        case PacketType.PIVOT_CREATE:
          setPivots(prev => ({ ...prev, [packet.p_pivot_id]: packet.data || packet }));
          break;
        case PacketType.PIVOT_DELETE:
          setPivots(prev => {
            const next = { ...prev };
            delete next[packet.p_pivot_id];
            return next;
          });
          break;

        // --- File Delivery ---
        case PacketType.FILEDELIVERY_CREATE:
          setFileDeliveries(prev => ({ ...prev, [packet.f_file_id]: packet.data || packet }));
          break;
        case PacketType.FILEDELIVERY_UPDATE:
          setFileDeliveries(prev => ({
            ...prev,
            [packet.f_file_id]: { ...(prev[packet.f_file_id] || {}), ...packet }
          }));
          break;
        case PacketType.FILEDELIVERY_DELETE:
          setFileDeliveries(prev => {
            const next = { ...prev };
            delete next[packet.f_file_id];
            return next;
          });
          break;

        // --- Browser Management ---
        case PacketType.BROWSER_DISKS:
          if (packet.b_agent_id || packet.a_id) {
            const aid = packet.b_agent_id || packet.a_id;
            setBrowserData(prev => ({
              ...prev,
              [aid]: { ...(prev[aid] || {}), disks: JSON.parse(packet.b_data || '[]') }
            }));
          }
          break;
        case PacketType.BROWSER_FILES:
          if (packet.b_agent_id || packet.a_id) {
            const aid = packet.b_agent_id || packet.a_id;
            setBrowserData(prev => ({
              ...prev,
              [aid]: { ...(prev[aid] || {}), files: JSON.parse(packet.b_data || '[]'), currentPath: packet.b_path }
            }));
          }
          break;
        case PacketType.BROWSER_PROCESS:
          if (packet.b_agent_id || packet.a_id) {
            const aid = packet.b_agent_id || packet.a_id;
            setBrowserData(prev => ({
              ...prev,
              [aid]: { ...(prev[aid] || {}), procs: JSON.parse(packet.b_data || '[]') }
            }));
          }
          break;
        case PacketType.BROWSER_STATUS:
          // Optional: Update UI with status message (e.g. "Refreshing...")
          break;

        // --- System Events ---
        case PacketType.SP_TYPE_EVENT:
          logQueueRef.current.push({
            type: 'event',
            time: packet.time || Math.floor(Date.now() / 1000),
            content: packet.message || packet.data
          });
          break;

        // --- Task Hooks ---
        case PacketType.AGENT_TASK_HOOK:
          // Post-process logic from Qt (PostHookProcess)
          break;

        default:
          break;
      }
    });
    return () => removeListener();
  }, [addListener]);

  const openAgentTab = (agent, subTab = 'console') => {
    const agentId = agent.a_id;
    const existingTab = openTabs.find(t => t.id === agentId);
    if (!existingTab) {
      setOpenTabs([...openTabs, { 
        ...agent, 
        id: agentId, 
        type: 'agent', 
        title: `${agent.a_name} (${agentId.substring(0, 8)})`,
        activeSubTab: subTab 
      }]);
    } else if (subTab !== existingTab.activeSubTab) {
      setOpenTabs(openTabs.map(t => t.id === agentId ? { ...t, activeSubTab: subTab } : t));
    }
    setActiveTabId(agentId);
    setIsDockExpanded(true);
  };

  const openDockTab = (tabId, type, title) => {
    const existingTab = openTabs.find(t => t.id === tabId);
    if (!existingTab) {
      setOpenTabs([...openTabs, { id: tabId, type, title }]);
    }
    setActiveTabId(tabId);
    setIsDockExpanded(true);
  };

  const setActiveSubTab = (agentId, subTab) => {
    setOpenTabs(openTabs.map(t => t.id === agentId ? { ...t, activeSubTab: subTab } : t));
  };

  const closeTab = (id) => {
    if (id === 'logs') return; // Logs tab is permanent
    const newTabs = openTabs.filter(t => t.id !== id);
    setOpenTabs(newTabs);
    if (activeTabId === id && newTabs.length > 0) {
      setActiveTabId(newTabs[newTabs.length - 1].id);
    } else if (newTabs.length === 0) {
      setActiveTabId(null);
    }
  };

  const addConsoleLine = (agentId, line) => {
    setConsoleHistory(prev => {
      if (line.type === 'clear') {
        return { ...prev, [agentId]: [] };
      }
      const current = prev[agentId] || [];
      const newHistory = [...current, line];
      return {
        ...prev,
        [agentId]: newHistory.slice(-CONSOLE_BUFFER_LIMIT)
      };
    });
  };

  return (
    <AgentContext.Provider value={{ 
      agents,
      listeners,
      tasks,
      logs,
      credentials,
      targets,
      downloads,
      screenshots,
      fileDeliveries,
      tunnels,
      pivots,
      openTabs, 
      activeTabId, 
      setActiveTabId, 
      openAgentTab, 
      openDockTab,
      closeTab,
      isDockExpanded,
      setIsDockExpanded,
      fetchAgents,
      chatMessages,
      browserData,
      agentConfigs,
      listenerConfigs,
      consoleHistory,
      addConsoleLine,
      axCommands,
      axPlugins,
      axEngine,
      setActiveSubTab,
      processCommand,
      axStats,
      reloadScripts
    }}>
      {children}
    </AgentContext.Provider>
  );
};
