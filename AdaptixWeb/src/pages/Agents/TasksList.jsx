import React, { useState, useEffect, useRef, useMemo } from 'react';
import { taskApi } from '../../api/control';
import { useAgents } from '../../context/AgentContext';
import { 
  RefreshCw, 
  Trash2, 
  StopCircle, 
  CheckCircle2, 
  Clock, 
  AlertCircle,
  Terminal,
  Search,
  X,
  ChevronDown,
  ChevronUp,
  Layout,
  Copy,
  ExternalLink,
  Filter
} from 'lucide-react';
import { cn } from '../../utils/cn';
import ContextMenu from '../../components/ContextMenu';

const TasksList = () => {
  const { agents, activeTabId, openAgentTab, tasks: contextTasks, fetchAgents, globalSearchQuery } = useAgents();
  const [selectedTask, setSelectedTask] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterAgent, setFilterAgent] = useState('All agents');
  const [filterStatus, setFilterStatus] = useState('Any status');
  const [outputHeight, setOutputHeight] = useState(200);
  const [menu, setMenu] = useState(null);
  const [loading, setLoading] = useState(false);
  const isResizing = useRef(false);

  // Convert tasks object to array for list view and sort by start time
  const tasksArray = useMemo(() => {
    return Object.values(contextTasks).sort((a, b) => (b.a_start_time || 0) - (a.a_start_time || 0));
  }, [contextTasks]);

  useEffect(() => {
    const handleMouseMove = (e) => {
      if (!isResizing.current) return;
      const newHeight = window.innerHeight - e.clientY - 40; // Adjust based on Dock header
      if (newHeight > 100 && newHeight < 500) {
        setOutputHeight(newHeight);
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

  const agentIds = ['All agents', ...new Set(agents.map(a => a.a_id))];

  const handleContextMenu = (e, task) => {
    e.preventDefault();
    setSelectedTask(task);
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Copy taskID', icon: Copy, onClick: () => navigator.clipboard.writeText(task.a_task_id) },
        { label: 'Copy commandLine', icon: Copy, onClick: () => navigator.clipboard.writeText(task.a_cmdline) },
        { divider: true },
        { label: 'Agent console', icon: Terminal, onClick: () => {
          const agent = agents.find(a => a.a_id === task.a_id);
          if (agent) openAgentTab(agent, 'console');
        }},
        { divider: true },
        { label: 'Cancel', icon: StopCircle, disabled: task.a_completed, onClick: () => {
          if (window.confirm('Cancel this task?')) taskApi.cancel(task.a_id, [task.a_task_id]);
        }},
        { label: 'Delete task', icon: Trash2, color: 'text-theme-danger', onClick: () => {
          if (window.confirm('Delete this task record?')) {
            taskApi.delete(task.a_id, [task.a_task_id]);
          }
        }},
      ]
    });
  };

  const filteredTasks = tasksArray.filter(t => {
    // Apply agent filter
    if (filterAgent !== 'All agents' && t.a_id !== filterAgent) return false;
    
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    const matchesSearch = query === '' || t.a_cmdline.toLowerCase().includes(query);
    const matchesStatus = filterStatus === 'Any status' || 
      (filterStatus === 'Completed' && t.a_completed) ||
      (filterStatus === 'In Progress' && !t.a_completed);
    return matchesSearch && matchesStatus;
  });

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header Controls */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center glass-input rounded-lg px-3 py-1.5">
            <span className="text-[10px] font-semibold text-theme-muted uppercase mr-2">Agent:</span>
            <select 
              value={filterAgent}
              onChange={(e) => setFilterAgent(e.target.value)}
              className="bg-transparent text-sm font-medium text-theme-accent outline-none cursor-pointer"
            >
              {agentIds.map(id => (
                <option key={id} value={id} className="bg-theme-glass-panel text-theme-primary">{id === 'All agents' ? 'All Agents' : id.substring(0,8)}</option>
              ))}
            </select>
          </div>

          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-xl transition-all",
              isSearchVisible ? "bg-theme-accent/20 text-theme-accent border border-theme-accent/30" : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-4 h-4" />
          </button>
        </div>

        <div className="flex items-center space-x-2">
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh All Tasks"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light space-x-3 shrink-0">
          <div className="relative flex-1">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search command line history..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
          <div className="flex items-center glass-input rounded-lg px-3 py-1.5 shrink-0">
            <span className="text-[10px] font-semibold text-theme-muted uppercase mr-2">Status:</span>
            <select 
              value={filterStatus}
              onChange={(e) => setFilterStatus(e.target.value)}
              className="bg-transparent text-sm font-medium text-theme-primary outline-none cursor-pointer"
            >
              <option className="bg-theme-glass-panel">Any status</option>
              <option className="bg-theme-glass-panel">In Progress</option>
              <option className="bg-theme-glass-panel">Completed</option>
            </select>
          </div>
        </div>
      )}

      {/* 3. Main Tasks Table */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[800px]">
          <thead>
            <tr>
              <th className="w-24">Type</th>
              <th>Command String</th>
              <th className="w-32">Execution</th>
              <th className="w-40 text-right">Timestamp</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium">
            {filteredTasks.length === 0 ? (
              <tr>
                <td colSpan="4" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-40">
                    <Layout size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">No telemetry records matching criteria</p>
                  </div>
                </td>
              </tr>
            ) : (
              [...filteredTasks].reverse().map((task, idx) => (
                <tr 
                  key={task.a_task_id || idx} 
                  onClick={() => setSelectedTask(task)}
                  onContextMenu={(e) => handleContextMenu(e, task)}
                  className={cn(
                    "group cursor-pointer",
                    selectedTask?.a_task_id === task.a_task_id && "bg-theme-hover"
                  )}
                >
                  <td className="w-24">
                    <span className={cn(
                      "px-2 py-0.5 rounded-full text-[9px] font-black uppercase tracking-wider",
                      task.Status === 'Completed' ? "bg-theme-success/10 text-theme-success border border-theme-success/20" : "bg-theme-accent/10 text-theme-accent border border-theme-accent/20"
                    )}>
                      {task.a_task_type || 'CMD'}
                    </span>
                  </td>
                  <td className="px-3 py-2 text-theme-primary font-mono">{task.a_cmdline}</td>
                  <td className="px-3 py-2">
                    <div className="flex items-center space-x-2">
                      <div className={cn(
                        "w-1.5 h-1.5 rounded-full shadow-[0_0_4px]",
                        task.a_completed ? (task.a_msg_type === 2 || task.a_msg_type === 4 ? "bg-theme-danger shadow-theme-danger/50" : "bg-theme-success shadow-theme-success/50") : 
                        "bg-theme-accent shadow-theme-accent/50 animate-pulse"
                      )} />
                      <span className={cn(
                        "text-[10px] font-black uppercase tracking-tighter",
                        task.a_completed ? (task.a_msg_type === 2 || task.a_msg_type === 4 ? "text-theme-danger" : "text-theme-accent-secondary") : 
                        "text-theme-accent"
                      )}>
                        {task.a_completed ? (task.a_msg_type === 2 || task.a_msg_type === 4 ? 'Error' : 'Success') : 'Running'}
                      </span>
                    </div>
                  </td>
                  <td className="px-3 py-2 text-right text-theme-muted font-mono text-[10px]">
                    {new Date((task.a_finish_time || task.a_start_time) * 1000).toLocaleTimeString([], { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })}
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* 4. Resizer & Task Output */}
      {selectedTask && (
        <div className="shrink-0 flex flex-col">
          <div 
            className="h-1 bg-theme-glass-light hover:bg-theme-accent/50 cursor-row-resize transition-colors z-20 flex items-center justify-center group"
            onMouseDown={() => {
              isResizing.current = true;
              document.body.style.cursor = 'row-resize';
            }}
          >
            <div className="w-10 h-0.5 bg-theme-glass rounded-full group-hover:bg-theme-accent transition-colors shadow-sm" />
          </div>
          <div 
            style={{ height: `${outputHeight}px` }}
            className="bg-theme-glass-panel border-t border-theme-glass-light flex flex-col overflow-hidden relative shadow-glow"
          >
            <div className="flex items-center justify-between px-4 py-2 bg-theme-glass border-b border-theme-glass-light shrink-0">
              <div className="flex items-center space-x-3">
                <Terminal className="w-4 h-4 text-theme-accent" />
                <div className="flex flex-col text-left">
                  <span className="text-[10px] font-black text-theme-primary uppercase tracking-widest leading-none">Task Telemetry Output</span>
                  <span className="text-[9px] font-mono text-theme-muted truncate max-w-lg mt-1">{selectedTask.a_task_id}</span>
                </div>
              </div>
              <div className="flex items-center space-x-2">
                <button 
                  onClick={() => navigator.clipboard.writeText(selectedTask.a_output || '')}
                  className="p-1.5 hover:bg-theme-glass-panel rounded-lg text-theme-muted hover:text-theme-primary transition-all shadow-sm"
                  title="Copy Output"
                >
                  <Copy size={16} />
                </button>
                <div className="w-px h-5 bg-theme-glass-light mx-1" />
                <button onClick={() => setSelectedTask(null)} className="p-1.5 hover:bg-theme-glass-panel text-theme-muted hover:text-theme-primary transition-all rounded-lg">
                  <X size={18} />
                </button>
              </div>
            </div>
            <div className="flex-1 p-6 font-mono text-[12px] text-theme-secondary overflow-auto select-text bg-theme-glass-panel/30 custom-scrollbar leading-relaxed">
              {selectedTask.a_output ? (
                <pre className="whitespace-pre-wrap break-words">{selectedTask.a_output}</pre>
              ) : (
                <div className="flex flex-col items-center justify-center h-full text-theme-muted space-y-3 uppercase font-black text-[10px] tracking-[0.2em] opacity-40">
                  <AlertCircle size={48} className="text-theme-accent" />
                  <span>Stream awaiting response...</span>
                </div>
              )}
            </div>
          </div>
        </div>
      )}
      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default TasksList;
