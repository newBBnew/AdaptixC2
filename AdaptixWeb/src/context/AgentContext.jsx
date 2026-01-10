import React, { createContext, useContext, useState, useEffect, useCallback, useRef } from 'react';
import { useSocket } from './SocketContext';
import api, { agentApi } from '../api/agent';
import { listenerApi, taskApi, tunnelApi, deliveryApi, dataApi, scriptApi, pivotApi } from '../api/control';
import { PacketType } from '../constants/packetTypes';

const AgentContext = createContext();

export const useAgents = () => useContext(AgentContext);

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

const _buildHelpText = (cmdIndex, parts) => {
	const cmd = (parts[1] || '').toLowerCase();
	const sub = (parts[2] || '').toLowerCase();

	if (!cmd) {
		const names = Array.from(cmdIndex.keys()).sort();
		return `[*] Commands:\n${names.map(n => `- ${n}`).join('\n')}\n\n[*] Use: help <cmd> or help <cmd> <sub>`;
	}

	const cmdObj = cmdIndex.get(cmd);
	if (!cmdObj) {
		return `[-] Unknown command: ${cmd}`;
	}

	let target = cmdObj;
	if (sub) {
		const subMap = cmdObj.sub_commands || cmdObj.subCommands;
		const subList = Array.isArray(subMap) ? subMap : Object.values(subMap || {});
		const subObj = subList.find(s => String(s?.name || '').toLowerCase() === sub);
		if (!subObj) {
			return `[-] Unknown subcommand: ${cmd} ${sub}`;
		}
		target = subObj;
	}

	const desc = target.description || '';
	const ex = target.example || '';
	const args = Array.isArray(target.args) ? target.args : [];
	const argLines = args.map(a => {
		const n = a.name || '';
		const d = a.description || '';
		const t = a.type || '';
		return `- ${n} (${t}) ${d}`.trim();
	});

	let subsText = '';
	if (!sub) {
		const subMap = cmdObj.sub_commands || cmdObj.subCommands;
		const subList = Array.isArray(subMap) ? subMap : Object.values(subMap || {});
		const subNames = subList.map(s => String(s?.name || '')).filter(Boolean);
		if (subNames.length) {
			subsText = `\n[*] Subcommands: ${subNames.join(', ')}`;
		}
	}

	return `[*] ${sub ? `${cmd} ${sub}` : cmd}\n${desc}${ex ? `\n\nExample: ${ex}` : ''}${argLines.length ? `\n\nArgs:\n${argLines.join('\n')}` : ''}${subsText}`;
};

const _deriveDataFromCmdline = (parts) => {
	const cmd = (parts[0] || '').toLowerCase();
	const sub = (parts[1] || '').toLowerCase();
	const data = {};
	if (!cmd) return data;
	data.command = cmd;

	if (cmd === 'ls') {
		data.directory = parts.length > 1 ? parts[1] : '.';
	}
	if (cmd === 'cd') {
		if (parts.length > 1) data.path = parts[1];
	}
	if (cmd === 'cat') {
		if (parts.length > 1) data.path = parts[1];
	}
	if (cmd === 'download') {
		if (parts.length > 1) data.file = parts[1];
	}
	if (cmd === 'rm' || cmd === 'mkdir') {
		if (parts.length > 1) data.path = parts[1];
	}
	if (cmd === 'ps' && sub === 'list') {
		data.subcommand = 'list';
	}
	if (cmd === 'ps' && sub === 'kill') {
		data.subcommand = 'kill';
		if (parts.length > 2) data.pid = parseInt(parts[2]);
	}
	if (cmd === 'sleep') {
		if (parts.length > 1) data.sleep = String(parts[1]);
		if (parts.length > 2) {
			const j = parseInt(parts[2]);
			if (!Number.isNaN(j)) data.jitter = j;
		}
	}
	return data;
};

