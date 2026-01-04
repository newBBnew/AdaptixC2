import React, { createContext, useContext, useState, useEffect, useCallback, useRef } from 'react';
import { useSocket } from './SocketContext';
import { agentApi } from '../api/agent';
import { listenerApi, taskApi, tunnelApi, deliveryApi, dataApi, scriptApi } from '../api/control';
import { PacketType } from '../constants/packetTypes';
import createAxEngine from '../utils/axScript';

const AgentContext = createContext();

export const useAgents = () => useContext(AgentContext);

// Pure helper functions defined outside the component to ensure absolute safety from TDZ
const _unserializeParams = (cmdline) => {
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
  if (token.length > 0) tokens.push(token);
  return tokens;
};

const _parseCommandArgs = (cmdDef, argParts, isSubcommand = false) => {
  const result = {};
  if (!cmdDef || !cmdDef.args) return result;
  let argIndex = 0;
  const args = cmdDef.args || [];
  for (let i = 0; i < argParts.length && argIndex < args.length; i++) {
    const part = argParts[i];
    const argDef = args[argIndex];
    if (part.startsWith('-') && argDef.mark) {
      if (argDef.type.toUpperCase() === 'BOOL' && argDef.mark === part) {
        result[argDef.mark] = true;
        continue;
      }
      if (argDef.mark === part && i + 1 < argParts.length) {
        i++;
        result[argDef.name] = argDef.type.toUpperCase() === 'INT' ? parseInt(argParts[i]) : argParts[i];
        argIndex++;
        continue;
      }
    }
    if (argDef.type.toUpperCase() === 'INT') {
      result[argDef.name] = parseInt(part);
    } else if (argDef.type.toUpperCase() === 'FILE') {
      result[argDef.name] = part;
    } else {
      if (argIndex === args.length - 1 && argDef.type.toUpperCase() === 'STRING') {
        result[argDef.name] = argParts.slice(i).join(' ');
        break;
      }
      result[argDef.name] = part;
    }
    argIndex++;
  }
  args.forEach(arg => {
    if (!(arg.name in result) && arg.defaultValue !== undefined) {
      result[arg.name] = arg.defaultValue;
    }
  });
  return result;
};

