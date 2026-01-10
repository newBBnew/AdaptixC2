import React, { useState, useEffect } from 'react';
import { 
  Database, 
  Search, 
  Filter, 
  Plus, 
  Trash2, 
  Link, 
  Copy,
  RefreshCw,
  X,
  FileUp,
  Download
} from 'lucide-react';
import { deliveryApi } from '../../api/control';
import { cn } from '../../utils/cn';

import CreateLinkDialog from './CreateLinkDialog';
import UploadFileDialog from './UploadFileDialog';
import { useAgents } from '../../context/AgentContext';
import ContextMenu from '../../components/ContextMenu';

const FileDeliveryList = () => {
  const { fileDeliveries, fetchAgents } = useAgents();
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [isSearchVisible, setIsSearchVisible] = useState(false);
  
  // Dialog states
  const [isLinkOpen, setIsLinkOpen] = useState(false);
  const [isUploadOpen, setIsUploadOpen] = useState(false);
  const [menu, setMenu] = useState(null);
  const [selectedFile, setSelectedFile] = useState(null);

  // Convert object to array for listing
  const filesArray = Object.values(fileDeliveries);

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

  const handleCreateLink = (file) => {
    setSelectedFile(file);
    setIsLinkOpen(true);
  };

  const filteredFiles = filesArray.filter(f => 
    Object.values(f).some(val => 
      String(val).toLowerCase().includes(searchQuery.toLowerCase())
    )
  );

  const handleDeleteFile = async (id) => {
    if (!window.confirm('Are you sure you want to delete this hosted file?')) return;
    try {
      await deliveryApi.stop(id);
    } catch (err) {
      console.error('Failed to delete file:', err);
    }
  };

  const handleCopyUrl = (url) => {
    navigator.clipboard.writeText(url);
  };

  const handleContextMenu = (e, file) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Copy download URL', icon: Copy, onClick: () => handleCopyUrl(file.fd_url) },
        { label: 'Create download link', icon: Link, onClick: () => handleCreateLink(file) },
        { divider: true },
        { label: 'Delete hosted file', icon: Trash2, color: 'text-theme-danger', onClick: () => handleDeleteFile(file.fd_file_id) },
      ]
    });
  };

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header with Controls */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-glass-border shrink-0">
        <div className="flex items-center space-x-3">
          <button 
            onClick={() => setIsSearchVisible(!isSearchVisible)}
            className={cn(
              "p-2 rounded-xl transition-all",
              isSearchVisible ? "bg-theme-accent/20 text-theme-accent border border-theme-accent/30" : "text-theme-muted hover:text-theme-primary hover:bg-theme-hover"
            )}
            title="Toggle Search (Ctrl+F)"
          >
            <Search className="w-4 h-4 text-theme-accent" />
          </button>
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          <button 
            onClick={() => {
              setLoading(true);
              fetchAgents().finally(() => setLoading(false));
            }}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh File Delivery"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>

        <div className="flex items-center">
          <button 
            onClick={() => setIsUploadOpen(true)}
            className="glass-btn-primary px-4 py-2 text-theme-primary flex items-center space-x-2 shadow-glow-sm"
          >
            <FileUp className="w-4 h-4" />
            <span className="font-semibold text-sm">Host New File</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      {isSearchVisible && (
        <div className="flex items-center px-4 py-2 glass-card-sm border-b border-theme-glass-light shrink-0 animate-in slide-in-from-top-1 duration-200">
          <div className="relative flex-1 max-w-md">
            <Filter className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              autoFocus
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search filename, url, size..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
        </div>
      )}

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-[900px]">
          <thead>
            <tr>
              <th className="w-64">Hosted File</th>
              <th className="w-32">Size</th>
              <th className="w-24">Hits</th>
              <th className="w-64">Download URL</th>
              <th className="w-48">Created</th>
              <th className="text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[12px] font-medium">
            {filteredFiles.length === 0 ? (
              <tr>
                <td colSpan="6" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Database size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">No telemetry artifacts hosted</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredFiles.map((f) => (
                <tr 
                  key={f.fd_file_id} 
                  onContextMenu={(e) => handleContextMenu(e, f)}
                  className="transition-colors group h-10 cursor-default border-b border-theme-glass-light hover:bg-theme-glass"
                >
                  <td className="text-theme-accent font-black font-mono tracking-tight">{f.fd_name}</td>
                  <td className="text-theme-secondary font-mono italic">{(f.fd_size / 1024).toFixed(2)} KB</td>
                  <td>
                    <span className="px-1.5 py-0.5 rounded-sm bg-theme-glass-panel text-[9px] font-black text-theme-accent-secondary border border-theme-glass-light">
                      {f.fd_downloads || 0}
                    </span>
                  </td>
                  <td className="text-theme-primary font-mono select-text hover:text-theme-accent transition-colors cursor-pointer text-[10px]" onClick={() => handleCopyUrl(f.fd_url)}>
                    {f.fd_url || 'PENDING_MAPPING'}
                  </td>
                  <td className="text-theme-muted font-mono text-[10px]">
                    {f.fd_date ? new Date(f.fd_date * 1000).toLocaleString([], { hour12: false }) : 'N/A'}
                  </td>
                  <td className="text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity">
                      <button 
                        onClick={() => handleCopyUrl(f.fd_url)}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-primary transition-colors" 
                        title="Copy Mapping URL"
                      >
                        <Copy size={14} />
                      </button>
                      <button 
                        onClick={() => handleCreateLink(f)}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-accent-secondary transition-colors" 
                        title="Generate Link"
                      >
                        <Link size={14} />
                      </button>
                      <button 
                        onClick={() => handleDeleteFile(f.fd_file_id)}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-danger/70 hover:text-theme-danger transition-all" 
                        title="Purge Artifact"
                      >
                        <Trash2 size={14} />
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
      <div className="px-3 py-1.5 glass-card-sm border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black text-theme-muted uppercase tracking-[0.15em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 rounded-lg border border-theme-glass-light shadow-glow-sm">
            <span className="opacity-60">HOSTED_ARTIFACTS:</span>
            <span className="text-theme-accent font-mono font-bold">{filesArray.length}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3 pr-1">
          <span className="text-theme-muted opacity-80 uppercase tracking-[0.2em]">Delivery_Service_Online</span>
          <div className="w-2 h-2 rounded-full bg-theme-success shadow-glow-sm animate-pulse" />
        </div>
      </div>

      {/* Dialogs */}
      <CreateLinkDialog 
        isOpen={isLinkOpen}
        onClose={() => setIsLinkOpen(false)}
        fileId={selectedFile?.fd_file_id}
        filename={selectedFile?.fd_name}
      />

      <UploadFileDialog
        isOpen={isUploadOpen}
        onClose={() => setIsUploadOpen(false)}
        onUploaded={() => {}}
      />

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

export default FileDeliveryList;
