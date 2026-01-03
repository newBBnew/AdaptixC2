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
  const { agents, activeTabId, openAgentTab, tasks: contextTasks, fetchAgents } = useAgents();
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
        { label: 'Delete task', icon: Trash2, color: 'text-accent-danger', onClick: () => {
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
    
    const matchesSearch = searchQuery === '' || t.a_cmdline.toLowerCase().includes(searchQuery.toLowerCase());
    const matchesStatus = filterStatus === 'Any status' || 
      (filterStatus === 'Completed' && t.a_completed) ||
      (filterStatus === 'In Progress' && !t.a_completed);
    return matchesSearch && matchesStatus;
  });

  return (
    <div className="flex flex-col h-full w-full bg-dark-900 select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header Controls */}
      <div className="flex items-center justify-between px-2 py-1 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-2">
          <div className="flex items-center bg-dark-950 border border-dark-700 rounded-sm px-1.5 py-0.5">
            <span className="text-[9px] font-black text-gray-600 uppercase mr-2">Agent:</span>
            <select 
              value={filterAgent}
              onChange={(e) => setFilterAgent(e.target.value)}
              className="bg-transparent text-[10px] font-bold text-accent-primary outline-none cursor-pointer"
            >
              {agentIds.map(id => (
                <option key={id} value={id} className="bg-dark-800 text-[#BEBEBE]">{id === 'All agents' ? 'GLOBAL_VIEW' : id.substring(0,8)}</option>
              ))}
            </select>
          </div>

          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-1 rounded transition-colors",
              isSearchVisible ? "bg-accent-selection/30 text-accent-primary" : "text-gray-500 hover:text-gray-300"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-3.5 h-3.5" />
          </button>
        </div>

        <div className="flex items-center space-x-2">
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-1 rounded hover:bg-dark-700 text-gray-500 hover:text-white transition-all"
            title="Refresh All Tasks"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-1.5 bg-dark-800 border-b border-dark-700 space-x-3 animate-in slide-in-from-top-2 duration-200 shrink-0">
          <div className="relative flex-1">
            <Filter className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search command line history..." 
              className="qt-input w-full pl-8 py-1"
            />
          </div>
          <div className="flex items-center bg-dark-950 border border-dark-700 rounded-sm px-2 py-1 shrink-0">
            <span className="text-[9px] font-black text-gray-600 uppercase mr-2">Status:</span>
            <select 
              value={filterStatus}
              onChange={(e) => setFilterStatus(e.target.value)}
              className="bg-transparent text-[10px] font-bold text-[#BEBEBE] outline-none cursor-pointer"
            >
              <option className="bg-dark-800">Any status</option>
              <option className="bg-dark-800">In Progress</option>
              <option className="bg-dark-800">Completed</option>
            </select>
          </div>
        </div>
      )}

      {/* 3. Main Tasks Table */}
      <div className="flex-1 overflow-auto custom-scrollbar bg-dark-950/10">
        <table className="qt-table min-w-[800px]">
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
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Layout size={48} className="text-gray-600" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-gray-500">No telemetry records matching criteria</p>
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
                    "transition-colors group h-8 cursor-default",
                    selectedTask?.a_task_id === task.a_task_id && "bg-accent-selection/20 border-l-2 border-accent-primary"
                  )}
                >
                  <td className="w-24 px-3 py-2">
                    <span className="px-1.5 py-0.5 rounded-sm bg-dark-800 border border-dark-700 font-mono text-gray-500 uppercase text-[9px]">
                      {task.a_task_type || 'CMD'}
                    </span>
                  </td>
                  <td className="px-3 py-2 text-gray-300 font-mono tracking-tight truncate max-w-xl">
                    <span className="text-gray-600 mr-2 opacity-50">$</span>
                    {task.a_cmdline}
                  </td>
                  <td className="px-3 py-2">
                    <div className="flex items-center space-x-2">
                      <div className={cn(
                        "w-1.5 h-1.5 rounded-full shadow-[0_0_4px]",
                        task.Status === 'Success' || task.a_completed ? "bg-accent-secondary shadow-accent-secondary/50" : 
                        task.Status === 'Error' ? "bg-accent-danger shadow-accent-danger/50" : 
                        "bg-accent-warning shadow-accent-warning/50 animate-pulse"
                      )} />
                      <span className={cn(
                        "text-[10px] font-black uppercase tracking-tighter",
                        task.Status === 'Success' || task.a_completed ? "text-accent-secondary" : 
                        task.Status === 'Error' ? "text-accent-danger" : 
                        "text-accent-warning"
                      )}>
                        {task.Status || (task.a_completed ? 'Finished' : 'Running')}
                      </span>
                    </div>
                  </td>
                  <td className="px-3 py-2 text-right text-gray-600 font-mono text-[10px]">
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
            className="h-1 bg-dark-700 hover:bg-accent-primary/50 cursor-row-resize transition-colors z-20 flex items-center justify-center group"
            onMouseDown={() => {
              isResizing.current = true;
              document.body.style.cursor = 'row-resize';
            }}
          >
            <div className="w-8 h-0.5 bg-dark-600 rounded-full group-hover:bg-accent-primary transition-colors" />
          </div>
          <div 
            style={{ height: `${outputHeight}px` }}
            className="bg-dark-900 border-t border-dark-700 flex flex-col overflow-hidden relative shadow-[0_-10px_20px_rgba(0,0,0,0.3)]"
          >
            <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0">
              <div className="flex items-center space-x-3">
                <Terminal className="w-3.5 h-3.5 text-accent-primary" />
                <div className="flex flex-col">
                  <span className="text-[10px] font-black text-white uppercase tracking-widest leading-none">Task Telemetry Output</span>
                  <span className="text-[9px] font-mono text-gray-500 truncate max-w-lg mt-0.5">{selectedTask.a_task_id}</span>
                </div>
              </div>
              <div className="flex items-center space-x-2">
                <button 
                  onClick={() => navigator.clipboard.writeText(selectedTask.a_output || '')}
                  className="p-1 hover:bg-dark-700 rounded text-gray-500 hover:text-white transition-colors"
                  title="Copy Output"
                >
                  <Copy size={14} />
                </button>
                <div className="w-px h-3.5 bg-dark-700 mx-1" />
                <button onClick={() => setSelectedTask(null)} className="text-gray-500 hover:text-white transition-colors">
                  <X size={16} />
                </button>
              </div>
            </div>
            <div className="flex-1 p-4 font-mono text-[12px] text-[#D0D0D0] overflow-auto select-text bg-dark-950/50 custom-scrollbar leading-relaxed">
              {selectedTask.a_output ? (
                <pre className="whitespace-pre-wrap break-words">{selectedTask.a_output}</pre>
              ) : (
                <div className="flex flex-col items-center justify-center h-full text-gray-700 space-y-2 uppercase font-black text-[10px] tracking-[0.2em]">
                  <AlertCircle size={32} className="opacity-20" />
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
