import React, { useState, useEffect } from 'react';
import { listenerApi } from '../../api/control';
import { Play, Square, Edit, Trash2, RefreshCw } from 'lucide-react';
import { cn } from '../../utils/cn';

const ListenersList = () => {
  const [listeners, setListeners] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  const fetchListeners = async () => {
    try {
      setLoading(true);
      const response = await listenerApi.list();
      // The server returns an array directly
      setListeners(Array.isArray(response.data) ? response.data : []);
      setError(null);
    } catch (err) {
      console.error('Failed to fetch listeners:', err);
      setError('Failed to load listeners');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchListeners();
    const interval = setInterval(fetchListeners, 15000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="flex flex-col h-full bg-dark-900/50">
      <div className="flex items-center justify-between px-4 py-2 border-b border-dark-700 bg-dark-800/20">
        <div className="flex items-center space-x-4">
          <button 
            onClick={fetchListeners}
            className="p-1 hover:bg-dark-700 rounded text-gray-400 hover:text-accent-primary transition-all"
            title="Refresh List"
          >
            <RefreshCw className={loading ? "w-3.5 h-3.5 animate-spin" : "w-3.5 h-3.5"} />
          </button>
          <div className="h-4 w-px bg-dark-700" />
          <span className="text-[10px] font-bold text-gray-500 uppercase tracking-widest">Active Listeners</span>
        </div>
      </div>

      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-fixed">
          <thead className="sticky top-0 bg-dark-800 z-10">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] uppercase font-black tracking-tight">
              <th className="py-2 px-4 w-40">Name</th>
              <th className="py-2 px-4 w-32">Type</th>
              <th className="py-2 px-4 w-40">Bind Interface</th>
              <th className="py-2 px-4 w-24">Port</th>
              <th className="py-2 px-4">Status</th>
              <th className="py-2 px-4 w-32 text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px]">
            {loading && listeners.length === 0 ? (
              <tr><td colSpan="6" className="py-12 text-center text-gray-600 animate-pulse font-bold tracking-widest text-[9px] uppercase">Retrieving Listeners...</td></tr>
            ) : listeners.length === 0 ? (
              <tr><td colSpan="6" className="py-12 text-center text-gray-600 italic">No listeners configured.</td></tr>
            ) : (
              listeners.map((listener, idx) => (
                <tr key={idx} className="border-b border-dark-800 hover:bg-dark-700/30 transition-colors h-8">
                  <td className="px-4 font-bold text-accent-primary truncate">{listener.l_name}</td>
                  <td className="px-4 text-gray-400 font-mono text-[10px]">{listener.l_reg_name}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{listener.l_bind_host}</td>
                  <td className="px-4 text-gray-300 font-mono">{listener.l_bind_port}</td>
                  <td className="px-4">
                    <span className="flex items-center space-x-2">
                      <span className={cn(
                        "w-1.5 h-1.5 rounded-full",
                        listener.l_status === 'Listen' ? "bg-accent-secondary" : "bg-accent-danger"
                      )} />
                      <span className={cn(
                        "font-black uppercase text-[9px] tracking-tighter",
                        listener.l_status === 'Listen' ? "text-accent-secondary" : "text-accent-danger"
                      )}>{listener.l_status}</span>
                    </span>
                  </td>
                  <td className="px-4 text-right space-x-2">
                    <button className="text-gray-500 hover:text-accent-warning transition-colors"><Edit className="w-3.5 h-3.5" /></button>
                    <button className="text-gray-500 hover:text-accent-danger transition-colors"><Square className="w-3.5 h-3.5" /></button>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default ListenersList;
