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
        { label: 'Delete', icon: Trash2, color: 'text-accent-danger', onClick: () => {
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
    <div className="flex flex-col h-full bg-[#0a0a0a] text-gray-300 font-sans select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Toolbar (Aligned with BrowserFilesWidget.cpp) */}
      <div className="flex items-center justify-between px-4 py-2 bg-dark-800 border-b border-dark-700 shrink-0">
        <div className="flex items-center space-x-2 flex-1 max-w-2xl">
          <button 
            onClick={handleRefresh}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Reload current directory"
          >
            <RefreshCw className={cn("w-3.5 h-3.5", loading && "animate-spin text-accent-primary")} />
          </button>
          <button 
            onClick={handleGoUp}
            className="p-1.5 rounded hover:bg-dark-700 text-gray-400 hover:text-white transition-all"
            title="Up one level"
          >
            <ArrowUp size={14} />
          </button>
          
          <div className="flex-1 flex items-center bg-dark-950/50 border border-dark-600 rounded px-2 py-1">
            <Folder size={12} className="text-gray-600 mr-2 shrink-0" />
            <input 
              type="text"
              value={currentPath}
              onChange={(e) => setCurrentPath(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleNavigate(currentPath)}
              className="w-full bg-transparent outline-none text-[11px] font-mono text-gray-300"
              placeholder="C:\Windows\System32"
            />
          </div>
          
          <button 
            onClick={() => handleNavigate(currentPath)}
            className="p-1.5 rounded bg-accent-primary/10 border border-accent-primary/20 text-accent-primary hover:bg-accent-primary/20 transition-all"
          >
            <ChevronRight size={14} />
          </button>
        </div>

        <div className="flex items-center space-x-2 ml-4">
          <button 
            onClick={handleGetDisks}
            className="flex items-center space-x-1.5 px-3 py-1 rounded bg-dark-700 border border-dark-600 text-gray-300 hover:text-white transition-all"
          >
            <HardDrive size={14} />
            <span className="text-[10px] font-bold uppercase">Disks</span>
          </button>
          <button className="flex items-center space-x-1.5 px-3 py-1 rounded bg-accent-primary/10 border border-accent-primary/30 text-accent-primary hover:bg-accent-primary/20 transition-all">
            <Upload size={14} />
            <span className="text-[10px] font-bold uppercase">Upload</span>
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      <div className="px-4 py-1.5 bg-dark-800/40 border-b border-dark-700 flex items-center space-x-4">
        <div className="relative flex-1 max-w-sm">
          <Search className="w-3 h-3 absolute left-2.5 top-1/2 -translate-y-1/2 text-gray-600" />
          <input 
            type="text" 
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="filter current view..." 
            className="w-full bg-dark-950/30 border border-dark-700 rounded px-8 py-1 text-[10px] text-gray-400 outline-none focus:border-accent-primary/30 placeholder:text-gray-700"
          />
        </div>
        <div className="text-[9px] font-mono text-gray-600 uppercase tracking-tighter">
          {filteredFiles.length} items found
        </div>
      </div>

      {/* 3. Main Splitter View */}
      <div className="flex-1 flex overflow-hidden">
        {/* Left: Tree View (Directory Tree) */}
        <div className="w-64 border-r border-dark-700 bg-dark-900/20 overflow-y-auto custom-scrollbar shrink-0">
          <div className="p-2 space-y-1">
            <div className="flex items-center space-x-2 px-2 py-1 text-[10px] font-black uppercase text-gray-500 tracking-widest mb-2">
              <HardDrive size={12} />
              <span>Drives & Roots</span>
            </div>
            {data.disks.map(disk => (
              <div 
                key={disk.b_name}
                onClick={() => handleNavigate(disk.b_name)}
                className="flex items-center space-x-2 px-3 py-1.5 rounded hover:bg-dark-700/50 cursor-pointer transition-colors group"
              >
                <HardDrive size={14} className="text-accent-primary opacity-60 group-hover:opacity-100" />
                <span className="text-[11px] font-mono">{disk.b_name}</span>
                <span className="text-[9px] text-gray-600 italic">({disk.b_type})</span>
              </div>
            ))}
            {data.disks.length === 0 && (
              <p className="text-[10px] text-gray-600 italic px-3 py-4 text-center">Click Disks to scan</p>
            )}
          </div>
        </div>

        {/* Right: File Table */}
        <div className="flex-1 overflow-auto scrollbar-thin">
          <table className="w-full text-left border-collapse table-fixed">
            <thead className="sticky top-0 bg-dark-800 z-10 shadow-sm">
              <tr className="border-b border-dark-700 text-gray-500 text-[10px] font-bold uppercase tracking-tight">
                <th className="py-2 px-4 w-1/2 border-r border-dark-700/30">Name</th>
                <th className="py-2 px-4 border-r border-dark-700/30 text-right w-24">Size</th>
                {agent.a_os !== 1 && (
                  <>
                    <th className="py-2 px-4 border-r border-dark-700/30 w-24 text-center">Mode</th>
                    <th className="py-2 px-4 border-r border-dark-700/30 w-24 text-center">Owner</th>
                  </>
                )}
                <th className="py-2 px-4 text-right">Modified</th>
              </tr>
            </thead>
            <tbody className="text-[11px] font-medium divide-y divide-dark-800/30">
              {filteredFiles.map((file, idx) => (
                <tr 
                  key={idx}
                  onDoubleClick={() => file.b_is_dir && handleNavigate(file.b_fullpath || (currentPath + (agent.a_os === 1 ? '\\' : '/') + file.b_filename))}
                  onContextMenu={(e) => handleContextMenu(e, file)}
                  className="hover:bg-accent-primary/5 group h-8 cursor-default transition-colors"
                >
                  <td className="px-4 flex items-center space-x-3 truncate">
                    {file.b_is_dir ? (
                      <Folder size={14} className="text-accent-warning fill-accent-warning/10 shrink-0" />
                    ) : (
                      <FileText size={14} className="text-gray-500 shrink-0" />
                    )}
                    <span className={cn(file.b_is_dir ? "text-gray-200" : "text-gray-400", "truncate font-mono")}>
                      {file.b_filename}
                    </span>
                  </td>
                  <td className="px-4 text-right font-mono text-gray-500 text-[10px]">
                    {file.b_is_dir ? 'DIR' : (file.b_size ? `${(file.b_size / 1024).toFixed(1)} KB` : '---')}
                  </td>
                  {agent.a_os !== 1 && (
                    <>
                      <td className="px-4 text-center font-mono text-gray-600 text-[10px]">{file.b_mode || '---'}</td>
                      <td className="px-4 text-center truncate text-gray-600 text-[10px]">
                        <div className="flex items-center justify-center space-x-1">
                          <User size={10} />
                          <span>{file.b_user || '---'}</span>
                        </div>
                      </td>
                    </>
                  )}
                  <td className="px-4 text-right text-gray-600 font-mono text-[10px]">
                    {file.b_date ? new Date(file.b_date * 1000).toLocaleDateString() : '---'}
                  </td>
                </tr>
              ))}
              {filteredFiles.length === 0 && (
                <tr>
                  <td colSpan={agent.a_os === 1 ? 3 : 5} className="py-20 text-center opacity-20">
                    <div className="flex flex-col items-center">
                      <Folder size={48} className="text-gray-600" />
                      <p className="mt-2 uppercase tracking-widest text-[10px]">Empty or unvisited directory</p>
                    </div>
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      {/* 4. Footer Status (Mimics statusLabel in Qt) */}
      <div className="px-4 py-1 bg-dark-800 border-t border-dark-700 flex items-center justify-between text-[9px] font-black uppercase text-gray-500 tracking-tighter">
        <div className="flex items-center space-x-4">
          <div className="flex items-center space-x-1">
            <harddrive size={10} />
            <span>Target OS: <span className="text-gray-300">{agent.a_os === 1 ? 'Windows' : 'Unix'}</span></span>
          </div>
          <div className="w-px h-2.5 bg-dark-600" />
          <span>Location: <span className="text-accent-primary lowercase">{currentPath || '/'}</span></span>
        </div>
        <div className="flex items-center space-x-2">
          <Shield size={10} className="text-accent-secondary" />
          <span className="text-accent-secondary/80 tracking-widest">Teamserver Data Synced</span>
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
