import React, { useState, useEffect } from 'react';
import { taskApi } from '../../api/control';
import { useAgents } from '../../context/AgentContext';
import { RefreshCw, Trash2, StopCircle, CheckCircle2, Clock } from 'lucide-react';
import { clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';
import { cn } from '../../utils/cn';

const TasksList = () => {
  const { activeTabId } = useAgents();
  const [tasks, setTasks] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  const fetchTasks = async () => {
    if (!activeTabId) return;
    try {
      setLoading(true);
      const response = await taskApi.list(activeTabId);
      // Server returns []TaskData directly
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
  }, [activeTabId]);

  if (!activeTabId) {
    return (
      <div className="flex items-center justify-center h-full text-gray-600 italic text-xs">
        Select an active beacon session to view tasks.
      </div>
    );
  }

  return (
    <div className="flex flex-col h-full bg-dark-900/50">
      <div className="flex items-center justify-between px-4 py-2 border-b border-dark-700 bg-dark-800/20">
        <div className="flex items-center space-x-4">
          <button 
            onClick={fetchTasks}
            className="p-1 hover:bg-dark-700 rounded text-gray-400 hover:text-accent-primary transition-all"
          >
            <RefreshCw className={loading ? "w-3.5 h-3.5 animate-spin" : "w-3.5 h-3.5"} />
          </button>
          <div className="h-4 w-px bg-dark-700" />
          <span className="text-[10px] font-bold text-gray-500 uppercase tracking-widest">Tasks for Session: {activeTabId.substring(0,8)}</span>
        </div>
      </div>

      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-fixed">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] uppercase font-black tracking-tight">
              <th className="py-2 px-4 w-24">Type</th>
              <th className="py-2 px-4">Command Line</th>
              <th className="py-2 px-4 w-32">Status</th>
              <th className="py-2 px-4 w-40 text-right">Started At</th>
            </tr>
          </thead>
          <tbody className="text-[11px]">
            {loading && tasks.length === 0 ? (
              <tr><td colSpan="4" className="py-12 text-center text-gray-600 animate-pulse font-bold tracking-widest text-[9px] uppercase">Retrieving Task Log...</td></tr>
            ) : tasks.length === 0 ? (
              <tr><td colSpan="4" className="py-12 text-center text-gray-600 italic">No tasks recorded for this session.</td></tr>
            ) : (
              tasks.map((task, idx) => (
                <tr key={idx} className="border-b border-dark-800 hover:bg-dark-700/30 transition-colors group h-8">
                  <td className="px-4 font-mono text-gray-500 text-[10px]">{task.a_task_type}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{task.a_cmdline}</td>
                  <td className="px-4">
                    <span className={cn(
                      "flex items-center space-x-2 font-black uppercase text-[9px] tracking-tighter",
                      task.a_completed ? "text-accent-secondary" : "text-accent-warning"
                    )}>
                      {task.a_completed ? <CheckCircle2 className="w-3 h-3" /> : <Clock className="w-3 h-3 animate-pulse" />}
                      <span>{task.a_completed ? 'Completed' : 'In Progress'}</span>
                    </span>
                  </td>
                  <td className="px-4 text-right text-gray-500 font-mono">{new Date(task.a_start_time * 1000).toLocaleString()}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default TasksList;