const _validateCommandCompliance = (cmdIndex, parts, data) => {
	const cmd = (parts[0] || '').toLowerCase();
	if (!cmd) return { ok: false, message: 'empty command' };

	if (cmd === 'help' || cmd === 'clear') return { ok: true };

	const cmdObj = cmdIndex.get(cmd);
	if (!cmdObj) {
		return { ok: false, message: `[-] Unknown command: ${cmd}` };
	}

	let target = cmdObj;
	let offset = 1;
	const subMap = cmdObj.sub_commands || cmdObj.subCommands;
	if (subMap && parts.length > 1) {
		const subToken = (parts[1] || '').toLowerCase();
		const subList = Array.isArray(subMap) ? subMap : Object.values(subMap || {});
		const subObj = subList.find(s => String(s?.name || '').toLowerCase() === subToken);
		if (subObj) {
			target = subObj;
			offset = 2;
		} else {
			// If command declares subcommands, block unknown subcommand locally to avoid sending malformed tasks
			return { ok: false, message: `[-] Unknown subcommand: ${cmd} ${subToken}` };
		}
	}

	const args = Array.isArray(target.args) ? target.args : [];
	const requiredArgs = args.filter(a => a && a.required === true && !(typeof a?.type === 'string' && a.type.startsWith('flag_')));
	for (const a of requiredArgs) {
		const name = String(a?.name || '').trim();
		if (!name) continue;
		const v = data ? data[name] : undefined;
		if (v === undefined || v === null) {
			return { ok: false, message: `[-] Missing required parameter: ${name}` };
		}
		if (typeof v === 'string' && v.trim() === '') {
			return { ok: false, message: `[-] Missing required parameter: ${name}` };
		}
	}

	if (cmd === 'sleep') {
		const v = data ? data.sleep : undefined;
		if (typeof v !== 'string' || v.trim() === '') {
			return { ok: false, message: `[-] Missing required parameter: sleep` };
		}
	}

	return { ok: true, offset };
};

const _deriveDataFromMetadata = (cmdIndex, parts) => {
	const cmd = (parts[0] || '').toLowerCase();
	if (!cmd) return {};

	const cmdObj = cmdIndex.get(cmd);
	if (!cmdObj) return {};

	let data = { command: cmd };
	let offset = 1;

	// Sub-command detection (only if declared by metadata)
	const subMap = cmdObj.sub_commands || cmdObj.subCommands;
	if (subMap && parts.length > 1) {
		const subToken = (parts[1] || '').toLowerCase();
		const subList = Array.isArray(subMap) ? subMap : Object.values(subMap || {});
		const subObj = subList.find(s => String(s?.name || '').toLowerCase() === subToken);
		if (subObj) {
			data.subcommand = subToken;
			offset = 2;
			// Prefer args declared on the subcommand
			if (Array.isArray(subObj.args)) {
				cmdObj.__resolvedArgs = subObj.args;
			}
		}
	}

	const args = Array.isArray(cmdObj.__resolvedArgs) ? cmdObj.__resolvedArgs : (Array.isArray(cmdObj.args) ? cmdObj.args : []);

	// Parse flags first (flag_* types)
	const flagDefs = args.filter(a => typeof a?.type === 'string' && a.type.startsWith('flag_') && a.mark);
	const positionalDefs = args.filter(a => !(typeof a?.type === 'string' && a.type.startsWith('flag_')));

	const tokens = parts.slice(offset);
	const consumed = new Set();

	for (let i = 0; i < tokens.length; i++) {
		const t = tokens[i];
		const def = flagDefs.find(fd => String(fd.mark) === t);
		if (!def) continue;
		const name = String(def.name || '').trim();
		if (!name) continue;
		const v = tokens[i + 1];
		if (typeof v === 'undefined') continue;
		consumed.add(i);
		consumed.add(i + 1);

		if (def.type === 'flag_int') {
			const n = parseInt(v);
			if (!Number.isNaN(n)) data[name] = n;
		} else {
			data[name] = v;
		}
		i++;
	}

	// Remaining tokens are positional
	const remaining = tokens.filter((_, idx) => !consumed.has(idx));
	let pi = 0;
	for (let ai = 0; ai < positionalDefs.length && pi < remaining.length; ai++) {
		const def = positionalDefs[ai];
		const name = String(def?.name || '').trim();
		if (!name) continue;
		const v = remaining[pi++];
		if (def.type === 'int') {
			const n = parseInt(v);
			if (!Number.isNaN(n)) data[name] = n;
		} else if (def.type === 'bool') {
			data[name] = v === 'true' || v === '1';
		} else {
			data[name] = v;
		}
	}

	return data;
};

