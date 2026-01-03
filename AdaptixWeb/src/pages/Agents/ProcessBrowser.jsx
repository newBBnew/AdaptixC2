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
        { label: 'Kill process', icon: Trash2, color: 'text-theme-danger', disabled: isCurrentAgent, onClick: () => {
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
          "hover:bg-theme-glass group h-10 cursor-default transition-colors border-b border-theme-glass-light",
          isCurrentAgent && "bg-theme-accent/5"
        )}>
          <td className="px-4 font-mono text-[11px]">
            <div className="flex items-center" style={{ paddingLeft: `${depth * 16}px` }}>
              {node.children.length > 0 ? (
                <ChevronDown size={12} className="mr-2 text-theme-muted" />
              ) : (
                <div className="w-5" />
              )}
              <span className={cn(isCurrentAgent ? "text-theme-accent font-black uppercase tracking-tight" : "text-theme-primary font-bold")}>
                {node.b_process_name}
              </span>
            </div>
          </td>
          <td className="px-4 text-center font-mono text-theme-muted text-[10px]">{node.b_pid}</td>
          <td className="px-4 text-center font-mono text-theme-muted text-[10px]">{node.b_ppid}</td>
          <td className="px-4 text-center font-mono text-theme-accent-secondary text-[10px]">{node.b_arch || '---'}</td>
          <td className="px-4 truncate text-theme-secondary text-[10px] uppercase font-bold tracking-tight">{node.b_context || '---'}</td>
        </tr>
        {node.children.map(child => renderTreeItem(child, depth + 1))}
      </React.Fragment>
    );
  };

  return (
    <div className="flex flex-col h-full select-none overflow-hidden">
      {/* 1. Toolbar */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3 flex-1 max-w-2xl">
          <button 
            onClick={handleRefresh}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh Processes"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
          
          <div className="relative flex-1 max-w-md">
            <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Filter processes..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
        </div>

        <div className="flex items-center space-x-3 ml-4">
          <div className="flex items-center glass-btn rounded-xl p-1 bg-theme-glass border border-theme-glass-light">
            <button 
              onClick={() => setViewMode('table')}
              className={cn(
                "px-4 py-1.5 rounded-lg text-xs font-black uppercase tracking-widest transition-all",
                viewMode === 'table' ? "bg-theme-accent text-theme-primary shadow-glow-sm" : "text-theme-muted hover:text-theme-secondary"
              )}
            >
              List
            </button>
            <button 
              onClick={() => setViewMode('tree')}
              className={cn(
                "px-4 py-1.5 rounded-lg text-xs font-black uppercase tracking-widest transition-all",
                viewMode === 'tree' ? "bg-theme-accent text-theme-primary shadow-glow-sm" : "text-theme-muted hover:text-theme-secondary"
              )}
            >
              Tree
            </button>
          </div>
        </div>
      </div>

      {/* 2. Process Table/Tree Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[800px]">
          <thead>
            <tr>
              <th className="w-24 text-center">PID</th>
              <th className="w-24 text-center">PPID</th>
              {agent.a_os === 1 ? (
                <>
                  <th className="w-24 text-center">Arch</th>
                  <th className="w-24 text-center">Session</th>
                </>
              ) : (
                <th className="w-24 text-center">TTY_IO</th>
              )}
              <th className="w-64">Operational Context</th>
              <th>Process Executable</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium">
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
                      "transition-colors group h-10 cursor-default border-b border-theme-glass-light hover:bg-theme-glass",
                      isCurrentAgent ? "bg-theme-accent/5" : ""
                    )}
                  >
                    <td className={cn("text-center font-mono text-[10px]", isCurrentAgent ? "text-theme-accent font-black" : "text-theme-muted")}>
                      {p.b_pid}
                    </td>
                    <td className="text-center font-mono text-theme-muted text-[10px] opacity-60">{p.b_ppid}</td>
                    {agent.a_os === 1 ? (
                      <>
                        <td className="text-center">
                          <span className="px-2 py-0.5 rounded-lg bg-theme-glass-panel text-[9px] font-black uppercase text-theme-accent-secondary border border-theme-glass-light shadow-sm">
                            {p.b_arch || 'X64'}
                          </span>
                        </td>
                        <td className="text-center font-mono text-theme-muted text-[10px]">{p.b_session ?? '0'}</td>
                      </>
                    ) : (
                      <td className="text-center font-mono text-theme-muted text-[10px]">{p.b_tty || 'N/A'}</td>
                    )}
                    <td className="truncate max-w-xs">
                      <span className={cn("text-[10px] font-black uppercase tracking-widest", isCurrentAgent ? "text-theme-accent" : "text-theme-secondary opacity-80")}>
                        {p.b_context || 'UNKNOWN_IDENTITY'}
                      </span>
                    </td>
                    <td className="flex items-center space-x-3">
                      <div className="shrink-0 p-1.5 bg-theme-glass-panel border border-theme-glass-light rounded-lg">
                        <Cpu size={14} className={cn(isCurrentAgent ? "text-theme-accent animate-pulse" : "text-theme-muted opacity-60")} />
                      </div>
                      <span className={cn(isCurrentAgent ? "font-black text-theme-primary tracking-tight" : "text-theme-secondary font-bold", "font-mono truncate tracking-tighter")}>
                        {p.b_process_name}
                      </span>
                    </td>
                  </tr>
                );
              })
            )}
            {filteredProcesses.length === 0 && (
              <tr>
                <td colSpan={agent.a_os === 1 ? 6 : 5} className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Activity size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black uppercase tracking-[0.2em] text-theme-muted">No telemetry nodes identified</p>
                  </div>
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      {/* 3. Footer Status */}
      <div className="px-3 py-1.5 bg-theme-glass border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black uppercase text-theme-muted tracking-[0.15em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 border border-theme-glass-light rounded-lg shadow-glow-sm">
            <span className="text-theme-muted opacity-60">PROCESS_COUNT:</span>
            <span className="text-theme-accent font-mono">{processes.length}</span>
          </div>
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 border border-theme-glass-light rounded-lg shadow-glow-sm">
            <span className="text-theme-muted opacity-60">SELF_PID:</span>
            <span className="text-theme-accent-secondary font-mono font-black">{agent.a_pid}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3">
          <span className="text-theme-muted">LIVE_TELEMETRY_STREAM</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
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
