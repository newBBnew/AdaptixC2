import React, { useState, useEffect } from 'react';
import { agentApi } from '../../api/agent';
import { useAgents } from '../../context/AgentContext';
import { useSocket } from '../../context/SocketContext';
import { useTheme } from '../../context/ThemeContext';
import SessionsGraph from './SessionsGraph';
import SetAgentDataDialog from './SetAgentDataDialog';
import ContextMenu from '../../components/ContextMenu';
import Toolbar from './Toolbar';
import Dock from './Dock';
import { 
  X, 
  Terminal, 
  Files, 
  Activity, 
  Info, 
  Power,
  ShieldAlert,
  Search,
  Filter,
  Monitor,
  Eye,
  EyeOff,
  ChevronRight,
  ChevronDown,
  RefreshCw
} from 'lucide-react';
import { clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';
import { cn } from '../../utils/cn';

const ControlPlatform = () => {
  const { theme } = useTheme();
  const [agents, setAgents] = useState([]);
  const [menu, setMenu] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isDataDialogOpen, setIsDataDialogOpen] = useState(false);
  const [selectedAgentForData, setSelectedAgentForData] = useState(null);
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterActiveOnly, setFilterActiveOnly] = useState(false);
  const [filterType, setFilterType] = useState('All types');
  const { agents: contextAgents, activeTabId, setActiveTabId, closeTab, openDockTab, openAgentTab, fetchAgents, isDockExpanded } = useAgents();
  const { isSyncing, syncProgress } = useSocket();

  const [viewMode, setViewMode] = useState('sessions'); // 'sessions' or 'graph'
  const [dockHeight, setDockHeight] = useState(300);
  const [isResizing, setIsResizing] = useState(false);

  useEffect(() => {
    const handleMouseMove = (e) => {
      if (!isResizing) return;
      const newHeight = window.innerHeight - e.clientY;
      if (newHeight > 40 && newHeight < window.innerHeight - 200) {
        setDockHeight(newHeight);
      }
    };
    const handleMouseUp = () => {
      setIsResizing(false);
      document.body.style.cursor = 'default';
    };
    if (isResizing) {
      window.addEventListener('mousemove', handleMouseMove);
      window.addEventListener('mouseup', handleMouseUp);
    }
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isResizing]);

  useEffect(() => {
    setAgents(contextAgents);
  }, [contextAgents]);

  // 获取所有 Agent 类型用于过滤器
  const agentTypes = ['All types', ...new Set(agents.map(a => a.a_name).filter(Boolean))].sort();

  // 过滤逻辑
  const filteredAgents = agents.filter(agent => {
    const matchesSearch = searchQuery === '' || 
      Object.values(agent).some(val => 
        String(val).toLowerCase().includes(searchQuery.toLowerCase())
      );
    const matchesType = filterType === 'All types' || agent.a_name === filterType;
    const matchesActive = !filterActiveOnly || (agent.a_last_tick && (Date.now() - new Date(agent.a_last_tick).getTime()) < 60000);
    
    return matchesSearch && matchesType && matchesActive;
  });

  useEffect(() => {
    const handleKeyDown = (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'f') {
        e.preventDefault();
        setIsSearchVisible(prev => !prev);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, []);

  useEffect(() => {
    fetchAgents();
    // Removed polling interval as we rely on WebSocket updates
    // const interval = setInterval(fetchAgents, 10000); 
    // return () => clearInterval(interval);
  }, [fetchAgents]);

  const handleSetColor = async (ids, reset = false) => {
    let bc = '', fc = '';
    if (!reset) {
      bc = window.prompt('Enter background color hex (e.g. #1a1a1a):', theme.colors.glassPanel.split(' ')[0].includes('#') ? theme.colors.glassPanel.split(' ')[0] : '#1a1a1a');
      if (!bc) return;
      fc = window.prompt('Enter foreground color hex (e.g. #ffffff):', theme.colors.primary);
      if (!fc) return;
    }
    try {
      await agentApi.setColor(ids, bc, fc, reset);
      fetchAgents();
    } catch (err) {
      console.error('Failed to set color:', err);
    }
  };

  const handleContextMenu = (e, agent) => {
    e.preventDefault();
    const ids = [agent.a_id];
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Console', icon: Terminal, onClick: () => openAgentTab(agent, 'console') },
        { divider: true },
        { 
          label: 'Agent', 
          icon: ChevronRight,
          children: [
            { label: 'Execute command', onClick: () => {
              const cmd = window.prompt('Enter command to execute:');
              if (cmd) {
                openAgentTab(agent, 'console');
                // Command will be executed via console
              }
            }},
            { label: 'Task manager', onClick: () => {
              openDockTab('tasks', 'tasks', 'Tasks');
            }},
            { divider: true },
            { label: 'Remove console data', onClick: () => {
              if (window.confirm('Clear console history for this agent?')) agentApi.removeConsole(ids);
            }},
            { label: 'Remove from server', color: 'text-theme-danger', onClick: () => {
              if (window.confirm('Are you sure you want to delete all information about this agent from the server?')) agentApi.remove(ids);
            }},
          ]
        },
        { 
          label: 'Session', 
          icon: ChevronRight,
          children: [
            { label: 'Mark as Active', onClick: () => agentApi.setMark(ids, '') },
            { label: 'Mark as Inactive', onClick: () => agentApi.setMark(ids, 'Inactive') },
            { divider: true },
            { label: 'Set data', onClick: () => {
              setSelectedAgentForData(agent);
              setIsDataDialogOpen(true);
            }},
            { label: 'Set tag...', onClick: () => {
              const tag = window.prompt('Enter tag:', agent.a_tags || '');
              if (tag !== null) agentApi.setTag(ids, tag);
            }},
            { divider: true },
            { label: 'Set items color', onClick: () => {
              const bc = window.prompt('Enter background color hex (e.g. #1a1a1a):', theme.colors.glassPanel.split(' ')[0].includes('#') ? theme.colors.glassPanel.split(' ')[0] : '#1a1a1a');
              if (bc) agentApi.setColor(ids, bc, '', false).then(fetchAgents);
            }},
            { label: 'Set text color', onClick: () => {
              const fc = window.prompt('Enter text color hex (e.g. #ffffff):', theme.colors.primary);
              if (fc) agentApi.setColor(ids, '', fc, false).then(fetchAgents);
            }},
            { label: 'Reset color', onClick: () => agentApi.setColor(ids, '', '', true).then(fetchAgents) },
            { divider: true },
            { label: 'Hide on client', onClick: () => {
              // Local hide - not persisted to server
              console.log('Hide agent:', agent.a_id);
            }},
          ]
        },
        { 
          label: 'Browsers', 
          icon: ChevronRight,
          children: [
            { label: 'File Browser', icon: Files, onClick: () => openAgentTab(agent, 'files') },
            { label: 'Process List', icon: Activity, onClick: () => openAgentTab(agent, 'procs') },
          ]
        },
      ]
    });
  };

  const handleToolbarClick = (dockId) => {
    if (dockId === 'reconnect') {
      window.location.reload();
      return;
    }
    if (dockId === 'sessions' || dockId === 'graph') {
      setViewMode(dockId);
      return;
    }

    // Map toolbar IDs to component types for Dock tabs
    const tabMap = {
      'listeners': { type: 'listeners', title: 'Listeners' },
      'logs': { type: 'logs', title: 'Logs' },
      'chat': { type: 'chat', title: 'Chat' },
      'tasks': { type: 'tasks', title: 'Tasks' },
      'tunnels': { type: 'tunnels', title: 'Tunnels' },
      'delivery': { type: 'delivery', title: 'Delivery' },
      'downloads': { type: 'downloads', title: 'Downloads' },
      'targets': { type: 'targets', title: 'Targets' },
      'creds': { type: 'creds', title: 'Credentials' },
      'screens': { type: 'screens', title: 'Screenshots' },
    };

    if (tabMap[dockId]) {
      openDockTab(dockId, tabMap[dockId].type, tabMap[dockId].title);
    }
  };

  const handleAgentClick = (agentId) => {
    setActiveTabId(agentId);
    openAgentTab(contextAgents.find(a => a.a_id === agentId));
  };

  return (
    <div className="flex flex-col h-full overflow-hidden select-none text-theme-primary" onClick={() => setMenu(null)}>
      {/* Sync Overlay */}
      {isSyncing && (
        <div className="absolute inset-0 z-[100] bg-theme-glass-panel/40 backdrop-blur-md flex items-center justify-center p-4">
          <div className="glass-card p-8 max-w-sm w-full space-y-6">
            <div className="flex items-center space-x-4">
              <div className="p-3 rounded-xl bg-theme-glass border border-theme-glass-light shadow-glow-sm">
                <RefreshCw className="w-6 h-6 text-theme-accent animate-spin" />
              </div>
              <div>
                <h3 className="text-base font-bold text-theme-primary">Synchronizing</h3>
                <p className="text-sm text-theme-muted">Establishing Teamserver Session...</p>
              </div>
            </div>
            
            <div className="space-y-2">
              <div className="flex justify-between text-xs font-medium text-theme-muted">
                <span>Data Objects</span>
                <span className="text-theme-accent font-semibold">{syncProgress.current} / {syncProgress.total}</span>
              </div>
              <div className="h-2 w-full glass-card-sm rounded-full overflow-hidden">
                <div 
                  className="h-full bg-gradient-to-r from-theme-accent to-theme-accent-secondary transition-all duration-300"
                  style={{ width: `${syncProgress.total > 0 ? (syncProgress.current / syncProgress.total) * 100 : 0}%` }}
                />
              </div>
            </div>
            
            <div className="pt-2 flex justify-center">
              <span className="text-xs text-theme-muted animate-pulse">
                Verification in progress...
              </span>
            </div>
          </div>
        </div>
      )}

      {/* Toolbar */}
      <Toolbar onButtonClick={handleToolbarClick} activeTabId={activeTabId} />

      <div className="flex-1 flex flex-col min-h-0 overflow-hidden relative">
        {/* Sessions/Graph Area */}
        <div className="flex-1 min-h-0 overflow-hidden glass-panel">
          {viewMode === 'sessions' ? (
            <div className="h-full flex flex-col">
              <div className="flex-1 overflow-auto custom-scrollbar">
                <table className="glass-table min-w-[1000px]">
                  <thead>
                    <tr>
                      <th>Agent Id</th>
                      <th>Type</th>
                      <th>External</th>
                      <th>Listener</th>
                      <th>Internal</th>
                      <th>Domain</th>
                      <th>Computer</th>
                      <th>User</th>
                      <th>OS</th>
                      <th>Process</th>
                      <th className="text-center">PID</th>
                      <th className="text-center">TID</th>
                      <th>Tags</th>
                      <th>Created</th>
                      <th className="text-center">Last</th>
                      <th className="text-center">Sleep</th>
                    </tr>
                  </thead>
                  <tbody className="text-[12px] font-medium">
                    {filteredAgents.length === 0 ? (
                      <tr>
                        <td colSpan="16" className="px-6 py-24 text-center border-none">
                          <div className="flex flex-col items-center space-y-4 opacity-20">
                            <Activity size={64} className="text-theme-muted" />
                            <p className="text-[10px] font-black uppercase tracking-[0.3em] text-theme-muted">No active beacons matching criteria</p>
                          </div>
                        </td>
                      </tr>
                    ) : (
                      filteredAgents.map((agent) => {
                        const username = agent.a_elevated ? `* ${agent.a_username}` : agent.a_username;
                        const userDisplay = agent.a_impersonated ? `${username} [${agent.a_impersonated}]` : username;
                        const process = agent.a_arch ? `${agent.a_process} (${agent.a_arch})` : agent.a_process;
                        const lastSec = Math.max(0, Math.floor(Date.now() / 1000) - agent.a_last_tick);
                        const sleepDisplay = agent.a_mark || `${agent.a_sleep}s (${agent.a_jitter}%)`;
                        return (
                        <tr 
                          key={agent.a_id} 
                          onDoubleClick={() => openAgentTab(agent)}
                          onContextMenu={(e) => handleContextMenu(e, agent)}
                          className={cn(
                            "transition-colors group cursor-pointer h-8",
                            activeTabId === agent.a_id && "bg-theme-hover"
                          )}
                          style={{ backgroundColor: agent.a_color?.split(';')[0] || undefined, color: agent.a_color?.split(';')[1] || undefined }}
                        >
                          <td className="text-theme-accent font-mono font-bold">{agent.a_id?.substring(0,8) || '---'}</td>
                          <td className="text-theme-primary font-semibold">{agent.a_name || '---'}</td>
                          <td className="text-theme-secondary font-mono">{agent.a_external_ip || '---'}</td>
                          <td className="text-theme-muted">{agent.a_listener || '---'}</td>
                          <td className="text-theme-secondary font-mono">{agent.a_internal_ip || '---'}</td>
                          <td className="text-theme-muted">{agent.a_domain || '---'}</td>
                          <td className="text-theme-primary">{agent.a_computer || '---'}</td>
                          <td className="text-theme-primary italic">{userDisplay || '---'}</td>
                          <td className="text-theme-secondary">{agent.a_os_desc || '---'}</td>
                          <td className="text-theme-muted">{process || '---'}</td>
                          <td className="text-theme-secondary font-mono text-center">{agent.a_pid || '---'}</td>
                          <td className="text-theme-muted font-mono text-center">{agent.a_tid || '---'}</td>
                          <td className="text-theme-accent-secondary font-bold">{agent.a_tags || '---'}</td>
                          <td className="text-theme-muted font-mono">{agent.a_create_time ? new Date(agent.a_create_time * 1000).toLocaleString() : '---'}</td>
                          <td className="text-theme-muted font-mono text-center">
                            <div className="flex items-center justify-center space-x-1">
                              <span>{lastSec}s</span>
                              <div className={cn("w-2 h-2 rounded-full", lastSec < 60 ? 'bg-theme-success shadow-glow-sm' : 'bg-theme-muted')} />
                            </div>
                          </td>
                          <td className="text-theme-secondary font-mono text-center">{sleepDisplay}</td>
                        </tr>
                      );})
                    )}
                  </tbody>
                </table>
              </div>
            </div>
          ) : (
            <SessionsGraph />
          )}
        </div>

        {/* Splitter */}
        {isDockExpanded && (
          <div 
            className="h-1 bg-theme-glass-light hover:bg-theme-accent/50 cursor-row-resize transition-colors z-20 flex items-center justify-center group"
            onMouseDown={() => {
              setIsResizing(true);
              document.body.style.cursor = 'row-resize';
            }}
          >
            <div className="w-10 h-0.5 bg-theme-glass rounded-full group-hover:bg-theme-accent transition-colors shadow-sm" />
          </div>
        )}

        {/* Dock Area */}
        <div 
          style={{ height: isDockExpanded ? `${dockHeight}px` : '40px' }} 
          className="shrink-0 transition-all duration-300 ease-in-out border-t border-theme-glass-light bg-theme-glass-panel"
        >
          <Dock />
        </div>
      </div>

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}

      {/* Dialogs */}
      <SetAgentDataDialog 
        isOpen={isDataDialogOpen}
        onClose={() => setIsDataDialogOpen(false)}
        agent={selectedAgentForData}
        onUpdated={fetchAgents}
      />
    </div>
  );
};

export default ControlPlatform;
