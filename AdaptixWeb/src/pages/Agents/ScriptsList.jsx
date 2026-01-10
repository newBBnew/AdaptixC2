import React, { useState, useEffect } from 'react';
import { 
  Code2, 
  Search, 
  RefreshCw, 
  FileCode, 
  FolderOpen, 
  ChevronRight, 
  Play, 
  Trash2, 
  FileUp, 
  FileText,
  X,
  Plus,
  Save,
  RotateCcw
} from 'lucide-react';
import { scriptApi } from '../../api/control';
import { cn } from '../../utils/cn';
import { useAgents } from '../../context/AgentContext';
import ContextMenu from '../../components/ContextMenu';

const ScriptsList = () => {
  const { fetchAgents, reloadScripts } = useAgents();
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [currentPath, setCurrentPath] = useState('');
  const [scripts, setScripts] = useState([]);
  const [basePath, setBasePath] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [menu, setMenu] = useState(null);
  const [selectedScript, setSelectedScript] = useState(null);
  const [isEditorOpen, setIsEditorOpen] = useState(false);
  const [editorContent, setEditorContent] = useState('');
  const [hasChanges, setHasChanges] = useState(false);

  const fetchScripts = async (path = '') => {
    setLoading(true);
    try {
      const response = await scriptApi.list(path);
      if (response.data?.ok) {
        setScripts(response.data.scripts || []);
        setBasePath(response.data.base || '');
        setCurrentPath(path);
      }
    } catch (err) {
      console.error('Failed to fetch scripts:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchScripts();
  }, []);

  const handleNavigate = (path) => {
    fetchScripts(path);
  };

  const handleGoUp = () => {
    const parts = currentPath.split(/[\\/]/).filter(Boolean);
    parts.pop();
    handleNavigate(parts.join('/'));
  };

  const handleReadScript = async (script) => {
    try {
      const response = await scriptApi.read(script.path);
      if (response.data?.ok) {
        setEditorContent(response.data.content);
        setSelectedScript(script);
        setHasChanges(false);
        setIsEditorOpen(true);
      }
    } catch (err) {
      console.error('Failed to read script:', err);
      alert('Error reading script content');
    }
  };

  const handleSaveScript = async () => {
    if (!selectedScript) return;
    setSaving(true);
    try {
      const response = await scriptApi.write(selectedScript.path, editorContent);
      if (response.data?.ok) {
        setHasChanges(false);
        // Show success indicator (could be a toast)
        console.log('Script saved successfully');
      } else {
        alert(response.data?.message || 'Failed to save script');
      }
    } catch (err) {
      console.error('Failed to save script:', err);
      alert('Error saving script to server');
    } finally {
      setSaving(false);
    }
  };

  const handleReloadExtensions = async () => {
    try {
      await reloadScripts();
      alert('Extensions reloaded successfully on Gateway');
    } catch (err) {
      console.error('Reload failed:', err);
      alert('Failed to reload extensions');
    }
  };

  const handleContextMenu = (e, script) => {
    e.preventDefault();
    setMenu({
      x: e.clientX,
      y: e.clientY,
      options: [
        { 
          label: script.is_dir ? 'Open folder' : 'View content', 
          icon: script.is_dir ? FolderOpen : FileText, 
          onClick: () => script.is_dir ? handleNavigate(script.path) : handleReadScript(script) 
        },
        { divider: true },
        { 
          label: 'Execute as BOF', 
          icon: Play, 
          disabled: script.is_dir || !script.name.endsWith('.o'),
          onClick: () => {
            // BOF execution logic would typically go through a dialog or console
            alert('BOF execution requires an active Agent context. Use Agent Console or Plugin Dialog.');
          }
        },
        { divider: true },
        { label: 'Delete', icon: Trash2, color: 'text-theme-danger', onClick: () => alert('Delete not implemented for security') },
      ]
    });
  };

  const filteredScripts = scripts.filter(s => 
    s.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div className="flex flex-col h-full w-full select-none overflow-hidden" onClick={() => setMenu(null)}>
      {/* 1. Header */}
      <div className="flex items-center justify-between px-3 py-2 glass-card-sm border-b border-theme-glass-light shrink-0">
        <div className="flex items-center space-x-3">
          <div className="flex items-center space-x-2 bg-theme-glass-panel px-3 py-1.5 rounded-lg border border-theme-glass-light">
            <Code2 size={14} className="text-theme-accent" />
            <span className="text-[10px] font-black uppercase tracking-widest text-theme-muted">Extension Library</span>
          </div>
          
          <div className="h-5 w-px bg-theme-glass-light mx-1" />
          
          <div className="flex items-center space-x-1">
            <button 
              onClick={() => handleNavigate('')}
              className="p-1.5 glass-btn text-theme-muted hover:text-theme-primary transition-all"
              title="Base Directory"
            >
              <FolderOpen size={14} />
            </button>
            <ChevronRight size={12} className="text-theme-muted opacity-40" />
            <div className="px-2 py-1 glass-card-sm rounded-lg text-[10px] font-mono text-theme-primary">
              /{currentPath || 'root'}
            </div>
            {currentPath && (
              <button onClick={handleGoUp} className="ml-2 text-[10px] font-bold text-theme-accent hover:underline uppercase tracking-tighter">
                UP_DIR
              </button>
            )}
          </div>
        </div>

        <div className="flex items-center space-x-2">
          <button 
            onClick={() => fetchScripts(currentPath)}
            className="p-2 glass-btn text-theme-muted hover:text-theme-accent transition-all"
            title="Refresh"
          >
            <RefreshCw className={cn("w-4 h-4", loading && "animate-spin text-theme-accent")} />
          </button>
        </div>
      </div>

      {/* 2. Search Panel */}
      <div className="px-4 py-2 glass-card-sm border-b border-theme-glass-light flex items-center space-x-4 shrink-0">
        <div className="relative flex-1 max-w-md">
          <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-theme-muted" />
          <input 
            type="text" 
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Filter extensions, BOFs, scripts..." 
            className="glass-input w-full pl-10 py-2 text-sm text-theme-primary placeholder:text-theme-muted"
          />
        </div>
      </div>

      {/* 3. Table Area */}
      <div className="flex-1 overflow-auto custom-scrollbar glass-panel">
        <table className="glass-table min-w-full">
          <thead>
            <tr>
              <th className="w-12"></th>
              <th>Resource Name</th>
              <th className="w-32">Type</th>
              <th className="w-32">Size</th>
              <th className="text-right">Actions</th>
            </tr>
          </thead>
          <tbody className="text-[12px] font-medium">
            {filteredScripts.length === 0 ? (
              <tr>
                <td colSpan="5" className="py-24 text-center border-none">
                  <div className="flex flex-col items-center space-y-4 opacity-20">
                    <Code2 size={48} className="text-theme-muted" />
                    <p className="text-[10px] font-black tracking-[0.2em] uppercase text-theme-muted">No script artifacts found</p>
                  </div>
                </td>
              </tr>
            ) : (
              filteredScripts.map((s) => (
                <tr 
                  key={s.path} 
                  onContextMenu={(e) => handleContextMenu(e, s)}
                  onDoubleClick={() => s.is_dir ? handleNavigate(s.path) : handleReadScript(s)}
                  className="transition-colors group h-10 cursor-default border-b border-theme-glass-light hover:bg-theme-glass"
                >
                  <td className="text-center">
                    {s.is_dir ? <FolderOpen size={14} className="text-theme-accent inline" /> : <FileCode size={14} className="text-theme-secondary inline" />}
                  </td>
                  <td className={cn("font-mono", s.is_dir ? "text-theme-primary font-bold" : "text-theme-secondary")}>
                    {s.name}
                  </td>
                  <td>
                    <span className="text-[10px] uppercase font-black text-theme-muted opacity-60">
                      {s.is_dir ? 'DIRECTORY' : s.name.split('.').pop().toUpperCase()}
                    </span>
                  </td>
                  <td className="text-theme-muted font-mono text-[10px]">
                    {s.is_dir ? '---' : `${(s.size / 1024).toFixed(1)} KB`}
                  </td>
                  <td className="text-right">
                    <div className="flex items-center justify-end space-x-1 opacity-0 group-hover:opacity-100 transition-opacity pr-2">
                      {!s.is_dir && (
                        <button 
                          onClick={() => handleReadScript(s)}
                          className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-primary transition-colors" 
                          title="View Source"
                        >
                          <FileText size={14} />
                        </button>
                      )}
                      <button 
                        onClick={() => s.is_dir ? handleNavigate(s.path) : handleReadScript(s)}
                        className="p-1.5 rounded-lg hover:bg-theme-glass text-theme-muted hover:text-theme-accent transition-colors"
                      >
                        {s.is_dir ? <ChevronRight size={14} /> : <Play size={14} />}
                      </button>
                    </div>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* Editor Modal */}
      {isEditorOpen && (
        <div className="fixed inset-0 z-[300] bg-black/80 backdrop-blur-md flex flex-col p-8">
          <div className="flex items-center justify-between px-6 py-4 border-b border-theme-glass-light bg-theme-glass-panel rounded-t-2xl">
            <div className="flex items-center space-x-4">
              <div className="p-2 bg-theme-glass border border-theme-glass-light rounded-lg">
                <FileCode className="text-theme-accent" size={20} />
              </div>
              <div className="text-left">
                <p className="text-[10px] font-black uppercase tracking-widest text-theme-muted mb-0.5">
                  {hasChanges ? 'Script Editor (Modified)' : 'Script Editor'}
                </p>
                <p className="text-sm font-mono font-bold text-theme-primary">{selectedScript?.name}</p>
              </div>
            </div>
            
            <div className="flex items-center space-x-3">
              <button 
                onClick={handleReloadExtensions}
                className="flex items-center space-x-2 px-4 py-2 glass-btn text-[10px] font-black uppercase tracking-widest hover:text-theme-accent transition-all"
                title="Reload Gateway Extensions"
              >
                <RotateCcw size={14} />
                <span>Reload Env</span>
              </button>

              <button 
                onClick={handleSaveScript}
                disabled={!hasChanges || saving}
                className={cn(
                  "flex items-center space-x-2 px-6 py-2 rounded-xl text-[10px] font-black uppercase tracking-widest transition-all",
                  hasChanges 
                    ? "bg-theme-accent text-white shadow-glow-sm hover:shadow-glow" 
                    : "glass-btn text-theme-muted opacity-50 cursor-not-allowed"
                )}
              >
                {saving ? <RefreshCw size={14} className="animate-spin" /> : <Save size={14} />}
                <span>{saving ? 'Saving...' : 'Commit Changes'}</span>
              </button>

              <div className="w-px h-6 bg-theme-glass-light mx-2" />

              <button 
                onClick={() => {
                  if (hasChanges && !window.confirm('Discard unsaved changes?')) return;
                  setIsEditorOpen(false);
                }} 
                className="p-2 text-theme-muted hover:text-theme-danger hover:bg-theme-danger/10 rounded-xl transition-all"
              >
                <X size={24} />
              </button>
            </div>
          </div>
          <div className="flex-1 bg-black/40 border-x border-b border-theme-glass-light rounded-b-2xl overflow-hidden flex flex-col p-4 relative">
            <textarea 
              value={editorContent}
              onChange={(e) => {
                setEditorContent(e.target.value);
                setHasChanges(true);
              }}
              spellCheck={false}
              className="flex-1 bg-transparent text-theme-secondary font-mono text-[13px] outline-none resize-none custom-scrollbar leading-relaxed"
            />
            {hasChanges && (
              <div className="absolute bottom-6 right-8 pointer-events-none animate-pulse">
                <div className="bg-theme-accent/20 border border-theme-accent/40 px-3 py-1 rounded-full flex items-center space-x-2">
                  <div className="w-1.5 h-1.5 rounded-full bg-theme-accent shadow-glow-sm" />
                  <span className="text-[9px] font-black text-theme-accent uppercase tracking-tighter">Unsaved_Buffer</span>
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {/* Context Menu */}
      {menu && <ContextMenu {...menu} onClose={() => setMenu(null)} />}
    </div>
  );
};

export default ScriptsList;
