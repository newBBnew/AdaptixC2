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
  const [agentConfigs, setAgentConfigs] = useState({}); // Metadata for agent types
  const [listenerConfigs, setListenerConfigs] = useState({}); // Metadata for listener types
  
  const axInitialized = useRef(false);
  const consoleQueueRef = useRef({});
  const logQueueRef = useRef([]);
  const chatQueueRef = useRef([]);
  const { addListener } = useSocket();

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

      console.log('[AgentContext] Global data sync completed');
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
          }
          break;

        case PacketType.AGENT_LINK:
          // Handle agent peer linking (P2P/SMB beacons)
          console.log('[AgentContext] Agent Link update:', packet);
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
            const item = {
              type: 'output',
              content: packet.a_text || packet.a_message,
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
          console.log('[AgentContext] Task Hook received:', packet);
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
      setActiveSubTab
    }}>
      {children}
    </AgentContext.Provider>
  );
};
