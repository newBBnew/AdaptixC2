import React, { useState, useEffect } from 'react';
import { 
  Download, 
  Search, 
  Filter, 
  Trash2, 
  RefreshCw,
  X,
  FileDown,
  Terminal,
  ExternalLink,
  CheckCircle2,
  Clock,
  AlertCircle
} from 'lucide-react';
import { dataApi } from '../../api/control';
import { cn } from '../../utils/cn';

const DownloadsList = () => {
  const [downloads, setDownloads] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterState, setFilterState] = useState('Any state');

  const fetchDownloads = async () => {
    try {
      setLoading(true);
      const response = await dataApi.downloads();
      setDownloads(Array.isArray(response.data) ? response.data : []);
      setError(null);
    } catch (err) {
      console.error('Failed to fetch downloads:', err);
      setError('Connection failed');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchDownloads();
    const interval = setInterval(fetchDownloads, 30000);
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

  const filteredDownloads = downloads.filter(d => {
    const matchesSearch = Object.values(d).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    );
    // Note: Qt client uses state index, we assume textual state mapping for now
    const matchesState = filterState === 'Any state' || d.state === filterState.toLowerCase();
    return matchesSearch && matchesState;
  });

  const getStatusIcon = (state) => {
    switch (state?.toLowerCase()) {
      case 'finished': return <CheckCircle2 size={14} className="text-accent-secondary" />;
      case 'running': return <RefreshCw size={14} className="text-accent-primary animate-spin" />;
      case 'stopped': return <Clock size={14} className="text-gray-500" />;
      default: return <AlertCircle size={14} className="text-accent-danger" />;
    }
  };

  return (
    <div className="flex flex-col h-full bg-dark-900 text-gray-300 font-sans select-none overflow-hidden">
      {/* 1. Header Controls (Mimics DownloadsWidget.cpp) */}
      <div className="flex items-center justify-between px-3 py-1.5 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 px-2 py-0.5 rounded bg-accent-primary/10 border border-accent-primary/20">
            <Download className="w-3.5 h-3.5 text-accent-primary" />
            <span className="text-[10px] font-black uppercase tracking-widest text-accent-primary">Downloads</span>
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
            onClick={fetchDownloads}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 bg-dark-800/50 border-b border-dark-700 space-x-4 animate-in slide-in-from-top-2 duration-200 shrink-0">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="filter: (exe | dll) & ^(temp)" 
              className="w-full bg-dark-950/50 border border-dark-600 rounded px-8 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
            />
          </div>
          <select 
            value={filterState}
            onChange={(e) => setFilterState(e.target.value)}
            className="bg-dark-950/50 border border-dark-600 rounded px-2 py-1 text-[11px] text-gray-300 outline-none focus:border-accent-primary/50"
          >
            <option>Any state</option>
            <option>Running</option>
            <option>Stopped</option>
            <option>Finished</option>
          </select>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto scrollbar-thin">
        <table className="w-full text-left border-collapse table-auto min-w-[800px]">
          <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
            <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
              <th className="py-2 px-4 border-r border-dark-700/30">File</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Agent</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Progress</th>
              <th className="py-2 px-4 border-r border-dark-700/30">Date</th>
              <th className="py-2 px-4 text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
            {filteredDownloads.length === 0 ? (
              <tr>
                <td colSpan="5" className="py-20 text-center text-gray-600 italic">
                  <div className="flex flex-col items-center space-y-3 opacity-20">
                    <Download size={40} />
                    <p className="text-xs font-medium tracking-widest uppercase">No downloads recorded</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredDownloads.map((d) => {
                const progress = d.total_size > 0 ? (d.recv_size / d.total_size) * 100 : 0;
                return (
                  <tr 
                    key={d.file_id} 
                    className="hover:bg-accent-primary/5 transition-colors group h-8 cursor-default"
                  >
                    <td className="px-4 text-accent-primary font-bold font-mono truncate max-w-xs" title={d.filename}>
                      {d.filename}
                    </td>
                    <td className="px-4 text-gray-400 font-mono truncate">{d.agent_id?.substring(0,8)}</td>
                    <td className="px-4 min-w-[150px]">
                      <div className="flex items-center space-x-3">
                        <div className="flex-1 h-1.5 bg-dark-700 rounded-full overflow-hidden">
                          <motion.div 
                            initial={{ width: 0 }}
                            animate={{ width: `${progress}%` }}
                            className={cn(
                              "h-full transition-all duration-500",
                              d.state === 'finished' ? "bg-accent-secondary" : "bg-accent-primary"
                            )}
                          />
                        </div>
                        <span className="text-[9px] font-bold text-gray-500 w-8">{Math.round(progress)}%</span>
                        {getStatusIcon(d.state)}
                      </div>
                    </td>
                    <td className="px-4 text-gray-500 font-mono truncate">
                      {new Date(d.date * 1000).toLocaleString()}
                    </td>
                    <td className="px-4 text-right">
                      <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                        <button className="p-1 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-colors" title="Sync to Client">
                          <FileDown size={14} />
                        </button>
                        <button className="p-1 rounded hover:bg-dark-700 text-accent-danger transition-colors" title="Delete">
                          <Trash2 size={14} />
                        </button>
                      </div>
                    </td>
                  </tr>
                );
              })
            )}
          </tbody>
        </table>
      </div>
      
      {/* 4. Footer Summary */}
      <div className="px-4 py-1.5 bg-dark-800 border-t border-dark-700 flex items-center justify-between text-[10px] font-bold text-gray-500 uppercase tracking-tighter shrink-0">
        <div className="flex items-center space-x-4">
          <span>Total Downloads: <span className="text-gray-300">{downloads.length}</span></span>
        </div>
        <div className="flex items-center space-x-1">
          <div className="w-1.5 h-1.5 rounded-full bg-accent-secondary animate-pulse" />
          <span className="text-accent-secondary/80">Teamserver Synchronized</span>
        </div>
      </div>
    </div>
  );
};

export default DownloadsList;