export const AgentProvider = ({ children }) => {
  const CONSOLE_BUFFER_LIMIT = 1000;
  const LOGS_BUFFER_LIMIT = 500;
  const CHAT_BUFFER_LIMIT = 200;

  const [agents, setAgents] = useState([]);
  const [listeners, setListeners] = useState([]);
  const [tasks, setTasks] = useState({});
  const [logs, setLogs] = useState([]);
  const [credentials, setCredentials] = useState([]);
  const [targets, setTargets] = useState([]);
  const [downloads, setDownloads] = useState([]);
  const [screenshots, setScreenshots] = useState([]);
  const [fileDeliveries, setFileDeliveries] = useState({});
  const [tunnels, setTunnels] = useState([]);
  const [pivots, setPivots] = useState({});
  const [chatMessages, setChatMessages] = useState([]);
  const [browserData, setBrowserData] = useState({});
  const [availableListeners, setAvailableListeners] = useState([]);
  const [availableAgentTypes, setAvailableAgentTypes] = useState([]);
  
  const [consoleHistory, setConsoleHistory] = useState({});
  const [axCommands, setAxCommands] = useState([]);
  const [axPlugins, setAxPlugins] = useState([]);
  const [axStats, setAxStats] = useState({ loadedScripts: 0, commandCount: 0 });
  const [agentConfigs, setAgentConfigs] = useState({});
  const [listenerConfigs, setListenerConfigs] = useState({});
  const [globalSearchQuery, setGlobalSearchQuery] = useState('');

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

  const [isDockExpanded, setIsDockExpanded] = useState(true);
  const { addListener, sendMessage } = useSocket();

  const consoleQueueRef = useRef({});
  const logQueueRef = useRef([]);
  const chatQueueRef = useRef([]);
  const agentsRef = useRef([]);

  // Sync state refs
  useEffect(() => {
    agentsRef.current = agents;
  }, [agents]);

  // Persist tabs
  useEffect(() => {
    localStorage.setItem('adaptix_openTabs', JSON.stringify(openTabs));
  }, [openTabs]);

  useEffect(() => {
    localStorage.setItem('adaptix_activeTabId', activeTabId);
  }, [activeTabId]);

  // --- Console Logging ---
  const addConsoleLine = useCallback((agentId, line) => {
    setConsoleHistory(prev => {
      if (line.type === 'clear') return { ...prev, [agentId]: [] };
      const current = prev[agentId] || [];
      return {
        ...prev,
        [agentId]: [...current, line].slice(-CONSOLE_BUFFER_LIMIT)
      };
    });
  }, []);

	const cmdIndex = React.useMemo(() => {
		const idx = new Map();
		(axCommands || []).forEach(c => {
			if (!c?.name) return;
			idx.set(String(c.name).toLowerCase(), c);
		});
		// Minimal builtins for help output
		idx.set('help', { name: 'help', description: 'Show command help', example: 'help ls' });
		idx.set('clear', { name: 'clear', description: 'Clear console', example: 'clear' });
		idx.set('sleep', {
			name: 'sleep',
			description: 'Change beacon sleep interval and optional jitter',
			example: 'sleep 5 10',
			args: [
				{ name: 'sleep', type: 'string', required: true, description: 'Sleep time in seconds or duration (e.g., 5, 10s, 1m)' },
				{ name: 'jitter', type: 'int', required: false, description: 'Jitter percentage (0-100)' }
			]
		});
		return idx;
	}, [axCommands]);

  // --- Extension-Kit Sync (Gateway Side) ---
  const reloadScripts = useCallback(async () => {
    try {
      console.log('[AgentContext] Triggering Extension reload on Gateway...');
      await api.post('/extensions/reload');
      const res = await api.get('/extensions/metadata');
      if (res.data) {
        const cmds = Object.values(res.data.commands || {});
        const plugins = res.data.plugins || [];
        setAxCommands(cmds);
        setAxPlugins(plugins);
        setAxStats({
          loadedScripts: plugins.length,
          commandCount: cmds.length
        });
        console.log(`[AgentContext] Synced ${cmds.length} commands from Gateway`);
      }
    } catch (err) {
      console.error('[AgentContext] Failed to sync extensions from Gateway:', err);
    }
  }, []);

  useEffect(() => {
    reloadScripts();
  }, [reloadScripts]);

  // --- Command Processing (Gateway Interception) ---
  const processCommand = useCallback(async (agentId, cmdline, data = {}) => {
    const agent = agents.find(a => a.a_id === agentId);
    if (!agent) return;

    const parts = _unserializeParams(cmdline.trim());
    if (parts.length === 0) return;

    const commandName = parts[0].toLowerCase();
    
    if (commandName === 'clear') {
      addConsoleLine(agentId, { type: 'clear' });
      return;
    }

		// Local help: do not forward to C2 (extenders may not implement 'help')
		if (commandName === 'help') {
			const helpText = _buildHelpText(cmdIndex, parts);
			addConsoleLine(agentId, {
				type: 'output',
				content: helpText,
				msgType: 5
			});
			return { ok: true, message: 'local help' };
		}

		// Derive minimal args for common built-in commands when caller didn't provide parsed data
		const derivedMeta = _deriveDataFromMetadata(cmdIndex, parts);
		const derivedBuiltin = _deriveDataFromCmdline(parts);
		data = { ...derivedBuiltin, ...derivedMeta, ...(data || {}) };

		const compliance = _validateCommandCompliance(cmdIndex, parts, data);
		if (!compliance.ok) {
			addConsoleLine(agentId, {
				type: 'output',
				content: `${compliance.message}\n\n${_buildHelpText(cmdIndex, ['help', parts[0], parts[1]])}`,
				msgType: 2
			});
			return { ok: false, message: compliance.message };
		}

    try {
      // Direct call to Gateway Interceptor
      // Passing data as object, Gateway will marshal to string for C2
      const response = await agentApi.executeCommand({
        id: agent.a_id,
        name: agent.a_name,
        command: commandName,
        cmdline: cmdline,
        data: data, 
        ui: true,
        ax_hook_id: "",
        ax_handler_id: ""
      });

      if (response.data && !response.data.ok) {
        addConsoleLine(agentId, {
          type: 'output',
          content: `[-] Error: ${response.data.message || 'Command rejected'}`,
          msgType: 2
        });
      } else if (response.data && response.data.ok) {
        // Provide immediate feedback for commands handled by hooks or successfully forwarded
        const feedback = response.data.message || 'Command issued';
        addConsoleLine(agentId, {
          type: 'output',
          content: `[*] ${feedback}: ${cmdline}`,
          msgType: 5
        });
      }
      return response.data;
    } catch (err) {
      addConsoleLine(agentId, {
        type: 'output',
        content: `[-] Error: ${err.response?.data?.message || err.message}`,
        msgType: 2
      });
      return { ok: false, error: err };
    }
  }, [agents, addConsoleLine]);

  // --- WebSocket Handling ---
  useEffect(() => {
    const removeListener = addListener((packet) => {
      const type = packet.type;
      
      switch (type) {
        case PacketType.AGENT_NEW:
          setAgents(prev => prev.find(a => a.a_id === packet.a_id) ? prev : [...prev, packet]);
          break;
        case PacketType.AGENT_TICK:
          if (Array.isArray(packet.a_id)) {
            setAgents(prev => prev.map(a => 
              packet.a_id.includes(a.a_id) ? { ...a, a_last_tick: Math.floor(Date.now() / 1000) } : a
            ));
          }
          break;
        case PacketType.AGENT_UPDATE:
          setAgents(prev => prev.map(a => a.a_id === packet.a_id ? { ...a, ...packet } : a));
          break;
        case PacketType.AGENT_REMOVE:
          setAgents(prev => prev.filter(a => a.a_id !== packet.a_id));
          break;
        case PacketType.AGENT_CONSOLE_OUT:
          {
            let content = packet.a_text || packet.a_message;
            if (packet.a_base64 && content) {
              try { content = atob(content); } catch (e) {}
            }
            const item = {
              type: 'output',
              content: content,
              msgType: packet.a_msg_type,
              time: packet.time || Math.floor(Date.now() / 1000)
            };
            if (!consoleQueueRef.current[packet.a_id]) consoleQueueRef.current[packet.a_id] = [];
            consoleQueueRef.current[packet.a_id].push(item);
          }
          break;
        case PacketType.AGENT_CONSOLE_TASK_SYNC:
        case PacketType.AGENT_CONSOLE_TASK_UPD:
          {
            const item = {
              type: 'task',
              taskId: packet.a_task_id,
              cmdline: packet.a_cmdline,
              completed: packet.a_completed,
              msgType: packet.a_msg_type,
              content: packet.a_text || packet.a_message,
              time: packet.a_start_time || packet.a_finish_time || Math.floor(Date.now() / 1000)
            };
            if (!consoleQueueRef.current[packet.a_id]) consoleQueueRef.current[packet.a_id] = [];
            consoleQueueRef.current[packet.a_id].push(item);
          }
          break;
        case PacketType.AGENT_TASK_SYNC:
        case PacketType.AGENT_TASK_UPDATE:
          setTasks(prev => ({
            ...prev,
            [packet.a_task_id]: { 
              ...packet, 
              Status: packet.a_completed ? (packet.a_msg_type === 2 || packet.a_msg_type === 4 ? "Error" : "Success") : "Running",
              a_output: packet.a_text || packet.a_message
            }
          }));
          break;
        case PacketType.SP_TYPE_EVENT:
          logQueueRef.current.push({
            type: packet.event_type || 0,
            time: packet.date || packet.time || Math.floor(Date.now() / 1000),
            content: packet.message || packet.data
          });
          break;
        case PacketType.LISTENER_REG:
          setAvailableListeners(prev => {
            const exists = prev.find(l => l.id === packet.l_type);
            const newItem = {
              id: packet.l_type,
              label: packet.l_name || packet.l_type,
              protocol: packet.l_protocol,
              ax: packet.l_ax,
              ui_schema: packet.ui_schema
            };
            
            if (exists) {
              return prev.map(l => l.id === packet.l_type ? newItem : l);
            }
            return [...prev, newItem];
          });
          break;
        case PacketType.AGENT_REG:
          setAvailableAgentTypes(prev => {
            const exists = prev.find(a => a.id === packet.agent);
            const newItem = {
              id: packet.agent,
              label: packet.agent, // You might want to parse a friendlier name if available
              ax: packet.ax,
              ui_schema: packet.ui_schema,
              listeners: packet.listeners
            };

            if (exists) {
              return prev.map(a => a.id === packet.agent ? newItem : a);
            }
            return [...prev, newItem];
          });
          break;
        case PacketType.LISTENER_START:
          setListeners(prev => prev.find(l => l.l_name === packet.l_name) ? prev : [...prev, packet]);
          break;
        case PacketType.LISTENER_STOP:
          setListeners(prev => prev.filter(l => l.l_name !== packet.l_name));
          break;
        case PacketType.LISTENER_EDIT:
          setListeners(prev => prev.map(l => l.l_name === packet.l_name ? { ...l, ...packet } : l));
          break;
        case PacketType.BROWSER_DISKS:
          {
            const aid = packet.b_agent_id || packet.a_id;
            if (!aid) break;
            let disks = [];
            try { disks = packet.b_data ? JSON.parse(packet.b_data) : []; } catch (e) { disks = []; }
            setBrowserData(prev => ({
              ...prev,
              [aid]: {
                ...(prev[aid] || { files: [], procs: [] }),
                disks
              }
            }));
            if (!consoleQueueRef.current[aid]) consoleQueueRef.current[aid] = [];
            const max = 50;
            const list = (Array.isArray(disks) ? disks : []).slice(0, max).map(d => {
              const name = d?.b_name ?? '';
              const type = d?.b_type ?? '';
              return `${name}${type ? `\t${type}` : ''}`;
            });
            const content = [
              `disks: ${Array.isArray(disks) ? disks.length : 0} item(s)`,
              ...list,
              (Array.isArray(disks) && disks.length > max) ? `... (${disks.length - max} more)` : ''
            ].filter(Boolean).join('\n');
            consoleQueueRef.current[aid].push({
              type: 'output',
              content,
              msgType: 10,
              time: packet.time || Math.floor(Date.now() / 1000)
            });
          }
          break;
        case PacketType.BROWSER_FILES:
          {
            const aid = packet.b_agent_id || packet.a_id;
            if (!aid) break;
            let files = [];
            try { files = packet.b_data ? JSON.parse(packet.b_data) : []; } catch (e) { files = []; }
            setBrowserData(prev => ({
              ...prev,
              [aid]: {
                ...(prev[aid] || { disks: [], procs: [] }),
                currentPath: packet.b_path,
                files
              }
            }));
            if (!consoleQueueRef.current[aid]) consoleQueueRef.current[aid] = [];
            consoleQueueRef.current[aid].push({
              type: 'info',
              content: `FileBrowser 已更新：${packet.b_path || ''}（${Array.isArray(files) ? files.length : 0} 项）`,
              time: packet.time || Math.floor(Date.now() / 1000)
            });
            const max = 50;
            const list = (Array.isArray(files) ? files : []).slice(0, max).map(f => {
              const name = f?.b_filename ?? '';
              if (f?.b_is_dir) return `<DIR>\t${name}`;
              const size = (typeof f?.b_size === 'number') ? String(f.b_size) : '';
              return `${size || '0'}\t${name}`;
            });
            const content = [
              `ls ${packet.b_path || ''}: ${Array.isArray(files) ? files.length : 0} item(s)`,
              ...list,
              (Array.isArray(files) && files.length > max) ? `... (${files.length - max} more)` : ''
            ].filter(Boolean).join('\n');
            consoleQueueRef.current[aid].push({
              type: 'output',
              content,
              msgType: 10,
              time: packet.time || Math.floor(Date.now() / 1000)
            });
          }
          break;
        case PacketType.BROWSER_PROCESS:
          {
            const aid = packet.b_agent_id || packet.a_id;
            if (!aid) break;
            let procs = [];
            try { procs = packet.b_data ? JSON.parse(packet.b_data) : []; } catch (e) { procs = []; }
            setBrowserData(prev => ({
              ...prev,
              [aid]: {
                ...(prev[aid] || { disks: [], files: [] }),
                procs
              }
            }));
            if (!consoleQueueRef.current[aid]) consoleQueueRef.current[aid] = [];
            const max = 50;
            const list = (Array.isArray(procs) ? procs : []).slice(0, max).map(p => {
              const pid = p?.b_pid ?? '';
              const ppid = p?.b_ppid ?? '';
              const ctx = p?.b_context ?? '';
              const name = p?.b_process_name ?? '';
              return `${pid}\t${ppid}\t${ctx}\t${name}`;
            });
            const content = [
              `ps list: ${Array.isArray(procs) ? procs.length : 0} process(es)`,
              'PID\tPPID\tCONTEXT\tNAME',
              ...list,
              (Array.isArray(procs) && procs.length > max) ? `... (${procs.length - max} more)` : ''
            ].filter(Boolean).join('\n');
            consoleQueueRef.current[aid].push({
              type: 'output',
              content,
              msgType: 10,
              time: packet.time || Math.floor(Date.now() / 1000)
            });
          }
          break;
        case PacketType.CREDS_DELETE:
          if (packet.cred_id_array) {
            setCredentials(prev => prev.filter(c => !packet.cred_id_array.includes(c.cred_id)));
          }
          break;
        case PacketType.TARGETS_CREATE:
          setTargets(prev => [...prev, packet]);
          break;
        case PacketType.TARGETS_DELETE:
          if (packet.target_id_array) {
            setTargets(prev => prev.filter(t => !packet.target_id_array.includes(t.t_target_id)));
          }
          break;
        case PacketType.TUNNEL_CREATE:
          setTunnels(prev => [...prev, packet]);
          break;
        case PacketType.TUNNEL_DELETE:
          if (packet.tunnel_id_array) {
            setTunnels(prev => prev.filter(t => !packet.tunnel_id_array.includes(t.p_tunnel_id)));
          } else if (packet.p_tunnel_id) {
            setTunnels(prev => prev.filter(t => t.p_tunnel_id !== packet.p_tunnel_id));
          }
          break;
        case PacketType.TUNNEL_EDIT:
          setTunnels(prev => prev.map(t => t.p_tunnel_id === packet.p_tunnel_id ? { ...t, ...packet } : t));
          break;
        case PacketType.DOWNLOAD_CREATE:
          setDownloads(prev => [...prev, {
            ...packet,
            d_state: packet.d_state || 1, // Default to Running (1)
            d_recv_size: packet.d_recv_size || 0
          }]);
          break;
        case PacketType.DOWNLOAD_UPDATE:
          setDownloads(prev => prev.map(d => d.d_file_id === packet.d_file_id ? { ...d, ...packet } : d));
          break;
        case PacketType.DOWNLOAD_DELETE:
          if (packet.d_files_id) {
            setDownloads(prev => prev.filter(d => !packet.d_files_id.includes(d.d_file_id)));
          }
          break;
        case PacketType.SCREEN_CREATE:
          setScreenshots(prev => [...prev, packet]);
          break;
        case PacketType.SCREEN_UPDATE:
          setScreenshots(prev => prev.map(s => s.s_screen_id === packet.s_screen_id ? { ...s, ...packet } : s));
          break;
        case PacketType.SCREEN_DELETE:
          setScreenshots(prev => prev.filter(s => s.s_screen_id !== packet.s_screen_id));
          break;
        case PacketType.CHAT_MESSAGE:
          {
            const msg = {
              username: packet.c_username,
              message: packet.c_message,
              time: packet.c_date || Math.floor(Date.now() / 1000)
            };
            setChatMessages(prev => [...prev, msg].slice(-CHAT_BUFFER_LIMIT));
          }
          break;
        case PacketType.FILEDELIVERY_CREATE:
          setFileDeliveries(prev => ({ ...prev, [packet.file_id]: packet }));
          break;
        case PacketType.FILEDELIVERY_DELETE:
          setFileDeliveries(prev => {
            const next = { ...prev };
            delete next[packet.file_id];
            return next;
          });
          break;
        case PacketType.FILEDELIVERY_UPDATE:
          setFileDeliveries(prev => ({ ...prev, [packet.file_id]: { ...prev[packet.file_id], ...packet } }));
          break;
        case PacketType.PIVOT_CREATE:
          setPivots(prev => ({ ...prev, [packet.p_pivot_id]: packet }));
          break;
        case PacketType.PIVOT_DELETE:
          setPivots(prev => {
            const next = { ...prev };
            delete next[packet.p_pivot_id];
            return next;
          });
          break;
        default:
          break;
      }
    });
    return () => removeListener();
  }, [addListener]);

  // Batch updates for performance
  useEffect(() => {
    const interval = setInterval(() => {
      const queuedConsole = consoleQueueRef.current;
      if (Object.keys(queuedConsole).length > 0) {
        consoleQueueRef.current = {};
        setConsoleHistory(prev => {
          const next = { ...prev };
          Object.entries(queuedConsole).forEach(([aid, newLines]) => {
            next[aid] = [...(next[aid] || []), ...newLines].slice(-CONSOLE_BUFFER_LIMIT);
          });
          return next;
        });
      }

      const queuedLogs = logQueueRef.current;
      if (queuedLogs.length > 0) {
        logQueueRef.current = [];
        setLogs(prev => [...prev, ...queuedLogs].slice(-LOGS_BUFFER_LIMIT));
      }
    }, 200);
    return () => clearInterval(interval);
  }, []);

  const syncAllData = useCallback(async () => {
    try {
      const [agentsRes, listenersRes, targetsRes, credsRes, pivotsRes] = await Promise.all([
        agentApi.list(),
        listenerApi.list(),
        dataApi.targets(),
        dataApi.creds(),
        pivotApi.list()
      ]);
      setAgents(Array.isArray(agentsRes.data) ? agentsRes.data : []);
      setListeners(Array.isArray(listenersRes.data) ? listenersRes.data : []);
      setTargets(Array.isArray(targetsRes.data) ? targetsRes.data : []);
      setCredentials(Array.isArray(credsRes.data) ? credsRes.data : []);
      
      if (Array.isArray(pivotsRes.data)) {
        const pivotMap = {};
        pivotsRes.data.forEach(p => {
          pivotMap[p.p_pivot_id] = p;
        });
        setPivots(pivotMap);
      }
    } catch (err) {
      console.error('[AgentContext] Sync failed:', err);
    }
  }, []);

  useEffect(() => { syncAllData(); }, [syncAllData]);

  // Tab Management
  const openAgentTab = (agent, subTab = 'console') => {
    if (!agent || !agent.a_id) {
      console.error('[AgentContext] Cannot open tab: Invalid agent object', agent);
      return;
    }
    const agentId = agent.a_id;
    const existingTab = openTabs.find(t => t.id === agentId);
    if (!existingTab) {
      setOpenTabs([...openTabs, { 
        ...agent, 
        id: agentId, 
        type: 'agent', 
        title: `${agent.a_name || 'Agent'} (${agentId.substring(0, 8)})`,
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
    if (id === 'logs') return;
    const newTabs = openTabs.filter(t => t.id !== id);
    setOpenTabs(newTabs);
    if (activeTabId === id && newTabs.length > 0) {
      setActiveTabId(newTabs[newTabs.length - 1].id);
    }
  };

  return (
    <AgentContext.Provider value={{ 
      agents, listeners, tasks, logs, credentials, targets, downloads, screenshots,
      browserData,
      openTabs, activeTabId, setActiveTabId, openAgentTab, closeTab, openDockTab, setActiveSubTab,
      isDockExpanded, setIsDockExpanded, consoleHistory, addConsoleLine,
      axCommands, axPlugins, axStats, reloadScripts, processCommand, fetchAgents: syncAllData,
      globalSearchQuery, setGlobalSearchQuery, availableListeners, availableAgentTypes
    }}>
      {children}
    </AgentContext.Provider>
  );
};
