import React, { useState, useEffect, useRef } from 'react';
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
  Layout
} from 'lucide-react';
import { cn } from '../../utils/cn';

const TasksList = () => {
  const { agents, activeTabId } = useAgents();
  const [tasks, setTasks] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [selectedTask, setSelectedTask] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterAgent, setFilterAgent] = useState('All agents');
  const [filterStatus, setFilterStatus] = useState('Any status');
  const [outputHeight, setOutputHeight] = useState(200);
  const isResizing = useRef(false);

  const fetchTasks = async () => {
    try {
      setLoading(true);
      // If filterAgent is specific, use that, otherwise if activeTabId exists, use that, else all
      const targetId = filterAgent !== 'All agents' ? filterAgent : (activeTabId || '');
      const response = await taskApi.list(targetId);
      setTasks(Array.isArray(response.data) ? response.data : []);
      setError(null);
    } catch (err) {
      console.error('Failed to fetch tasks:', err);
      setError('Failed to load tasks');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchTasks();
    const interval = setInterval(fetchTasks, 10000);
    return () => clearInterval(interval);
  }, [activeTabId, filterAgent]);

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

  const filteredTasks = tasks.filter(t => {
    const matchesSearch = searchQuery === '' || t.a_cmdline.toLowerCase().includes(searchQuery.toLowerCase());
    const matchesStatus = filterStatus === 'Any status' || 
      (filterStatus === 'Completed' && t.a_completed) ||
      (filterStatus === 'In Progress' && !t.a_completed);
    return matchesSearch && matchesStatus;
  });

  return (
    <div className="flex flex-col h-full bg-dark-900 select-none overflow-hidden font-sans">
      {/* 1. Header Controls (Mimics TasksWidget.cpp) */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <Layout className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Task Manager</span>
          </div>
          <div className="h-4 w-px bg-dark-600" />
          
          <select 
            value={filterAgent}
            onChange={(e) => setFilterAgent(e.target.value)}
            className="bg-dark-950/50 border border-dark-600 rounded px-2 py-0.5 text-[10px] text-gray-300 outline-none focus:border-accent-primary/50"
          >
            {agentIds.map(id => (
              <option key={id} value={id}>{id === 'All agents' ? id : id.substring(0,8)}</option>
            ))}
          </select>

          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-1 rounded transition-colors",
              isSearchVisible ? "bg-accent-primary/20 text-accent-primary" : "text-gray-500 hover:text-gray-300"
            )}
          >
            <Search className="w-3.5 h-3.5" />
          </button>
        </div>

        <div className="flex items-center space-x-2">
          <button 
            onClick={fetchTasks}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 space-x-4 animate-in slide-in-from-top-2 duration-200 shrink-0">
          <input 
            type="text" 
            autoFocus
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="filter: command line..." 
            className="flex-1 bg-dark-950/50 border border-dark-600 rounded px-3 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
          />
          <select 
            value={filterStatus}
            onChange={(e) => setFilterStatus(e.target.value)}
            className="bg-dark-950/50 border border-dark-600 rounded px-2 py-1 text-[11px] text-gray-300 outline-none"
          >
            <option>Any status</option>
            <option>In Progress</option>
            <option>Completed</option>
          </select>
        </div>
      )}

      {/* 3. Main Tasks Table */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-auto min-w-[600px]">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 w-24 border-r border-dark-700/30">Type</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Command Line</th>
              <th className="py-2 px-4 w-32 border-r border-dark-700/30">Status</th>
              <th className="py-2 px-4 w-40 text-right">Started At</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {filteredTasks.length === 0 ? (
              <tr><td colSpan="4" className="py-12 text-center text-gray-600 italic">No tasks matching criteria</td></tr>
            ) : (
              filteredTasks.map((task, idx) => (
                <tr 
                  key={idx} 
                  onClick={() => setSelectedTask(task)}
                  className={cn(
                    "hover:bg-accent-primary/5 transition-colors group h-8 cursor-pointer",
                    selectedTask === task && "bg-accent-primary/10 border-l-2 border-l-accent-primary"
                  )}
                >
                  <td className="px-4 font-mono text-gray-500 uppercase text-[10px]">{task.a_task_type}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{task.a_cmdline}</td>
                  <td className="px-4">
                    <div className="flex items-center space-x-2">
                      {task.a_completed ? (
                        <CheckCircle2 className="w-3 h-3 text-accent-secondary" />
                      ) : (
                        <Clock className="w-3 h-3 text-accent-warning animate-pulse" />
                      )}
                      <span className={cn(
                        "text-[10px] font-black uppercase tracking-tighter",
                        task.a_completed ? "text-accent-secondary" : "text-accent-warning"
                      )}>
                        {task.a_completed ? 'Completed' : 'Pending'}
                      </span>
                    </div>
                  </td>
                  <td className="px-4 text-right text-gray-500 font-mono">
                    {new Date(task.a_start_time * 1000).toLocaleTimeString([], { hour12: false })}
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* 4. Resizer & Task Output (Mimics TaskOutputWidget) */}
      {selectedTask && (
        <>
          <div 
            className="h-1 bg-dark-700 hover:bg-accent-primary cursor-row-resize transition-colors z-20 shrink-0"
            onMouseDown={() => {
              isResizing.current = true;
              document.body.style.cursor = 'row-resize';
            }}
          />
          <div 
            style={{ height: `${outputHeight}px` }}
            className="bg-[#0a0a0a] border-t border-dark-700 flex flex-col overflow-hidden shrink-0"
          >
            <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700">
              <div className="flex items-center space-x-2">
                <Terminal className="w-3 h-3 text-gray-500" />
                <span className="text-[10px] font-bold text-gray-400 uppercase tracking-widest">Task Output</span>
                <div className="h-3 w-px bg-dark-600 mx-1" />
                <span className="text-[10px] font-mono text-accent-primary truncate max-w-md">{selectedTask.a_cmdline}</span>
              </div>
              <button onClick={() => setSelectedTask(null)} className="text-gray-500 hover:text-white transition-colors">
                <X size={14} />
              </button>
            </div>
            <div className="flex-1 p-4 font-mono text-[12px] text-gray-300 overflow-auto custom-scrollbar select-text whitespace-pre-wrap">
              {selectedTask.a_output || (
                <div className="flex flex-col items-center justify-center h-full opacity-20">
                  <AlertCircle size={32} />
                  <p className="mt-2 uppercase tracking-widest text-[10px]">No output available</p>
                </div>
              )}
            </div>
          </div>
        </>
      )}
    </div>
  );
};

export default TasksList;
