import React, { useState, useEffect } from 'react';
import { agentApi } from '../../api/agent';
import { useAgents } from '../../context/AgentContext';
import { useSocket } from '../../context/SocketContext';
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
  ChevronDown
} from 'lucide-react';
import { clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';
import { cn } from '../../utils/cn';

const ControlPlatform = () => {
  const [agents, setAgents] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [menu, setMenu] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [activeDock, setActiveDock] = useState('logs');
  const [viewMode, setViewMode] = useState('sessions'); // 'sessions' or 'graph'
  const [isDataDialogOpen, setIsDataDialogOpen] = useState(false);
  const [selectedAgentForData, setSelectedAgentForData] = useState(null);
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterActiveOnly, setFilterActiveOnly] = useState(false);
  const [filterType, setFilterType] = useState('All types');
  const { agents: contextAgents, openTabs, activeTabId, setActiveTabId, openAgentTab, closeTab } = useAgents();
  const { isSyncing, syncProgress } = useSocket();
  const fetchAgents = useAgents().fetchAgents;

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
    const interval = setInterval(fetchAgents, 10000); 
    return () => clearInterval(interval);
  }, [fetchAgents]);

  const handleSetColor = async (ids, reset = false) => {
    let bc = '', fc = '';
    if (!reset) {
      bc = window.prompt('Enter background color hex (e.g. #A01641):', '#A01641');
      if (!bc) return;
      fc = window.prompt('Enter foreground color hex (e.g. #FFFFFF):', '#FFFFFF');
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
              setActiveDock('tasks');
              // TODO: Set agent filter in tasks
            }},
            { divider: true },
            { label: 'Remove console data', onClick: () => {
              if (window.confirm('Clear console history for this agent?')) agentApi.removeConsole(ids);
            }},
            { label: 'Remove from server', color: 'text-accent-danger', onClick: () => {
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
              const bc = window.prompt('Enter background color hex (e.g. #A01641):', '#A01641');
              if (bc) agentApi.setColor(ids, bc, '', false).then(fetchAgents);
            }},
            { label: 'Set text color', onClick: () => {
              const fc = window.prompt('Enter text color hex (e.g. #FFFFFF):', '#FFFFFF');
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

  return (
    <div className="flex flex-col h-full overflow-hidden bg-dark-900 select-none text-[#e0e0e0]" onClick={() => setMenu(null)}>
      {/* Sync Overlay (Mimics Qt Sync Dialog) */}
      {isSyncing && (
        <div className="absolute inset-0 z-[100] bg-dark-900/80 backdrop-blur-sm flex items-center justify-center">
          <div className="bg-dark-800 border border-dark-700 p-8 rounded-2xl shadow-2xl max-w-md w-full mx-4 space-y-6">
            <div className="flex items-center space-x-4">
              <div className="p-3 bg-accent-primary/20 rounded-xl">
                <RefreshCw className="w-6 h-6 text-accent-primary animate-spin" />
              </div>
              <div>
                <h3 className="text-lg font-bold text-white tracking-tight">Synchronizing Data</h3>
                <p className="text-xs text-gray-500 uppercase tracking-widest font-black">Fetching established sessions...</p>
              </div>
            </div>
            
            <div className="space-y-2">
              <div className="flex justify-between text-[10px] font-mono text-gray-400 uppercase">
                <span>Synchronizing objects</span>
                <span>{syncProgress.current} / {syncProgress.total}</span>
              </div>
              <div className="h-1.5 w-full bg-dark-950 rounded-full overflow-hidden border border-dark-700">
                <div 
                  className="h-full bg-accent-primary transition-all duration-300 shadow-[0_0_10px_rgba(59,130,246,0.5)]"
                  style={{ width: `${syncProgress.total > 0 ? (syncProgress.current / syncProgress.total) * 100 : 0}%` }}
                />
              </div>
            </div>
            
            <div className="pt-2 flex justify-center">
              <span className="text-[9px] text-gray-600 font-mono animate-pulse uppercase tracking-tighter">
                Establishing encrypted tunnel to teamserver...
              </span>
            </div>
          </div>
        </div>
      )}

      {/* 1. Top Toolbar (Directly from Qt Client) */}
      <Toolbar 
        activeDock={activeDock} 
        onButtonClick={(id) => {
          if (id === 'sessions') {
            setViewMode('sessions');
            setIsSearchVisible(false);
            setSearchQuery('');
            setFilterType('All types');
            setFilterActiveOnly(false);
          } else if (id === 'graph') {
            setViewMode('graph');
          } else if (id === 'reconnect') {
            window.location.reload();
          } else {
            setActiveDock(id);
          }
        }} 
      />

      {/* 2. Main Content Splitter */}
      <div className="flex-1 flex flex-col min-h-0 overflow-hidden relative">
        {/* Top: Agent List Table - Now takes full remaining height */}
        <div className="flex-1 flex flex-col border-b border-dark-700 overflow-hidden">
          {/* Header Controls (Mimics SessionsTableWidget.cpp) */}
          <div className="flex items-center justify-between px-4 py-1.5 bg-dark-800/80 border-b border-dark-700">
            <div className="flex items-center space-x-4">
              <span className="text-[10px] font-black text-gray-500 uppercase tracking-widest flex items-center">
                <Activity className="w-3 h-3 mr-1.5 text-accent-primary" />
                Active Beacons
              </span>
              <div className="h-4 w-px bg-dark-600" />
              
              <button 
                onClick={() => setIsSearchVisible(!isSearchVisible)}
                className={cn(
                  "p-1 rounded transition-colors",
                  isSearchVisible ? "bg-accent-primary/20 text-accent-primary" : "text-gray-500 hover:text-gray-300"
                )}
                title="Toggle Search (Ctrl+F)"
              >
                <Search className="w-3.5 h-3.5" />
              </button>
            </div>

            <div className="flex items-center space-x-3">
              <span className="text-[10px] text-gray-500">
                Total: <span className="text-gray-300 font-mono">{agents.length}</span>
              </span>
              <span className="text-[10px] text-gray-500">
                Filtered: <span className="text-accent-primary font-mono">{filteredAgents.length}</span>
              </span>
            </div>
          </div>

          {/* Search Panel (Mimics SessionsTableWidget.cpp searchWidget) */}
          {isSearchVisible && (
            <div className="flex items-center px-4 py-2 bg-dark-800/40 border-b border-dark-700 space-x-4 animate-in slide-in-from-top-2 duration-200">
              <div className="relative flex-1 max-w-sm">
                <Filter className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
                <input 
                  type="text" 
                  autoFocus
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  placeholder="filter: (admin | root) & ^(test)" 
                  className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50 placeholder:text-gray-700"
                />
              </div>

              <select 
                value={filterType}
                onChange={(e) => setFilterType(e.target.value)}
                className="bg-dark-950/50 border border-dark-600 rounded px-2 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
              >
                {agentTypes.map(type => (
                  <option key={type} value={type}>{type}</option>
                ))}
              </select>

              <label className="flex items-center space-x-2 cursor-pointer group">
                <input 
                  type="checkbox" 
                  checked={filterActiveOnly}
                  onChange={(e) => setFilterActiveOnly(e.target.checked)}
                  className="sr-only"
                />
                <div className={cn(
                  "w-3 h-3 border border-dark-500 rounded-sm flex items-center justify-center transition-colors",
                  filterActiveOnly ? "bg-accent-primary border-accent-primary" : "group-hover:border-dark-400"
                )}>
                  {filterActiveOnly && <div className="w-1.5 h-1.5 bg-white rounded-full" />}
                </div>
                <span className="text-[11px] text-gray-400 group-hover:text-gray-200 transition-colors">Only Active</span>
              </label>

              <button 
                onClick={() => {
                  setSearchQuery('');
                  setFilterType('All types');
                  setFilterActiveOnly(false);
                }}
                className="text-[10px] text-gray-500 hover:text-accent-danger transition-colors uppercase font-bold tracking-tighter"
              >
                Reset
              </button>
            </div>
          )}

          <div className="flex-1 overflow-hidden bg-dark-900 relative">
            {viewMode === 'sessions' ? (
              <div className="h-full overflow-auto scrollbar-thin">
                <table className="w-full text-left border-collapse table-fixed">
                  <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
                    <tr className="border-b border-dark-700 text-gray-400 text-[10px] font-bold uppercase tracking-tight">
                      <th className="py-1.5 px-2 w-20 border-r border-dark-700/50">Agent Id</th>
                      <th className="py-1.5 px-2 w-16 border-r border-dark-700/50">Type</th>
                      <th className="py-1.5 px-2 w-24 border-r border-dark-700/50">External</th>
                      <th className="py-1.5 px-2 w-20 border-r border-dark-700/50">Listener</th>
                      <th className="py-1.5 px-2 w-24 border-r border-dark-700/50">Internal</th>
                      <th className="py-1.5 px-2 w-20 border-r border-dark-700/50">Domain</th>
                      <th className="py-1.5 px-2 w-20 border-r border-dark-700/50">Computer</th>
                      <th className="py-1.5 px-2 w-24 border-r border-dark-700/50">User</th>
                      <th className="py-1.5 px-2 w-28 border-r border-dark-700/50">OS</th>
                      <th className="py-1.5 px-2 w-28 border-r border-dark-700/50">Process</th>
                      <th className="py-1.5 px-2 w-12 border-r border-dark-700/50">PID</th>
                      <th className="py-1.5 px-2 w-12 border-r border-dark-700/50">TID</th>
                      <th className="py-1.5 px-2 w-16 border-r border-dark-700/50">Tags</th>
                      <th className="py-1.5 px-2 w-24 border-r border-dark-700/50">Created</th>
                      <th className="py-1.5 px-2 w-20 border-r border-dark-700/50">Last</th>
                      <th className="py-1.5 px-2 w-24">Sleep</th>
                    </tr>
                  </thead>
                  <tbody className="text-[11px] font-medium">
                    {filteredAgents.length === 0 ? (
                      <tr>
                        <td colSpan="16" className="px-6 py-20 text-center">
                          <div className="flex flex-col items-center space-y-3 opacity-20">
                            <Activity size={48} className="text-gray-600" />
                            <p className="text-sm font-medium tracking-widest uppercase">No active beacons matching criteria</p>
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
                            "border-b border-dark-800/50 hover:bg-accent-primary/5 transition-colors group cursor-pointer h-7",
                            activeTabId === agent.a_id && "bg-accent-primary/10 border-l-2 border-l-accent-primary"
                          )}
                          style={{ backgroundColor: agent.a_color?.split(';')[0] || undefined, color: agent.a_color?.split(';')[1] || undefined }}
                        >
                          <td className="px-2 text-accent-primary font-mono font-bold truncate text-[10px]">{agent.a_id?.substring(0,8) || '---'}</td>
                          <td className="px-2 text-gray-300 truncate text-[10px]">{agent.a_name || '---'}</td>
                          <td className="px-2 text-gray-400 font-mono truncate text-[10px]">{agent.a_external_ip || '---'}</td>
                          <td className="px-2 text-gray-500 truncate text-[10px]">{agent.a_listener || '---'}</td>
                          <td className="px-2 text-gray-400 font-mono truncate text-[10px]">{agent.a_internal_ip || '---'}</td>
                          <td className="px-2 text-gray-500 truncate text-[10px]">{agent.a_domain || '---'}</td>
                          <td className="px-2 text-gray-300 truncate text-[10px]">{agent.a_computer || '---'}</td>
                          <td className="px-2 text-gray-300 italic truncate text-[10px]">{userDisplay || '---'}</td>
                          <td className="px-2 text-gray-400 truncate text-[10px]">{agent.a_os_desc || '---'}</td>
                          <td className="px-2 text-gray-500 truncate text-[10px]">{process || '---'}</td>
                          <td className="px-2 text-gray-500 font-mono text-center text-[10px]">{agent.a_pid || '---'}</td>
                          <td className="px-2 text-gray-500 font-mono text-center text-[10px]">{agent.a_tid || '---'}</td>
                          <td className="px-2 text-accent-secondary truncate text-[10px]">{agent.a_tags || '---'}</td>
                          <td className="px-2 text-gray-500 font-mono truncate text-[10px]">{agent.a_create_time ? new Date(agent.a_create_time * 1000).toLocaleString() : '---'}</td>
                          <td className="px-2 text-gray-500 font-mono text-center text-[10px]">
                            <div className="flex items-center justify-center space-x-1">
                              <span>{lastSec}s</span>
                              <div className={cn("w-1.5 h-1.5 rounded-full", lastSec < 60 ? 'bg-accent-secondary' : 'bg-gray-600')} />
                            </div>
                          </td>
                          <td className="px-2 text-gray-400 font-mono text-center text-[10px]">{sleepDisplay}</td>
                        </tr>
                      );})
                    )}
                  </tbody>
                </table>
              </div>
            ) : (
              <SessionsGraph />
            )}
          </div>
        </div>

      </div>

      {/* 3. Global Dock (Bottom Fixed Panels) - Now includes Console tabs */}
      <Dock 
        activeDock={activeDock} 
        setActiveDock={(dock) => {
          setActiveDock(dock);
          setActiveTabId(null); // Clear active agent when switching to other dock
        }}
        openAgentTabs={openTabs}
        activeAgentId={activeTabId}
        onAgentTabChange={setActiveTabId}
        onAgentTabClose={closeTab}
      />

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
