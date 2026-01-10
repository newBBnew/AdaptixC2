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
import UploadFileDialog from './UploadFileDialog';
import FilePreviewDialog from './FilePreviewDialog';
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
  const [isUploadOpen, setIsUploadOpen] = useState(false);
  const [isPreviewOpen, setIsPreviewOpen] = useState(false);
  const [previewFile, setPreviewFile] = useState(null);
  const [previewTaskId, setPreviewTaskId] = useState(null);

  const data = browserData[agent.a_id] || { disks: [], files: [], currentPath: '' };

  useEffect(() => {
    if (data.currentPath && !currentPath) {
      setCurrentPath(data.currentPath);
    }
  }, [data.currentPath]);

  const handleRefresh = async () => {
    setLoading(true);
    try {
      await agentApi.listDir(agent.a_id, agent.a_name, currentPath || '.');
    } finally {
      setTimeout(() => setLoading(false), 1000);
    }
  };

  const handleGetDisks = async () => {
    setLoading(true);
    try {
      await agentApi.getDisks(agent.a_id, agent.a_name);
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
      agentApi.listDir(agent.a_id, agent.a_name, parentPath);
    }
  };

  const handleNavigate = (path) => {
    const cleanPath = path.replace(/[\\/]+$/, '') || (agent.a_os === 1 ? '' : '/');
    setCurrentPath(cleanPath);
    agentApi.listDir(agent.a_id, agent.a_name, cleanPath || '.');
  };

  const Breadcrumbs = () => {
    const separator = agent.a_os === 1 ? '\\' : '/';
    const parts = currentPath.split(separator).filter(Boolean);
    const crumbs = [];
    
    // Add Root/Drive
    if (agent.a_os === 1) {
      // For Windows, the first part is usually the drive (e.g., C:)
    } else {
      crumbs.push({ name: '/', path: '/' });
    }

    let accPath = '';
    parts.forEach((p, i) => {
      if (agent.a_os === 1 && i === 0 && p.includes(':')) {
        accPath = p + separator;
        crumbs.push({ name: p, path: accPath });
      } else {
        accPath += (accPath && !accPath.endsWith(separator) ? separator : '') + p;
        crumbs.push({ name: p, path: accPath });
      }
    });

    return (
      <div className="flex items-center flex-wrap gap-1 px-2 py-1 bg-theme-glass/30 rounded-lg border border-theme-glass-light mb-2">
        {crumbs.map((c, i) => (
          <React.Fragment key={i}>
            <button 
              onClick={() => handleNavigate(c.path)}
              className="px-1.5 py-0.5 rounded-md hover:bg-theme-accent/20 text-[10px] font-bold text-theme-muted hover:text-theme-accent transition-all"
            >
              {c.name}
            </button>
            {i < crumbs.length - 1 && <ChevronRight size={10} className="text-theme-muted opacity-40" />}
          </React.Fragment>
        ))}
      </div>
    );
  };

  const handleViewContent = async (file) => {
    const fullPath = file.b_fullpath || (currentPath + (agent.a_os === 1 ? '\\' : '/') + file.b_filename);
    const cmd = agent.a_os === 1 ? `type "${fullPath}"` : `cat "${fullPath}"`;
    
    try {
      const response = await agentApi.executeCommand({
        id: agent.a_id,
        name: agent.a_name,
        cmdline: cmd,
        data: { path: fullPath },
        ui: true
      });

      // We need to find the task ID from the command response or wait for it via WebSocket
      if (response.data && response.data.ok) {
        const taskId = response.data.task_id || response.data.id; // Try to get Task ID from response
        if (taskId) {
            setPreviewTaskId(taskId);
        } else {
            console.warn("No Task ID returned from executeCommand for file preview");
        }
        setPreviewFile(fullPath);
        setIsPreviewOpen(true);
      }
    } catch (err) {
      console.error('Failed to issue view command:', err);
    }
  };

  const handleContextMenu = (e, file) => {
    e.preventDefault();
    const fullPath = file.b_fullpath || (currentPath + (agent.a_os === 1 ? '\\' : '/') + file.b_filename);
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Open', icon: FolderOpen, disabled: !file.b_is_dir, onClick: () => handleNavigate(fullPath) },
        { label: 'View content', icon: FileText, disabled: file.b_is_dir, onClick: () => handleViewContent(file) },
        { label: 'Download', icon: Download, disabled: file.b_is_dir, onClick: () => agentApi.downloadFile(agent.a_id, agent.a_name, fullPath) },
        { divider: true },
        { label: 'Copy path', icon: Copy, onClick: () => navigator.clipboard.writeText(fullPath) },
        { label: 'Copy to...', icon: Copy, onClick: () => {
          const newPath = window.prompt('Destination path:', fullPath);
          if (newPath && newPath !== fullPath) {
            agentApi.copyFile(agent.a_id, agent.a_name, fullPath, newPath);
            setTimeout(handleRefresh, 1000);
          }
        }},
        { label: 'Rename', icon: Edit3, onClick: () => {
          const newName = window.prompt('New name:', file.b_filename);
          if (newName && newName !== file.b_filename) {
            const newPath = currentPath + (agent.a_os === 1 ? '\\' : '/') + newName;
            agentApi.moveFile(agent.a_id, agent.a_name, fullPath, newPath);
          }
        }},
        { divider: true },
        { label: 'Delete', icon: Trash2, color: 'text-theme-danger', onClick: () => {
          if (window.confirm(`Delete ${file.b_filename}?`)) {
            agentApi.deleteFile(agent.a_id, agent.a_name, fullPath);
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

  const handleBackgroundContextMenu = (e) => {
    e.preventDefault();
    if (e.target.closest('tr')) return; // Ignore if clicked on a file row
    
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { label: 'Refresh', icon: RefreshCw, onClick: handleRefresh },
        { label: 'New Folder', icon: Folder, onClick: async () => {
          const name = window.prompt('Enter folder name:');
          if (name) {
             const separator = agent.a_os === 1 ? '\\' : '/';
             const path = currentPath + (currentPath.endsWith(separator) ? '' : separator) + name;
             try {
               await agentApi.makeDirectory(agent.a_id, agent.a_name, path);
               setTimeout(handleRefresh, 1000);
             } catch(err) {
               console.error("Failed to create directory", err);
             }
          }
        }},
        { divider: true },
        { label: 'Upload', icon: Upload, onClick: () => setIsUploadOpen(true) },
      ]
    });
  };

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
          <button 
            onClick={() => setIsUploadOpen(true)}
            className="glass-btn-primary px-4 py-2 flex items-center space-x-2 shadow-glow-sm"
          >
            <Upload size={14} />
            <span className="font-black uppercase tracking-widest text-[10px]">Upload</span>
          </button>
        </div>
      </div>

      {/* 2. Search & Breadcrumbs Panel */}
      <div className="px-4 py-3 glass-card-sm border-b border-theme-glass-light flex flex-col space-y-3 shrink-0">
        <Breadcrumbs />
        <div className="flex items-center space-x-4">
          <div className="relative flex-1 max-w-sm">
            <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
            <input 
              type="text" 
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Filter current view..." 
              className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
            />
          </div>
          <div className="text-[9px] font-black text-theme-muted uppercase tracking-[0.2em]">
            TOTAL_ARTIFACTS: <span className="text-theme-accent font-mono font-bold">{filteredFiles.length}</span>
          </div>
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
        <div 
          className="flex-1 overflow-auto custom-scrollbar bg-theme-glass-panel"
          onContextMenu={handleBackgroundContextMenu}
        >
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
                <th className="w-16"></th>
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
                  <td className="text-center">
                    {!file.b_is_dir && (
                      <button 
                        onClick={(e) => {
                          e.stopPropagation();
                          const fullPath = file.b_fullpath || (currentPath + (agent.a_os === 1 ? '\\' : '/') + file.b_filename);
                          agentApi.downloadFile(agent.a_id, agent.a_name, fullPath);
                        }}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-accent transition-colors opacity-0 group-hover:opacity-100"
                        title="Task Download"
                      >
                        <Download size={14} />
                      </button>
                    )}
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

      <UploadFileDialog 
        isOpen={isUploadOpen} 
        onClose={() => setIsUploadOpen(false)} 
        onUploaded={async (fileData) => {
          if (fileData && fileData.path) {
            // fileData.path is the path on the Teamserver
            // We need to upload it to the Agent's current path
            // Extract filename from the server path or use the original name if available
            // Assuming fileData.path is the full server path. 
            // The upload command expects: upload <local_server_path> <remote_agent_path>
            
            // We need the filename. UploadFileDialog doesn't return the original filename in the data object usually, 
            // but we can try to extract it or assume it's the last part of the path.
            // Let's rely on the server response structure. 
            // If API returns 'name' use it, otherwise basename of path.
            const fileName = fileData.name || fileData.path.split(/[/\\]/).pop();
            const remotePath = currentPath + (agent.a_os === 1 ? '\\' : '/') + fileName;
            
            setLoading(true);
            try {
              await agentApi.uploadFile(agent.a_id, agent.a_name, fileData.path, remotePath);
              // Give it some time to process
              setTimeout(() => handleRefresh(), 2000);
            } catch (err) {
              console.error("Failed to trigger agent upload:", err);
              setLoading(false);
            }
          } else {
            handleRefresh();
          }
        }}
      />

      <FilePreviewDialog
        isOpen={isPreviewOpen}
        onClose={() => setIsPreviewOpen(false)}
        agent={agent}
        filePath={previewFile}
        taskId={previewTaskId}
      />
    </div>
  );
};

export default FileBrowser;
