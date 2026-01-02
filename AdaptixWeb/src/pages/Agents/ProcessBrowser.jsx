import React, { useState, useEffect, useMemo } from 'react';
import { 
  Activity, 
  Search, 
  RefreshCw, 
  ChevronRight, 
  ChevronDown, 
  Shield, 
  Cpu, 
  MoreVertical,
  Layers,
  Crosshair,
  Copy,
  Trash2
} from 'lucide-react';
import ContextMenu from '../../components/ContextMenu';
import { useAgents } from '../../context/AgentContext';
import { agentApi } from '../../api/agent';
import { cn } from '../../utils/cn';

const ProcessBrowser = ({ agent }) => {
  const { browserData } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [viewMode, setViewMode] = useState('table'); // 'table' or 'tree'
  const [menu, setMenu] = useState(null);

  const data = browserData[agent.a_id] || { procs: [] };
  const processes = data.procs;

  const handleRefresh = async () => {
    setLoading(true);
    try {
      await agentApi.getProcesses(agent.a_id);
    } finally {
      setTimeout(() => setLoading(false), 1000);
    }
  };

  useEffect(() => {
    if (processes.length === 0) {
      handleRefresh();
    }
  }, [agent.a_id]);

  const filteredProcesses = useMemo(() => {
    if (!searchQuery) return processes;
    const query = searchQuery.toLowerCase();
    return processes.filter(p => 
      p.b_process_name?.toLowerCase().includes(query) || 
      p.b_pid?.toString().includes(query) ||
      p.b_context?.toLowerCase().includes(query)
    );
  }, [processes, searchQuery]);

  // Build tree structure
  const handleContextMenu = (e, process) => {
    e.preventDefault();
    const isCurrentAgent = process.b_pid.toString() === agent.a_pid;
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Inject', icon: Crosshair, disabled: isCurrentAgent, onClick: () => {
          agentApi.injectProcess(agent.a_id, process.b_pid);
        }},
        { label: 'Copy PID', icon: Copy, onClick: () => navigator.clipboard.writeText(process.b_pid.toString()) },
        { label: 'Copy Name', icon: Copy, onClick: () => navigator.clipboard.writeText(process.b_process_name) },
        { divider: true },
        { label: 'Kill process', icon: Trash2, color: 'text-accent-danger', disabled: isCurrentAgent, onClick: () => {
          if (window.confirm(`Kill process ${process.b_process_name} (${process.b_pid})?`)) {
            agentApi.killProcess(agent.a_id, process.b_pid);
          }
        }},
      ]
    });
  };

  const processTree = useMemo(() => {
    const nodes = {};
    const roots = [];

    processes.forEach(p => {
      nodes[p.b_pid] = { ...p, children: [] };
    });

    processes.forEach(p => {
      if (p.b_ppid && nodes[p.b_ppid]) {
        nodes[p.b_ppid].children.push(nodes[p.b_pid]);
      } else {
        roots.push(nodes[p.b_pid]);
      }
    });

    return roots;
  }, [processes]);

  const renderTreeItem = (node, depth = 0) => {
    const isCurrentAgent = node.b_pid.toString() === agent.a_pid;
    
    return (
      <React.Fragment key={node.b_pid}>
        <tr className={cn(
          "hover:bg-accent-primary/5 group h-8 cursor-default transition-colors border-b border-dark-800/30",
          isCurrentAgent && "bg-accent-danger/5"
        )}>
          <td className="px-4 font-mono text-[11px]">
            <div className="flex items-center" style={{ paddingLeft: `${depth * 16}px` }}>
              {node.children.length > 0 ? (
                <ChevronDown size={12} className="mr-1 text-gray-600" />
              ) : (
                <div className="w-4" />
              )}
              <span className={cn(isCurrentAgent ? "text-accent-danger font-bold" : "text-gray-300")}>
                {node.b_process_name}
              </span>
            </div>
          </td>
          <td className="px-4 text-center font-mono text-gray-500 text-[10px]">{node.b_pid}</td>
          <td className="px-4 text-center font-mono text-gray-500 text-[10px]">{node.b_ppid}</td>
          <td className="px-4 text-center font-mono text-gray-600 text-[10px]">{node.b_arch || '---'}</td>
          <td className="px-4 truncate text-gray-500 text-[10px]">{node.b_context || '---'}</td>
        </tr>
        {node.children.map(child => renderTreeItem(child, depth + 1))}
      </React.Fragment>
    );
  };

  return (
    <div className="flex flex-col h-full bg-[#0a0a0a] text-gray-300 font-sans select-none overflow-hidden">
      {/* 1. Toolbar */}
      <div className="flex items-center justify-between px-4 py-2 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-4 flex-1">
          <button 
            onClick={handleRefresh}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Reload process list"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
          
          <div className="relative flex-1 max-w-sm">
            <Search className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
            <input 
              type="text" 
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search by PID, Name or Context..." 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
            />
          </div>
        </div>

        <div className="flex items-center space-x-2 ml-4">
          <div className="flex bg-dark-900 rounded-lg p-0.5 border border-dark-700">
            <button 
              onClick={() => setViewMode('table')}
              className={cn(
                "px-3 py-1 rounded-md text-[10px] font-bold uppercase transition-all",
                viewMode === 'table' ? "bg-dark-700 text-accent-primary shadow-sm" : "text-gray-500 hover:text-gray-300"
              )}
            >
              List
            </button>
            <button 
              onClick={() => setViewMode('tree')}
              className={cn(
                "px-3 py-1 rounded-md text-[10px] font-bold uppercase transition-all",
                viewMode === 'tree' ? "bg-dark-700 text-accent-primary shadow-sm" : "text-gray-500 hover:text-gray-300"
              )}
            >
              Tree
            </button>
          </div>
        </div>
      </div>

      {/* 2. Process Table/Tree Area */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-fixed">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 w-16 border-r border-dark-700/30 text-center">PID</th>
              <th className="py-2 px-4 w-16 border-r border-dark-700/30 text-center">PPID</th>
              {agent.a_os === 1 ? (
                <>
                  <th className="py-2 px-4 w-16 border-r border-dark-700/30 text-center">Arch</th>
                  <th className="py-2 px-4 w-16 border-r border-dark-700/30 text-center">Session</th>
                </>
              ) : (
                <th className="py-2 px-4 w-20 border-r border-dark-700/30 text-center">TTY</th>
              )}
              <th className="py-2 px-4 w-48 border-r border-dark-700/30">Context</th>
              <th className="py-2 px-4">Process</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {viewMode === 'tree' ? (
              processTree.map(root => renderTreeItem(root))
            ) : (
              filteredProcesses.map((p) => {
                const isCurrentAgent = p.b_pid.toString() === agent.a_pid;
                return (
                  <tr 
                    key={p.b_pid}
                    onContextMenu={(e) => handleContextMenu(e, p)}
                    className={cn(
                      "hover:bg-accent-primary/5 group h-8 cursor-default transition-colors",
                      isCurrentAgent && "bg-accent-danger/5"
                    )}
                  >
                    <td className="px-4 text-center font-mono text-gray-500 text-[10px]">{p.b_pid}</td>
                    <td className="px-4 text-center font-mono text-gray-500 text-[10px]">{p.b_ppid}</td>
                    {agent.a_os === 1 ? (
                      <>
                        <td className="px-4 text-center font-mono text-gray-600 text-[10px]">{p.b_arch || '---'}</td>
                        <td className="px-4 text-center font-mono text-gray-600 text-[10px]">{p.b_session || '---'}</td>
                      </>
                    ) : (
                      <td className="px-4 text-center font-mono text-gray-600 text-[10px]">{p.b_tty || '---'}</td>
                    )}
                    <td className="px-4 truncate text-gray-500 text-[10px]">{p.b_context || '---'}</td>
                    <td className="px-4 flex items-center space-x-2 truncate">
                      <Cpu size={14} className={cn("shrink-0", isCurrentAgent ? "text-accent-danger" : "text-gray-600")} />
                      <span className={cn(isCurrentAgent ? "text-accent-danger font-bold" : "text-gray-300", "truncate font-mono")}>
                        {p.b_process_name}
                      </span>
                    </td>
                  </tr>
                );
              })
            )}
            {filteredProcesses.length === 0 && (
              <tr>
                <td colSpan={agent.a_os === 1 ? 6 : 5} className="py-20 text-center opacity-20">
                  <div className="flex flex-col items-center">
                    <Activity size={48} className="text-gray-600" />
                    <p className="mt-2 uppercase tracking-widest text-[10px]">No processes found</p>
                  </div>
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      {/* 3. Footer Status */}
      <div className="px-4 py-1 bg-dark-800 border-t border-dark-700 flex items-center justify-between text-[9px] font-black uppercase text-gray-500 tracking-tighter">
        <div className="flex items-center space-x-4">
          <span>Total Processes: <span className="text-accent-primary font-mono">{processes.length}</span></span>
          <div className="w-px h-2.5 bg-dark-600" />
          <span>Self: <span className="text-accent-danger font-mono">{agent.a_pid}</span></span>
        </div>
        <div className="flex items-center space-x-2">
          <Shield size={10} className="text-accent-secondary" />
          <span className="text-accent-secondary/80 tracking-widest">Real-time Telemetry</span>
        </div>
      </div>

      {menu && (
        <ContextMenu
          x={menu.x}
          y={menu.y}
          options={menu.options}
          onClose={() => setMenu(null)}
        />
      )}
    </div>
  );
};

export default ProcessBrowser;