// --- Command Processing Logic (Extracted to avoid TDZ) ---
const _executeCommandLogic = async (agentId, cmdline, agents, axEngine, reloadScripts, addConsoleLine, parseCommandArgs, unserializeParams, BUILTIN_COMMANDS) => {
  const agent = agents.find(a => a.a_id === agentId);
  if (!agent || !axEngine) return null;

  const parts = unserializeParams(cmdline.trim());
  if (parts.length === 0) return null;

  const commandName = parts[0].toLowerCase();
  
  if (commandName === 'reload') {
    addConsoleLine(agentId, { type: 'output', content: '[*] Reloading Extension-Kit scripts...', msgType: 0 });
    try {
      await reloadScripts();
      const allCmds = axEngine.getAllCommands(agent.a_name);
      addConsoleLine(agentId, { type: 'output', content: `[+] Successfully reloaded ${allCmds.size} commands`, msgType: 0 });
    } catch (err) {
      addConsoleLine(agentId, { type: 'output', content: `[-] Reload failed: ${err.message}`, msgType: 2 });
    }
    return { name: 'reload' };
  }
  
  if (commandName === 'help') {
    const allCommands = axEngine.getAllCommands(agent.a_name);
    const commandGroups = axEngine.getCommandGroups?.() || [];
    let helpText = '';
    
    if (parts.length === 1) {
      // help - show all commands grouped (matches Qt client)
      helpText = '\n  Command                       Description\n  -------                       -----------\n';
      // Built-in commands first
      Object.entries(BUILTIN_COMMANDS).forEach(([name, def]) => {
        if (!allCommands.has(name)) {
          const padding = ' '.repeat(Math.max(1, 30 - name.length));
          helpText += `  ${name}${def.subcommands?.length > 0 ? '*' : ''}${padding}${def.description || '(built-in)'}\n`;
        }
      });
      const reloadPadding = ' '.repeat(Math.max(1, 30 - 'reload'.length));
      helpText += `  reload${reloadPadding}Reload Extension-Kit scripts\n`;
      
      // Extension-Kit commands by group
      commandGroups.forEach(group => {
        helpText += `\n  Group - ${group.groupName}\n  =====================================\n`;
        group.commands.filter(cmd => cmd && cmd.name).forEach(cmd => {
          if (cmd.subcommands?.length > 0) {
            cmd.subcommands.forEach(sub => {
              const fullName = `${cmd.name} ${sub.name}`;
              const padding = ' '.repeat(Math.max(1, 30 - fullName.length));
              helpText += `  ${fullName}${padding}${sub.description || ''}\n`;
            });
          } else {
            const padding = ' '.repeat(Math.max(1, 30 - cmd.name.length));
            helpText += `  ${cmd.name}${padding}${cmd.description || ''}\n`;
          }
        });
      });
    } else {
      // help <command> or help <command> <subcommand>
      const helpCmdName = parts[1];
      let targetCmd = allCommands.get(helpCmdName) || BUILTIN_COMMANDS[helpCmdName];
      
      if (!targetCmd) {
        addConsoleLine(agentId, { type: 'output', content: `[-] Unknown command: ${helpCmdName}`, msgType: 2 });
        return { name: 'help' };
      }
      
      if (parts.length === 2) {
        // help <command>
        helpText = `\n  Command               : ${targetCmd.name}\n`;
        if (targetCmd.description) helpText += `  Description           : ${targetCmd.description}\n`;
        if (targetCmd.example) helpText += `  Example               : ${targetCmd.example}\n`;
        
        if (targetCmd.subcommands?.length > 0) {
          helpText += '\n  SubCommand                Description\n  ----------                -----------\n';
          targetCmd.subcommands.forEach(sub => {
            const padding = ' '.repeat(Math.max(1, 20 - (sub.name?.length || 0)));
            helpText += `  ${sub.name}${padding}      ${sub.description || ''}\n`;
          });
        } else if (targetCmd.args?.length > 0) {
          let usageStr = targetCmd.name;
          targetCmd.args.forEach(arg => {
            const bracket = (arg.required && !arg.defaultUsed) ? ['<', '>'] : ['[', ']'];
            const argStr = arg.mark ? (arg.name ? `${arg.mark} ${arg.name}` : arg.mark) : arg.name;
            usageStr += ` ${bracket[0]}${argStr}${bracket[1]}`;
          });
          helpText += `  Usage                 : ${usageStr}\n\n  Arguments:\n`;
          targetCmd.args.forEach(arg => {
            const bracket = (arg.required && !arg.defaultUsed) ? ['<', '>'] : ['[', ']'];
            const argStr = arg.mark ? (arg.name ? `${arg.mark} ${arg.name}` : arg.mark) : arg.name;
            const fullArg = `${bracket[0]}${argStr}${bracket[1]}`;
            const padding = ' '.repeat(Math.max(1, 20 - fullArg.length));
            const defStr = arg.defaultUsed ? ` (default: '${arg.defaultValue}'). ` : ' ';
            helpText += `    ${fullArg}${padding}  : ${(arg.type + '.').padEnd(9)}${defStr}${arg.description || ''}\n`;
          });
        }
      } else {
        // help <command> <subcommand>
        const subCmdName = parts[2];
        const subCmd = targetCmd.subcommands?.find(s => s.name === subCmdName);
        if (!subCmd) {
          addConsoleLine(agentId, { type: 'output', content: `[-] Unknown subcommand: ${subCmdName}`, msgType: 2 });
          return { name: 'help' };
        }
        helpText = `\n  Command               : ${targetCmd.name} ${subCmd.name}\n`;
        if (subCmd.description) helpText += `  Description           : ${subCmd.description}\n`;
        if (subCmd.example) helpText += `  Example               : ${subCmd.example}\n`;
        if (subCmd.args?.length > 0) {
          let usageStr = `${targetCmd.name} ${subCmd.name}`;
          subCmd.args.forEach(arg => {
            const bracket = (arg.required && !arg.defaultUsed) ? ['<', '>'] : ['[', ']'];
            const argStr = arg.mark ? (arg.name ? `${arg.mark} ${arg.name}` : arg.mark) : arg.name;
            usageStr += ` ${bracket[0]}${argStr}${bracket[1]}`;
          });
          helpText += `  Usage                 : ${usageStr}\n\n  Arguments:\n`;
          subCmd.args.forEach(arg => {
            const bracket = (arg.required && !arg.defaultUsed) ? ['<', '>'] : ['[', ']'];
            const argStr = arg.mark ? (arg.name ? `${arg.mark} ${arg.name}` : arg.mark) : arg.name;
            const fullArg = `${bracket[0]}${argStr}${bracket[1]}`;
            const padding = ' '.repeat(Math.max(1, 20 - fullArg.length));
            const defStr = arg.defaultUsed ? ` (default: '${arg.defaultValue}'). ` : ' ';
            helpText += `    ${fullArg}${padding}  : ${(arg.type + '.').padEnd(9)}${defStr}${arg.description || ''}\n`;
          });
        }
      }
    }
    addConsoleLine(agentId, { type: 'output', content: helpText, msgType: 0 });
    return { name: 'help' };
  }
  
  let engineCommand = axEngine.getAgentCommand(agent.a_name, commandName);
  if (!engineCommand) engineCommand = axEngine.getCommand(commandName);
  if (!engineCommand) {
    const builtinDef = BUILTIN_COMMANDS[commandName];
    if (builtinDef) engineCommand = builtinDef;
  }
  
  // For commands with subcommands (e.g., "token steal"), verify subcommand exists
  let subCmd = null;
  if (engineCommand && engineCommand.subcommands?.length > 0 && parts.length > 1) {
    const subName = parts[1];
    subCmd = engineCommand.subcommands.find(s => s.name === subName);
    if (!subCmd) {
      addConsoleLine(agentId, { type: 'output', content: `[-] Unknown subcommand: ${commandName} ${subName}`, msgType: 2 });
      return null;
    }
  }
  
  // Error command prevention: if command not found, show error and don't send to server
  if (!engineCommand) {
    addConsoleLine(agentId, { type: 'output', content: `[-] Command not found: ${commandName}`, msgType: 2 });
    return null;
  }

  const commandData = { command: engineCommand?.name || commandName };

  if (engineCommand && engineCommand.name) {
    if (engineCommand.name && engineCommand.name !== commandName) {
      commandData.command = engineCommand.name;
    }
    
    // Parse arguments BEFORE calling pre_hook so parsed_json contains the args
    const hasSubcommands = (engineCommand.subcommands && engineCommand.subcommands.length > 0);
    if (hasSubcommands && parts.length > 1) {
      const subcommandName = parts[1];
      const subcommandList = engineCommand.subcommands || [];
      const subcommand = Array.isArray(subcommandList) && typeof subcommandList[0] === 'string'
        ? subcommandList.includes(subcommandName) ? { name: subcommandName } : null
        : subcommandList.find(sc => sc.name === subcommandName);
      
      if (subcommand) {
        commandData.subcommand = subcommandName;
        if (subcommand.args) {
          const parsedArgs = parseCommandArgs(subcommand, parts.slice(2));
          Object.assign(commandData, parsedArgs);
        } else if (parts.length > 2) {
          commandData.args = parts.slice(2).join(' ');
          parts.slice(2).forEach((arg, idx) => {
            commandData[`arg${idx}`] = arg;
          });
        }
      } else {
        const parsedArgs = parseCommandArgs(engineCommand, parts.slice(1));
        Object.assign(commandData, parsedArgs);
      }
    } else if (engineCommand.args && engineCommand.args.length > 0) {
      const parsedArgs = parseCommandArgs(engineCommand, parts.slice(1));
      Object.assign(commandData, parsedArgs);
    }
    
    // Now call pre_hook with parsed commandData
    if (engineCommand.is_pre_hook && typeof engineCommand.pre_hook === 'function') {
      try {
        const hookResult = engineCommand.pre_hook(agentId, cmdline, commandData, ...parts.slice(1));
        if (typeof hookResult === 'string' && hookResult !== "") {
          addConsoleLine(agentId, { type: 'output', content: `[-] Error: ${hookResult}`, msgType: 2 });
        }
        // pre_hook handles command execution via ax.execute_alias, don't send original command
        return engineCommand;
      } catch (e) {
        console.error('[AgentContext] Pre-hook execution failed:', e);
        addConsoleLine(agentId, { type: 'output', content: `[-] Script Error: ${e.message}`, msgType: 2 });
        return engineCommand;
      }
    }
  } else {
    if (parts.length > 1) {
      commandData.subcommand = parts[1];
      if (parts.length > 2) commandData.args = parts.slice(2).join(' ');
    }
  }

  try {
    const dataJson = JSON.stringify(commandData);
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
    if (response.data && !response.data.ok) {
      addConsoleLine(agentId, {
        type: 'output',
        content: `[-] Error: ${response.data.message || 'Command rejected by server'}`,
        msgType: 2
      });
    }
    return engineCommand || { name: commandName };
  } catch (err) {
    console.error('[AgentContext] Command execution failed:', err);
    addConsoleLine(agentId, {
      type: 'output',
      content: `[-] Error: ${err.response?.data?.message || err.message || 'Command failed to send'}`,
      msgType: 2
    });
    return engineCommand || { name: commandName };
  }
};

