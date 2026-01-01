import React, { useState, useEffect } from 'react';
import { 
  Shield, 
  Search, 
  Filter, 
  StopCircle, 
  Edit3, 
  RefreshCw,
  X,
  Globe,
  ArrowRight
} from 'lucide-react';
import { tunnelApi } from '../../api/control';
import { cn } from '../../utils/cn';

const TunnelsList = () => {
  const [tunnels, setTunnels] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);

  const fetchTunnels = async () => {
    try {
      setLoading(true);
      const response = await tunnelApi.list();
      setTunnels(Array.isArray(response.data) ? response.data : []);
      setError(null);
    } catch (err) {
      console.error('Failed to fetch tunnels:', err);
      setError('Connection failed');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchTunnels();
    const interval = setInterval(fetchTunnels, 30000);
    return () => clearInterval(interval);
  }, []);

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

  const filteredTunnels = tunnels.filter(t => 
    Object.values(t).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    )
  );

  const handleStopTunnel = async (tunnelId) => {
    if (!window.confirm('Are you sure you want to stop this tunnel?')) return;
    try {
      await tunnelApi.stop(tunnelId);
      fetchTunnels();
    } catch (err) {
      console.error('Failed to stop tunnel:', err);
    }
  };

  return (
    <div className="flex flex-col h-full bg-dark-900 text-gray-300 font-sans select-none overflow-hidden">
      {/* 1. Header with Controls (Mimics TunnelsWidget.cpp) */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <Shield className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Tunnel Manager</span>
          </div>
          <div className="h-4 w-px bg-dark-600" />
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-1 rounded hover:bg-dark-700 transition-colors",
              isSearchVisible ? "bg-accent-primary/20 text-accent-primary" : "text-gray-500"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-3.5 h-3.5" />
          </button>
        </div>

        <div className="flex items-center space-x-1">
          <button 
            onClick={fetchTunnels}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 animate-in slide-in-from-top-2 duration-200 shrink-0">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="filter: socks | forward..." 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50 placeholder:text-gray-700"
            />
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-auto min-w-[800px]">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 border-r border-dark-700/30">Agent ID</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Interface</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Port</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Forward Host</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Forward Port</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Info</th>
              <th className="py-2 px-4 text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {filteredTunnels.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-20 text-center text-gray-600 italic">
                  <div className="flex flex-col items-center space-y-3 opacity-20">
                    <Shield size={40} />
                    <p className="text-xs font-medium tracking-widest uppercase">No active tunnels</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredTunnels.map((t) => (
                <tr 
                  key={t.tunnel_id} 
                  className="hover:bg-accent-primary/5 transition-colors group h-8 cursor-default"
                >
                  <td className="px-4 text-gray-400 font-mono truncate">{t.agent_id?.substring(0,8) || '---'}</td>
                  <td className="px-4 truncate">
                    <div className="flex items-center space-x-2">
                      <Globe className="w-3 h-3 text-gray-500" />
                      <span className="font-bold text-gray-300">{t.interface || '0.0.0.0'}</span>
                    </div>
                  </td>
                  <td className="px-4 text-accent-primary font-bold font-mono truncate">{t.port}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{t.fhost || '---'}</td>
                  <td className="px-4 text-gray-300 font-mono truncate">{t.fport || '---'}</td>
                  <td className="px-4 text-gray-400 italic truncate max-w-xs">{t.info || 'No description'}</td>
                  <td className="px-4 text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button 
                        onClick={() => handleStopTunnel(t.tunnel_id)}
                        className="p-1 rounded hover:bg-dark-700 text-accent-danger transition-colors" 
                        title="Stop Tunnel"
                      >
                        <StopCircle size={14} />
                      </button>
                    </div>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
      
      {/* 4. Footer Summary */}
      <div className="px-4 py-1.5 bg-dark-800 border-t border-dark-700 flex items-center justify-between text-[10px] font-bold text-gray-500 uppercase tracking-tighter shrink-0">
        <div className="flex items-center space-x-4">
          <span>Active Tunnels: <span className="text-accent-primary">{tunnels.length}</span></span>
        </div>
        <div className="flex items-center space-x-1">
          <div className="w-1.5 h-1.5 rounded-full bg-accent-secondary animate-pulse" />
          <span className="text-accent-secondary/80">Real-time Tunnel Sync</span>
        </div>
      </div>
    </div>
  );
};

export default TunnelsList;
