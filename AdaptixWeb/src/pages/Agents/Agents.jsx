import React, { useState, useEffect, useRef } from 'react';
import { agentApi } from '../../api/agent';
import { useAgents } from '../../context/AgentContext';
import AgentConsole from './AgentConsole';
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
  const [activeDock, setActiveDock] = useState('listeners');
  const [tableHeight, setTableHeight] = useState(300); 
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterActiveOnly, setFilterActiveOnly] = useState(false);
  const [filterType, setFilterType] = useState('All types');
  const isResizing = useRef(false);
  const { openTabs, activeTabId, setActiveTabId, openAgentTab, closeTab } = useAgents();

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
    const handleMouseMove = (e) => {
      if (!isResizing.current) return;
      const newHeight = e.clientY - 48; // 减去 Top Toolbar 高度
      if (newHeight > 100 && newHeight < window.innerHeight - 200) {
        setTableHeight(newHeight);
      }
    };
    const handleMouseUp = () => {
      isResizing.current = false;
      document.body.style.cursor = 'default';
    };
    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    return () => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
    };
  }, []);

  useEffect(() => {
    const fetchAgents = async () => {
      try {
        setLoading(true);
        const response = await agentApi.list();
        // Server returns []AgentData directly
        setAgents(Array.isArray(response.data) ? response.data : []);
        setError(null);
      } catch (err) {
        console.error('Failed to fetch agents:', err);
        setError('Connection to Teamserver failed');
      } finally {
        setLoading(false);
      }
    };

    fetchAgents();
    const interval = setInterval(fetchAgents, 10000); 
    return () => clearInterval(interval);
  }, []);

  const handleContextMenu = (e, agent) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Interact', icon: Terminal, onClick: () => openAgentTab(agent, 'console') },
        { label: 'File Browser', icon: Files, onClick: () => openAgentTab(agent, 'files') },
        { label: 'Process List', icon: Activity, onClick: () => openAgentTab(agent, 'procs') },
        { divider: true },
        { label: 'Elevate...', icon: ShieldAlert, onClick: () => console.log('Elevate', agent.a_id) },
        { label: 'Exit Agent', icon: Power, onClick: () => console.log('Exit', agent.a_id) },
      ]
    });
  };

  return (
    <div className="flex flex-col h-full overflow-hidden bg-dark-900 select-none text-[#e0e0e0]" onClick={() => setMenu(null)}>
      {/* 1. Top Toolbar (Directly from Qt Client) */}
      <Toolbar 
        activeDock={activeDock} 
        onButtonClick={(id) => {
          if (id === 'sessions') {
            // Sessions button might reset filters or scroll to top
            setIsSearchVisible(false);
            setSearchQuery('');
            setFilterType('All types');
            setFilterActiveOnly(false);
          } else if (id === 'reconnect') {
            window.location.reload();
          } else {
            setActiveDock(id);
          }
        }} 
      />

      {/* 2. Main Content Splitter */}
      <div className="flex-1 flex flex-col min-h-0 overflow-hidden relative">
        {/* Top: Agent List Table */}
        <div 
          style={{ height: openTabs.length > 0 ? `${tableHeight}px` : '100%' }}
          className={cn(
            "flex flex-col border-b border-dark-700 transition-all duration-75 overflow-hidden",
            !openTabs.length && "flex-1"
          )}
        >
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

          <div className="flex-1 overflow-auto scrollbar-thin bg-dark-900">
            <table className="w-full text-left border-collapse table-fixed">
              <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
                <tr className="border-b border-dark-700 text-gray-400 text-[10px] font-bold uppercase tracking-tight">
                  <th className="py-1.5 px-4 w-40 border-r border-dark-700/50">External IP</th>
                  <th className="py-1.5 px-4 w-40 border-r border-dark-700/50">Internal IP</th>
                  <th className="py-1.5 px-4 w-32 border-r border-dark-700/50">User</th>
                  <th className="py-1.5 px-4 w-32 border-r border-dark-700/50">Computer</th>
                  <th className="py-1.5 px-4 border-r border-dark-700/50">OS Descriptor</th>
                  <th className="py-1.5 px-4 w-24 border-r border-dark-700/50">PID</th>
                  <th className="py-1.5 px-4 w-32 text-right">Last Check-in</th>
                </tr>
              </thead>
              <tbody className="text-[11px] font-medium">
                {filteredAgents.length === 0 ? (
                  <tr>
                    <td colSpan="7" className="px-6 py-20 text-center">
                      <div className="flex flex-col items-center space-y-3 opacity-20">
                        <Activity size={48} className="text-gray-600" />
                        <p className="text-sm font-medium tracking-widest uppercase">No active beacons matching criteria</p>
                      </div>
                    </td>
                  </tr>
                ) : (
                  filteredAgents.map((agent) => (
                    <tr 
                      key={agent.a_id} 
                      onDoubleClick={() => openAgentTab(agent)}
                      onContextMenu={(e) => handleContextMenu(e, agent)}
                      className={cn(
                        "border-b border-dark-800/50 hover:bg-accent-primary/5 transition-colors group cursor-pointer h-7",
                        activeTabId === agent.a_id && "bg-accent-primary/10 border-l-2 border-l-accent-primary"
                      )}
                    >
                      <td className="px-4 text-gray-400 font-mono truncate">{agent.a_external_ip || '---'}</td>
                      <td className="px-4 text-accent-primary font-mono font-bold truncate">{agent.a_internal_ip}</td>
                      <td className="px-4 text-gray-300 italic truncate">{agent.a_username}</td>
                      <td className="px-4 text-gray-300 truncate">{agent.a_computer || 'Unknown'}</td>
                      <td className="px-4 text-gray-400 truncate text-[10px]">{agent.a_os_desc}</td>
                      <td className="px-4 text-gray-500 font-mono truncate">{agent.a_pid || '---'}</td>
                      <td className="px-4 text-right text-gray-500 font-mono flex items-center justify-end space-x-2 pr-4">
                        <span>{Math.max(0, Math.floor(Date.now() / 1000) - agent.a_last_tick)}s</span>
                        <div className={cn(
                          "w-1.5 h-1.5 rounded-full flex-shrink-0",
                          (Math.floor(Date.now() / 1000) - agent.a_last_tick) < 60 ? 'bg-accent-secondary shadow-[0_0_6px_rgba(16,185,129,0.4)]' : 'bg-gray-600'
                        )} />
                      </td>
                    </tr>
                  ))
                )}
              </tbody>
            </table>
          </div>
        </div>

        {/* Resizer Handle */}
        {openTabs.length > 0 && (
          <div 
            className="h-1 bg-dark-700 hover:bg-accent-primary cursor-row-resize transition-colors z-20"
            onMouseDown={() => {
              isResizing.current = true;
              document.body.style.cursor = 'row-resize';
            }}
          />
        )}

        {/* Bottom: Tabs Area (Console / Multi-session) */}
        {openTabs.length > 0 && (
          <div className="flex-1 flex flex-col min-h-0 bg-dark-800 overflow-hidden">
            {/* Console Tab Headers */}
            <div className="flex overflow-x-auto no-scrollbar bg-dark-900/90 border-b border-dark-700 h-8">
              {openTabs.map((tab) => (
                <div
                  key={tab.a_id}
                  onClick={() => setActiveTabId(tab.a_id)}
                  className={cn(
                    "group flex items-center space-x-2 px-4 border-r border-dark-700 cursor-pointer transition-all min-w-[120px] max-w-[220px] h-8 relative",
                    activeTabId === tab.a_id 
                      ? "bg-dark-800 text-accent-primary border-t-2 border-t-accent-primary shadow-inner" 
                      : "text-gray-500 hover:bg-dark-800/50 hover:text-gray-400"
                  )}
                >
                  <Terminal className={cn("w-3 h-3 flex-shrink-0", activeTabId === tab.a_id ? "text-accent-primary" : "text-gray-600")} />
                  <span className="flex-1 truncate text-[10px] font-black font-mono tracking-tight uppercase">
                    {tab.a_name || tab.a_id.substring(0,8)}
                  </span>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      closeTab(tab.a_id);
                    }}
                    className="p-0.5 rounded hover:bg-dark-600 transition-colors opacity-0 group-hover:opacity-100"
                  >
                    <X className="w-3 h-3" />
                  </button>
                </div>
              ))}
            </div>

            <div className="flex-1 overflow-hidden">
              {openTabs.map(tab => (
                <div 
                  key={tab.a_id} 
                  className={cn("h-full", activeTabId === tab.a_id ? "block" : "hidden")}
                >
                  <AgentConsole agent={tab} />
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* 3. Global Dock (Bottom Fixed Panels) */}
      <Dock activeDock={activeDock} setActiveDock={setActiveDock} />

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default ControlPlatform;