export const AgentProvider = ({ children }) => {
  // Buffer limit constant (defined first for use in addConsoleLine)
  const CONSOLE_BUFFER_LIMIT = 1000;
  
  // Console history state (must be defined before addConsoleLine)
  const [consoleHistory, setConsoleHistory] = useState({});
  
  // Define addConsoleLine early to avoid TDZ issues
  const addConsoleLine = useCallback((agentId, line) => {
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
  }, []);

  // Use useCallback to wrap the external helpers
  const unserializeParams = useCallback(_unserializeParams, []);
  const parseCommandArgs = useCallback(_parseCommandArgs, []);

  const [agents, setAgents] = useState([]);
  // ... rest of the states
  const [axEngine] = useState(() => createAxEngine());
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
  // Note: consoleHistory is defined at top of component to avoid TDZ
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

  // Use a ref to track registered agents and their script loading status
  const loadedAgentScriptsRef = useRef(new Set());
  
  // Use refs to store callbacks to avoid TDZ in useEffect
  const processCommandRef = useRef(null);
  const addConsoleLineRef = useRef(null);
  const agentsRef = useRef([]);

  // --- Extension-Kit Engine Integration ---
  const reloadScripts = useCallback(async () => {
    if (!axEngine) return;
    try {
      // Clear tracking ref on global reload
      loadedAgentScriptsRef.current.clear();
      
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
  }, [axEngine]);

  // Update engine stats whenever commands are updated
  useEffect(() => {
    if (!axEngine) return;
    
    // Clear existing callbacks to prevent duplicate registration
    axEngine.setOnCommandsUpdated(null);
    axEngine.setOnExecuteCommand(null);
    axEngine.setOnConsoleMessage(null);

    axEngine.setOnCommandsUpdated(() => {
      setAxStats({
        loadedScripts: axEngine.loadedScripts.size,
        commandCount: axEngine.commands.size,
        pluginCount: axEngine.plugins.length
      });
      setAxCommands(Array.from(axEngine.commands.values()));
      setAxPlugins(axEngine.plugins);
    });

    axEngine.setOnExecuteCommand(async (agentId, cmdline, commandData, message, hook) => {
      // Debounce rapid executions
      const now = Date.now();
      const execKey = `${agentId}:${cmdline}:${JSON.stringify(commandData)}`;
      
      if (axEngine._lastExecKey === execKey && (now - axEngine._lastExecTime < 500)) {
        console.log(`[AgentContext] Debounced duplicate command: ${cmdline}`);
        return;
      }
      
      axEngine._lastExecKey = execKey;
      axEngine._lastExecTime = now;

      // Check if commandData is a direct alias command (e.g., shell -> ps run)
      if (commandData && typeof commandData === 'object' && commandData.__direct_cmdline) {
        // Direct execution mode: Send aliased command directly without re-parsing
        console.log(`[AgentContext] Direct alias execution: ${commandData.__direct_cmdline}`);
        const agent = agentsRef.current.find(a => a.a_id === agentId);
        if (!agent) return;
        
        try {
          const response = await agentApi.executeCommand({
            name: agent.a_name,
            id: agent.a_id,
            ui: false,
            cmdline: commandData.__direct_cmdline,
            data: "{}",
            ax_hook_id: "",
            ax_handler_id: ""
          });
          
          // Add Task issued message to console
          if (addConsoleLineRef.current && response?.data?.task_id) {
            addConsoleLineRef.current(agentId, { 
              type: 'task',
              taskId: response.data.task_id,
              cmdline: commandData.__original_cmdline || cmdline,
              completed: false,
              msgType: 5
            });
          }
          
          if (commandData.message && addConsoleLineRef.current) {
            addConsoleLineRef.current(agentId, { type: 'output', content: `[*] ${commandData.message}`, msgType: 0 });
          }
        } catch (err) {
          console.error('[AgentContext] Direct alias command failed:', err);
          if (addConsoleLineRef.current) {
            addConsoleLineRef.current(agentId, {
              type: 'output',
              content: `[-] Error: ${err.response?.data?.message || err.message}`,
              msgType: 2
            });
          }
        }
      } else if (commandData && typeof commandData === 'object' && commandData.command === 'execute' && commandData.bof_path) {
        // Notification Mode: Send bof_path + param_data directly to server
        console.log(`[AgentContext] Notification mode BOF: ${commandData.bof_path}`);
        const agent = agentsRef.current.find(a => a.a_id === agentId);
        if (!agent) return;
        
        try {
          const dataJson = JSON.stringify(commandData);
          await agentApi.executeCommand({
            name: agent.a_name,
            id: agent.a_id,
            ui: false,
            cmdline: cmdline,
            data: dataJson,
            ax_hook_id: "",
            ax_handler_id: ""
          });
          
          if (message && addConsoleLineRef.current) {
            addConsoleLineRef.current(agentId, { type: 'output', content: `[*] ${message}`, msgType: 0 });
          }
        } catch (err) {
          console.error('[AgentContext] Notification mode command failed:', err);
          if (addConsoleLineRef.current) {
            addConsoleLineRef.current(agentId, {
              type: 'output',
              content: `[-] Error: ${err.response?.data?.message || err.message}`,
              msgType: 2
            });
          }
        }
      } else {
        // Traditional mode: Use ref to access processCommand (avoids TDZ)
        console.log(`[AgentContext] Executing extension command: ${cmdline}`);
        if (processCommandRef.current) {
          processCommandRef.current(agentId, typeof commandData === 'string' ? commandData : cmdline);
        }
      }
    });

    axEngine.setOnConsoleMessage((agentId, message, type, clearText) => {
      addConsoleLine(agentId, {
        type: 'output',
        content: message,
        msgType: type,
        clearText: clearText
      });
    });

    return () => {
      if (axEngine) {
        axEngine.setOnCommandsUpdated(null);
        axEngine.setOnExecuteCommand(null);
        axEngine.setOnConsoleMessage(null);
      }
    };
  }, [axEngine]);

  useEffect(() => {
    if (!axInitialized.current && axEngine) {
      axInitialized.current = true;
      // Using a slightly longer delay to ensure complete module settlement
      const timer = setTimeout(() => {
        axEngine.setScriptApi(scriptApi);
        reloadScripts();
      }, 500);
      return () => clearTimeout(timer);
    }
  }, [reloadScripts, axEngine, scriptApi]);

  // Update engine with current agents for script context
  useEffect(() => {
    if (axEngine) {
      axEngine.setAgents(agents.reduce((acc, a) => ({ ...acc, [a.a_id]: a }), {}));
    }
  }, [agents, axEngine]);


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
    shell: { args: [{ name: 'command', type: 'string' }], description: 'Execute shell command' },
    run: { args: [{ name: 'command', type: 'string' }], description: 'Run command without shell' },
    powershell: { args: [{ name: 'command', type: 'string' }], description: 'Execute PowerShell command' },
    execute: {
      description: 'Execute various payloads',
      subcommands: [
        { name: 'bof', args: [{ name: 'bof', type: 'file' }, { name: 'args', type: 'string' }], description: 'Execute BOF' },
        { name: 'assembly', args: [{ name: 'assembly', type: 'file' }, { name: 'args', type: 'string' }], description: 'Execute .NET assembly' },
        { name: 'pe', args: [{ name: 'pe', type: 'file' }, { name: 'args', type: 'string' }], description: 'Execute PE' },
        { name: 'dll', args: [{ name: 'dll', type: 'file' }, { name: 'export', type: 'string' }, { name: 'args', type: 'string' }], description: 'Execute DLL' },
        { name: 'shellcode', args: [{ name: 'shellcode', type: 'file' }], description: 'Execute shellcode' }
      ]
    },
    inject: {
      description: 'Injection commands',
      subcommands: [
        { name: 'shellcode', args: [{ name: 'pid', type: 'int' }, { name: 'shellcode', type: 'file' }], description: 'Inject shellcode into process' },
        { name: 'dll', args: [{ name: 'pid', type: 'int' }, { name: 'dll', type: 'file' }], description: 'Inject DLL into process' }
      ]
    },
  };

  // --- Command Processing ---
  const processCommand = useCallback(async (agentId, cmdline) => {
    // Late-bind the execution logic to ensure no TDZ issues with internal variables
    const logic = _executeCommandLogic;
    const helpers = {
      parse: _parseCommandArgs,
      unserialize: _unserializeParams
    };
    return logic(agentId, cmdline, agents, axEngine, reloadScripts, addConsoleLine, helpers.parse, helpers.unserialize, BUILTIN_COMMANDS);
  }, [agents, axEngine, addConsoleLine, reloadScripts]);
  
  // Keep ref updated with latest processCommand
  useEffect(() => {
    processCommandRef.current = processCommand;
  }, [processCommand]);

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
            
            // PREVENT DUPLICATE EXECUTION: Check ref before loading agent script
            if (packet.ax && packet.listeners && axEngine) {
              if (!loadedAgentScriptsRef.current.has(packet.agent)) {
                loadedAgentScriptsRef.current.add(packet.agent);
                axEngine.loadAgentScript(packet.agent, packet.ax, packet.listeners);
              }
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

  // Keep refs updated with latest values
  useEffect(() => {
    agentsRef.current = agents;
  }, [agents]);

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
