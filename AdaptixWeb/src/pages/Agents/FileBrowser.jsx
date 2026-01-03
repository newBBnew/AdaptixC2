import React, { useState, useEffect, useMemo } from 'react';
import { 
  Folder, 
  File, 
  HardDrive, 
  ChevronRight, 
  ChevronDown, 
  ArrowUp, 
  RefreshCw, 
  Upload, 
  MoreVertical,
  Search,
  Clock,
  User,
  Shield,
  FileText,
  Download,
  Trash2,
  Copy,
  FolderOpen,
  Edit3
} from 'lucide-react';
import ContextMenu from '../../components/ContextMenu';
import { useAgents } from '../../context/AgentContext';
import { agentApi } from '../../api/agent';
import { cn } from '../../utils/cn';

const FileBrowser = ({ agent }) => {
  const { browserData } = useAgents();
  const [currentPath, setCurrentPath] = useState('');
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedFolders, setExpandedFolders] = useState(new Set());
  const [menu, setMenu] = useState(null);

  const data = browserData[agent.a_id] || { disks: [], files: [], currentPath: '' };

  useEffect(() => {
    if (data.currentPath && !currentPath) {
      setCurrentPath(data.currentPath);
    }
  }, [data.currentPath]);

  const handleRefresh = async () => {
    setLoading(true);
    try {
      await agentApi.listDir(agent.a_id, currentPath || '.');
    } finally {
      setTimeout(() => setLoading(false), 1000);
    }
  };

  const handleGetDisks = async () => {
    setLoading(true);
    try {
      await agentApi.getDisks(agent.a_id);
    } finally {
      setTimeout(() => setLoading(false), 1000);
    }
  };

  const handleGoUp = () => {
    if (!currentPath) return;
    const separator = agent.a_os === 1 ? '\\' : '/'; // OS_WINDOWS = 1
    const parts = currentPath.split(separator);
    if (parts.length > 1) {
      parts.pop();
      const parentPath = parts.join(separator) || (agent.a_os === 1 ? '' : '/');
      setCurrentPath(parentPath);
      agentApi.listDir(agent.a_id, parentPath);
    }
  };

  const handleNavigate = (path) => {
    setCurrentPath(path);
    agentApi.listDir(agent.a_id, path);
  };

  const handleContextMenu = (e, file) => {
    e.preventDefault();
    const fullPath = file.b_fullpath || (currentPath + (agent.a_os === 1 ? '\\' : '/') + file.b_filename);
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Open', icon: FolderOpen, disabled: !file.b_is_dir, onClick: () => handleNavigate(fullPath) },
        { label: 'Download', icon: Download, disabled: file.b_is_dir, onClick: () => agentApi.downloadFile(agent.a_id, fullPath) },
        { divider: true },
        { label: 'Copy path', icon: Copy, onClick: () => navigator.clipboard.writeText(fullPath) },
        { label: 'Rename', icon: Edit3, onClick: () => {
          const newName = window.prompt('New name:', file.b_filename);
          if (newName && newName !== file.b_filename) {
            const newPath = currentPath + (agent.a_os === 1 ? '\\' : '/') + newName;
            agentApi.moveFile(agent.a_id, fullPath, newPath);
          }
        }},
        { divider: true },
        { label: 'Delete', icon: Trash2, color: 'text-theme-danger', onClick: () => {
          if (window.confirm(`Delete ${file.b_filename}?`)) {
            agentApi.deleteFile(agent.a_id, fullPath);
          }
        }},
      ]
    });
  };

  const filteredFiles = useMemo(() => {
    if (!searchQuery) return data.files;
    return data.files.filter(f => 
      f.b_filename.toLowerCase().includes(searchQuery.toLowerCase())
    );
  }, [data.files, searchQuery]);

  return (
    <div className="flex flex-col h-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Toolbar */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3 flex-1 max-w-2xl">
          <button 
            onClick={handleRefresh}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
          <button 
            onClick={handleGoUp}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Go Up"
          >
            <ArrowUp size={16} />
          </button>
          
          <div className="flex-1 flex items-center glass-input rounded-xl px-4 py-1.5">
            <Folder size={14} className="text-theme-muted mr-2 shrink-0" />
            <input 
              type="text"
              value={currentPath}
              onChange={(e) => setCurrentPath(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleNavigate(currentPath)}
              className="w-full bg-transparent outline-none text-sm font-mono text-theme-primary placeholder:text-theme-muted"
              placeholder="Enter path..."
            />
          </div>
          
          <button 
            onClick={() => handleNavigate(currentPath)}
            className="p-2 glass-btn-primary shadow-glow-sm"
            title="Navigate"
          >
            <ChevronRight size={16} className="text-theme-primary" />
          </button>
        </div>

        <div className="flex items-center space-x-3 ml-4">
          <button 
            onClick={handleGetDisks}
            className="glass-btn px-4 py-2 text-theme-muted hover:text-theme-primary flex items-center space-x-2"
          >
            <HardDrive size={14} />
            <span className="font-black uppercase tracking-widest text-[10px]">Disks</span>
          </button>
          <button className="glass-btn-primary px-4 py-2 flex items-center space-x-2 shadow-glow-sm">
            <Upload size={14} />
            <span className="font-black uppercase tracking-widest text-[10px]">Upload</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      <div className="px-4 py-2 glass-card-sm border-b border-theme-glass-light flex items-center space-x-4 shrink-0">
        <div className="relative flex-1 max-w-sm">
          <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
          <input 
            type="text" 
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Filter files..." 
            className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
          />
        </div>
        <div className="text-[9px] font-black text-theme-muted uppercase tracking-[0.2em]">
          TELEMETRY_COUNT: <span className="text-theme-accent font-mono font-bold">{filteredFiles.length}</span>
        </div>
      </div>

      {/* 3. Main Splitter View */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left: Tree View (Directory Tree) */}
        <div className="w-52 border-r border-theme-glass-light bg-theme-glass-panel overflow-y-auto shrink-0 shadow-glow-sm">
          <div className="p-2 space-y-1">
            <div className="flex items-center space-x-2 px-3 py-2 text-[10px] font-black uppercase text-theme-muted tracking-[0.2em] mb-1 border-b border-theme-glass-light bg-theme-glass">
              <HardDrive size={12} className="text-theme-accent" />
              <span>Logical Drives</span>
            </div>
            {data.disks.map(disk => (
              <div 
                key={disk.b_name}
                onClick={() => handleNavigate(disk.b_name)}
                className="flex items-center space-x-3 px-3 py-2 rounded-xl hover:bg-theme-glass cursor-default transition-all group border border-transparent hover:border-theme-accent/20"
              >
                <div className="p-1.5 bg-theme-glass-panel border border-theme-glass-light rounded-lg shadow-sm group-hover:bg-theme-glass group-hover:border-theme-accent/30 transition-all">
                  <HardDrive size={14} className="text-theme-accent opacity-80" />
                </div>
                <div className="flex flex-col min-w-0">
                  <span className="text-[11px] font-black font-mono text-theme-primary group-hover:text-theme-accent transition-colors">{disk.b_name}</span>
                  <span className="text-[8px] text-theme-muted font-bold uppercase tracking-tighter">{disk.b_type || 'UNKNOWN_MEDIA'}</span>
                </div>
              </div>
            ))}
            {data.disks.length === 0 && (
              <div className="flex flex-col items-center justify-center py-12 space-y-3 opacity-20">
                <HardDrive size={32} className="text-theme-muted" />
                <p className="text-[9px] font-black uppercase tracking-widest text-center text-theme-muted">Awaiting Enumeration</p>
              </div>
            )}
          </div>
        </div>

        {/* Right: File Table */}
        <div className="flex-1 overflow-auto custom-scrollbar bg-theme-glass-panel">
          <table className="glass-table min-w-[700px]">
            <thead>
              <tr>
                <th className="w-1/2">Artifact Name</th>
                <th className="text-right w-24">Size</th>
                {agent.a_os !== 1 && (
                  <>
                    <th className="w-24 text-center">Perms</th>
                    <th className="w-32 text-center">Owner UID/GID</th>
                  </>
                )}
                <th className="text-right w-40">Modified Timestamp</th>
              </tr>
            </thead>
            <tbody className="text-[11px] font-medium">
              {filteredFiles.map((file, idx) => (
                <tr 
                  key={idx}
                  onDoubleClick={() => file.b_is_dir && handleNavigate(file.b_fullpath || (currentPath + (agent.a_os === 1 ? '\\' : '/') + file.b_filename))}
                  onContextMenu={(e) => handleContextMenu(e, file)}
                  className="transition-colors group h-10 cursor-default border-b border-theme-glass-light hover:bg-theme-glass"
                >
                  <td className="flex items-center space-x-3">
                    <div className="shrink-0">
                      {file.b_is_dir ? (
                        <Folder size={16} className="text-theme-accent fill-theme-accent/10" />
                      ) : (
                        <FileText size={16} className="text-theme-muted group-hover:text-theme-primary transition-colors" />
                      )}
                    </div>
                    <span className={cn(file.b_is_dir ? "text-theme-primary font-black uppercase tracking-tight" : "text-theme-secondary", "truncate font-mono text-[11px] group-hover:text-theme-primary transition-colors")}>
                      {file.b_filename}
                    </span>
                  </td>
                  <td className="text-right font-mono text-theme-muted text-[10px]">
                    {file.b_is_dir ? (
                      <span className="text-theme-muted italic text-[9px] font-black uppercase opacity-40">DIR_STRUCT</span>
                    ) : (
                      file.b_size ? `${(file.b_size / 1024).toFixed(1)} KB` : '0 B'
                    )}
                  </td>
                  {agent.a_os !== 1 && (
                    <>
                      <td className="text-center font-mono text-theme-muted text-[10px]">{file.b_mode || '---'}</td>
                      <td className="text-center truncate text-theme-muted text-[10px]">
                        <div className="flex items-center justify-center space-x-1.5 bg-theme-glass-panel border border-theme-glass-light rounded-lg px-2 py-1 mx-2">
                          <User size={10} className="text-theme-accent opacity-60" />
                          <span className="font-mono text-[10px]">{file.b_user || '0'}</span>
                        </div>
                      </td>
                    </>
                  )}
                  <td className="text-right text-theme-muted font-mono text-[10px]">
                    {file.b_date ? new Date(file.b_date * 1000).toLocaleString([], { hour12: false }) : 'N/A'}
                  </td>
                </tr>
              ))}
              {filteredFiles.length === 0 && (
                <tr>
                  <td colSpan={agent.a_os === 1 ? 3 : 5} className="py-24 text-center border-none">
                    <div className="flex flex-col items-center space-y-4 opacity-20">
                      <FolderOpen size={48} className="text-theme-muted" />
                      <p className="text-[10px] font-black uppercase tracking-[0.2em] text-theme-muted">Directory Context Empty</p>
                    </div>
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* 4. Footer Status */}
      <div className="px-3 py-1.5 bg-theme-glass border-t border-theme-glass-light flex items-center justify-between text-[9px] font-black uppercase text-theme-muted tracking-[0.1em] shrink-0">
        <div className="flex items-center space-x-6">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1 border border-theme-glass-light rounded-lg shadow-glow-sm">
            <span className="text-theme-muted opacity-60">OS_TARGET:</span>
            <span className={cn("font-mono font-black", agent.a_os === 1 ? "text-theme-accent" : "text-theme-accent-secondary")}>{agent.a_os === 1 ? 'WINDOWS_NT' : 'POSIX_UNIX'}</span>
          </div>
          <div className="w-px h-4 bg-theme-glass-light" />
          <div className="flex items-center space-x-2">
            <span className="text-theme-muted opacity-60">WORKSPACE:</span>
            <span className="text-theme-secondary font-mono italic normal-case truncate max-w-lg">{currentPath || '/'}</span>
          </div>
        </div>
        <div className="flex items-center space-x-3">
          <span className="text-theme-muted">REMOTE_IO_READY</span>
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

export default FileBrowser;
