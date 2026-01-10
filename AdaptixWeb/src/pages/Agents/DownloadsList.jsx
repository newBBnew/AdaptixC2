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
  AlertCircle,
  Copy,
  TerminalSquare
} from 'lucide-react';
import { dataApi } from '../../api/control';
import { cn } from '../../utils/cn';
import ContextMenu from '../../components/ContextMenu';
import DownloadProgressDialog from './DownloadProgressDialog';
import { useAgents } from '../../context/AgentContext';

const DownloadsList = () => {
  const { downloads, fetchAgents, globalSearchQuery } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  const [filterState, setFilterState] = useState('Any state');
  const [menu, setMenu] = useState(null);

  const [isDownloadOpen, setIsDownloadOpen] = useState(false);
  const [downloadUrl, setDownloadUrl] = useState('');
  const [downloadFilename, setDownloadFilename] = useState('');

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
    const query = (searchQuery || globalSearchQuery).toLowerCase();
    const matchesSearch = Object.values(d).some(val => 
      String(val).toLowerCase().includes(query)
    );
    // Backend states: 1=Running, 2=Finished, 3=Stopped (based on common patterns)
    const matchesState = filterState === 'Any state' || 
      (filterState === 'Running' && d.d_state === 1) ||
      (filterState === 'Finished' && d.d_state === 2) ||
      (filterState === 'Stopped' && d.d_state === 3);
    return matchesSearch && matchesState;
  });

  const getStatusText = (state) => {
    switch (state) {
      case 1: return 'Running';
      case 2: return 'Finished';
      case 3: return 'Stopped';
      default: return 'Unknown';
    }
  };

  const handleSync = async (download) => {
    try {
      const response = await dataApi.getOTP('download', download.d_file_id);
      if (response.data?.ok) {
        const otp = response.data.message;
        const baseUrl = localStorage.getItem('adaptix_url') || window.location.origin;
        // Construct the full proxy URL for the Gateway
        const syncUrl = `${window.location.origin}/api/proxy/otp/download/sync?token=${otp}`;
        
        setDownloadUrl(syncUrl);
        setDownloadFilename(download.d_file.split(/[\\/]/).pop());
        setIsDownloadOpen(true);
      }
    } catch (err) {
      console.error('Sync failed:', err);
      alert('Failed to generate sync token');
    }
  };

  const handleCopyCommand = async (download, tool) => {
    try {
      const response = await dataApi.getOTP('download', download.d_file_id);
      if (response.data?.ok) {
        const otp = response.data.message;
        const baseUrl = localStorage.getItem('adaptix_url') || window.location.origin;
        const syncUrl = `${baseUrl}/endpoint/otp/download/sync`;
        const filename = download.d_file.split(/[\\/]/).pop();
        
        let cmd = '';
        if (tool === 'curl') {
          cmd = `curl -k ${syncUrl} -H 'OTP: ${otp}' -o ${filename}`;
        } else {
          cmd = `wget --no-check-certificate ${syncUrl} --header='OTP: ${otp}' -O ${filename}`;
        }
        
        navigator.clipboard.writeText(cmd);
        alert(`${tool.toUpperCase()} command copied to clipboard`);
      }
    } catch (err) {
      console.error('Command generation failed:', err);
    }
  };

  const handleContextMenu = (e, download) => {
    e.preventDefault();
    const isFinished = download.d_state === 2;
    
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Sync file to client', icon: FileDown, disabled: !isFinished, onClick: () => handleSync(download) },
        { 
          label: 'Sync as...', 
          icon: TerminalSquare,
          disabled: !isFinished,
          children: [
            { label: 'Curl command', onClick: () => handleCopyCommand(download, 'curl') },
            { label: 'Wget command', onClick: () => handleCopyCommand(download, 'wget') },
          ]
        },
        { divider: true },
        { label: 'Delete file', icon: Trash2, color: 'text-theme-danger', onClick: () => {
          if (window.confirm('Delete this download record?')) {
            // dataApi.removeDownload([download.d_file_id]);
          }
        }},
      ]
    });
  };

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header Controls */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
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
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh Transfers"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light space-x-3 shrink-0">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search filename, agent, status..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
          <div className="flex items-center glass-input rounded-lg px-3 py-1.5 shrink-0">
            <span className="text-[10px] font-semibold text-theme-muted uppercase mr-2">Status:</span>
            <select 
              value={filterState}
              onChange={(e) => setFilterState(e.target.value)}
              className="bg-transparent text-sm font-medium text-theme-primary outline-none cursor-pointer"
            >
              <option className="bg-theme-glass-panel">Any state</option>
              <option className="bg-theme-glass-panel">Running</option>
              <option className="bg-theme-glass-panel">Stopped</option>
              <option className="bg-theme-glass-panel">Finished</option>
            </select>
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[900px]">
          <thead>
            <tr>
              <th className="w-64">File Identifier</th>
              <th className="w-32">Source Node</th>
              <th className="w-64">Transfer Progress</th>
              <th className="w-48">Discovery Date</th>
              <th className="text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[12px] font-medium">
            {filteredDownloads.length === 0 ? (
              <tr>
                <td colSpan="5" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-40">
                    <Download size={48} className="text-theme-muted" />
                    <p className="text-sm font-medium tracking-wider text-theme-muted">No active data transfers</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredDownloads.map((d) => {
                const totalSize = d.d_size || 0;
                const recvSize = d.d_recv_size || 0;
                const progress = totalSize > 0 ? (recvSize / totalSize) * 100 : 0;
                const status = getStatusText(d.d_state);
                return (
                  <tr 
                    key={d.d_file_id} 
                    onContextMenu={(e) => handleContextMenu(e, d)}
                    className={cn(
                      "transition-colors group h-8 cursor-default"
                    )}
                  >
                    <td className="text-theme-accent font-black font-mono tracking-tight" title={d.d_file}>
                      <div className="flex items-center space-x-2">
                        <FileDown size={12} className="text-theme-muted" />
                        <span className="truncate max-w-[200px]">{d.d_file}</span>
                      </div>
                    </td>
                    <td className="text-theme-primary font-mono text-[11px]">{d.d_computer || 'Unknown'}</td>
                    <td className="px-4">
                      <div className="flex items-center space-x-3">
                        <div className="flex-1 h-1 bg-theme-glass rounded-full overflow-hidden">
                          <div 
                            className={cn(
                              "h-full transition-all duration-500",
                              d.d_state === 2 ? "bg-theme-success shadow-glow-sm" : "bg-theme-accent shadow-glow-sm animate-pulse"
                            )}
                            style={{ width: `${progress}%` }}
                          />
                        </div>
                        <span className="text-[10px] font-mono font-bold text-theme-muted w-12 text-right">{progress.toFixed(1)}%</span>
                      </div>
                    </td>
                    <td className="text-theme-muted font-mono text-[11px]">{new Date(d.d_date * 1000).toLocaleString()}</td>
                    <td className="text-right">
                      <div className="flex items-center justify-end space-x-1 pr-2">
                        <span className={cn(
                          "px-2 py-0.5 rounded text-[9px] font-black uppercase tracking-widest",
                          d.d_state === 2 ? "bg-theme-success/10 text-theme-success border border-theme-success/20" :
                          d.d_state === 3 ? "bg-theme-danger/10 text-theme-danger border border-theme-danger/20" :
                          "bg-theme-accent/10 text-theme-accent border border-theme-accent/20"
                        )}>
                          {status}
                        </span>
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
      <div className="px-3 py-1.5 glass-card-sm border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.1em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2">
            <span className="opacity-60">ACTIVE_TRANSFERS:</span>
            <span className="text-theme-accent font-mono">{downloads.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-2">
          <span>ENCRYPTED_STREAM</span>
          <div className="w-1.5 h-1.5 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
        </div>
      </div>

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}

      {/* Download Portal */}
      <DownloadProgressDialog 
        isOpen={isDownloadOpen}
        onClose={() => setIsDownloadOpen(false)}
        url={downloadUrl}
        filename={downloadFilename}
      />
    </div>
  );
};

export default DownloadsList;
